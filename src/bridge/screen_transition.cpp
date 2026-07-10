#include "bridge/screen_transition.hpp"

#include "bridge/screen_dispatch.hpp"
#include "bridge/sfx_trigger.hpp"

#include "backbone/animation_clock.hpp"
#include "backbone/screen_state.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <utility>

#ifdef __EMSCRIPTEN__
#include <imgui.h>
#include <imgui_internal.h>  // ImGuiContext / ImGuiWindow, to translate per-window draw lists
#endif

// Single-threaded: begun from input/render handlers and advanced from the main loop,
// all on the browser main thread. No synchronization needed.

namespace poker_trainer::bridge {

namespace {

// Ceremonial fade timings (ARCHITECTURE: ~750ms out, brief hold, ~750ms in, ~1.5s
// total). The hold keeps the screen fully black across the swap so it is never seen.
constexpr float kCeremonialFadeOutMs = 700.0f;
constexpr float kCeremonialHoldMs = 100.0f;
constexpr float kCeremonialFadeInMs = 700.0f;
constexpr float kCeremonialTotalMs =
    kCeremonialFadeOutMs + kCeremonialHoldMs + kCeremonialFadeInMs;

enum class Kind : std::uint8_t { None, Ceremonial, Slide };

struct TransitionState {
    Kind kind{Kind::None};
    std::uint64_t start_ms{0};

    // Ceremonial: run once at the fully-black midpoint.
    std::function<void()> on_swap;
    bool swapped{false};

    // Slide: run once at slide end.
    SlideDirection dir{SlideDirection::GameToPostRound};
    std::function<void()> on_complete;
};

TransitionState g_state{};
std::function<bool()> g_transitions_enabled{};

[[nodiscard]] bool transitions_enabled() {
    // Unwired (native tests / pre-boot) => OFF, so callers behave as instant cuts with
    // no render loop to advance the animation.
    return static_cast<bool>(g_transitions_enabled) && g_transitions_enabled();
}

[[nodiscard]] float elapsed_ms() noexcept {
    return static_cast<float>(backbone::total_ms_since_app_start() - g_state.start_ms);
}

#ifdef __EMSCRIPTEN__
// Slide timing (ARCHITECTURE: 350ms ease-out). Only the wasm render path advances it.
constexpr float kSlideMs = 350.0f;

// Black-overlay alpha across the ceremonial timeline: ramp up over the fade-out, hold at
// full black across the swap, ramp down over the fade-in.
[[nodiscard]] float ceremonial_alpha(float e) noexcept {
    if (e < kCeremonialFadeOutMs) {
        return std::clamp(e / kCeremonialFadeOutMs, 0.0f, 1.0f);
    }
    if (e < kCeremonialFadeOutMs + kCeremonialHoldMs) {
        return 1.0f;
    }
    const float fade_in_e = e - kCeremonialFadeOutMs - kCeremonialHoldMs;
    return std::clamp(1.0f - fade_in_e / kCeremonialFadeInMs, 0.0f, 1.0f);
}

// Cubic ease-out: fast then settling, per the 350ms slide spec.
[[nodiscard]] float ease_out_cubic(float t) noexcept {
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

// Render one screen into the background draw list translated horizontally by `dx`, by
// offsetting the vertices AND clip rects it appends this frame. Off-canvas content is not
// visible, so translating the (default full-viewport) clip too still yields correct
// screen-edge clipping while keeping any sub-clip (the stat-modal scroll body) aligned
// with its translated content. A trailing draw command with the un-translated current
// clip isolates subsequent (overlay) draws from the translated commands.
void render_screen_offset(backbone::ScreenId screen, float dx) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddDrawCmd();  // fresh command so this screen's draws are isolated
    const int vtx_begin = dl->VtxBuffer.Size;
    const int cmd_begin = dl->CmdBuffer.Size - 1;
    render_screen(screen);
    const int vtx_end = dl->VtxBuffer.Size;
    const int cmd_end = dl->CmdBuffer.Size;
    if (dx != 0.0f) {
        for (int i = vtx_begin; i < vtx_end; ++i) {
            dl->VtxBuffer[i].pos.x += dx;
        }
        for (int i = cmd_begin; i < cmd_end; ++i) {
            dl->CmdBuffer[i].ClipRect.x += dx;
            dl->CmdBuffer[i].ClipRect.z += dx;
        }
    }
    dl->AddDrawCmd();  // restore a clean trailing command (default clip) for overlays
}

// Translate every ImGui window rendered THIS frame by `dx`, so content in its own draw
// list (the Game screen's "##math_inputs" window — the only per-window draw list on any
// screen a slide touches) slides in lockstep with the background rather than popping. A
// window's draw list is finalized at End(); ImGui::Render() only references it (no vertex
// rebuild), so translating it here — after the screen renders, before the GL submit —
// reaches the renderer. Invariant that makes "all active windows" safe: a slide never has a
// modal open (submission / Again are not modal), the tutorial overlay draws to shared draw
// lists (not windows), and render_overlay runs AFTER this — so the math-input window is the
// only window active this frame. Both it and the background move by the same per-screen dx.
void translate_active_windows(float dx) {
    if (dx == 0.0f) {
        return;
    }
    const ImGuiContext& g = *ImGui::GetCurrentContext();
    for (ImGuiWindow* w : g.Windows) {
        if (w == nullptr || w->LastFrameActive != g.FrameCount) {
            continue;  // not rendered this frame
        }
        ImDrawList* dl = w->DrawList;
        if (dl == nullptr || dl->VtxBuffer.Size == 0) {
            continue;  // no geometry (skips ImGui's internal bookkeeping windows)
        }
        for (int i = 0; i < dl->VtxBuffer.Size; ++i) {
            dl->VtxBuffer[i].pos.x += dx;
        }
        for (int i = 0; i < dl->CmdBuffer.Size; ++i) {
            dl->CmdBuffer[i].ClipRect.x += dx;
            dl->CmdBuffer[i].ClipRect.z += dx;
        }
    }
}
#endif

}  // namespace

bool is_screen_transition_active() noexcept { return g_state.kind != Kind::None; }

void begin_ceremonial_transition(std::function<void()> on_swap) {
    if (g_state.kind != Kind::None) {
        return;  // one transition at a time
    }
    if (!transitions_enabled()) {
        if (on_swap) {
            on_swap();  // instant cut
        }
        return;
    }
    g_state = TransitionState{};
    g_state.kind = Kind::Ceremonial;
    g_state.start_ms = backbone::total_ms_since_app_start();
    g_state.on_swap = std::move(on_swap);
    g_state.swapped = false;
}

void begin_slide_transition(SlideDirection direction, audio::SfxId sfx,
                            std::function<void()> on_complete) {
    if (g_state.kind != Kind::None) {
        return;
    }
    if (!transitions_enabled()) {
        if (on_complete) {
            on_complete();  // instant cut; no slide SFX for a slide the user never saw
        }
        return;
    }
    g_state = TransitionState{};
    g_state.kind = Kind::Slide;
    g_state.start_ms = backbone::total_ms_since_app_start();
    g_state.dir = direction;
    g_state.on_complete = std::move(on_complete);
    play_sfx(sfx);  // fires as the content starts to slide
}

bool render_active_slide() {
#ifdef __EMSCRIPTEN__
    if (g_state.kind != Kind::Slide) {
        return false;
    }
    const float e = elapsed_ms();
    if (e >= kSlideMs) {
        // Finalize: run the teardown, clear state, and let the caller render the
        // now-current destination normally this frame (no dropped frame).
        std::function<void()> done = std::move(g_state.on_complete);
        g_state = TransitionState{};
        if (done) {
            done();
        }
        return false;
    }
    const float t = ease_out_cubic(std::clamp(e / kSlideMs, 0.0f, 1.0f));
    const float w = ImGui::GetMainViewport()->Size.x;

    backbone::ScreenId outgoing{};
    backbone::ScreenId incoming{};
    float out_dx = 0.0f;
    float in_dx = 0.0f;
    if (g_state.dir == SlideDirection::GameToPostRound) {
        outgoing = backbone::ScreenId::Game;      // exits left
        incoming = backbone::ScreenId::PostRound;  // enters from the right
        out_dx = -w * t;
        in_dx = w * (1.0f - t);
    } else {
        outgoing = backbone::ScreenId::PostRound;  // exits right
        incoming = backbone::ScreenId::Game;       // enters from the left
        out_dx = w * t;
        in_dx = -w * (1.0f - t);
    }
    render_screen_offset(outgoing, out_dx);
    render_screen_offset(incoming, in_dx);
    // The Game screen's math-input window is a separate draw list (render_screen_offset only
    // moves the background list), so slide it by the Game screen's offset too. Whichever of
    // the two screens is the Game screen owns that offset; the other (Post-Round) has no
    // window, so translating "windows active this frame" moves exactly the math window.
    const float game_dx = g_state.dir == SlideDirection::GameToPostRound ? out_dx : in_dx;
    translate_active_windows(game_dx);
    return true;
#else
    return false;
#endif
}

void render_ceremonial_overlay() {
    if (g_state.kind != Kind::Ceremonial) {
        return;
    }
    const float e = elapsed_ms();
    // Swap at the first fully-black frame (the fade-out has completed). The overlay is
    // opaque here, so the underlying screen change is invisible.
    if (!g_state.swapped && e >= kCeremonialFadeOutMs) {
        g_state.swapped = true;
        if (g_state.on_swap) {
            g_state.on_swap();
        }
    }
#ifdef __EMSCRIPTEN__
    const float alpha = ceremonial_alpha(e);
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::GetForegroundDrawList()->AddRectFilled(
        vp->Pos, ImVec2{vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y},
        ImGui::ColorConvertFloat4ToU32(ImVec4{0.0f, 0.0f, 0.0f, alpha}));
#endif
    if (e >= kCeremonialTotalMs) {
        g_state = TransitionState{};  // done
    }
}

void set_transitions_enabled_gate(std::function<bool()> enabled) {
    g_transitions_enabled = std::move(enabled);
}

void reset_screen_transition_for_testing() noexcept {
    g_state = TransitionState{};
    g_transitions_enabled = nullptr;
}

}  // namespace poker_trainer::bridge
