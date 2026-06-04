#include "bridge/platform.hpp"

#include "bridge/canvas_sizing.hpp"
#include "bridge/gl_renderer.hpp"
#include "bridge/input_routing.hpp"

#include "backbone/event_router.hpp"

#include "imgui.h"

#include <cstring>

#include <GLES3/gl3.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgl.h>

// Binding code: compiled only into the wasm app and held to -Wall -Wextra
// -Werror (not the full -Wconversion baseline) per the gen_placeholders
// precedent — DOM/GL plumbing would otherwise bury the logic in casts.

namespace poker_trainer::bridge {

namespace {

// Map a DOM KeyboardEvent.code string to a backbone KeyCode.
[[nodiscard]] backbone::KeyCode map_key_code(const char* code) noexcept {
    using KC = backbone::KeyCode;
    if (code == nullptr) {
        return KC::Unknown;
    }
    if (std::strcmp(code, "Tab") == 0) return KC::Tab;
    if (std::strcmp(code, "Enter") == 0 || std::strcmp(code, "NumpadEnter") == 0)
        return KC::Enter;
    if (std::strcmp(code, "Escape") == 0) return KC::Escape;
    if (std::strcmp(code, "Space") == 0) return KC::Space;
    if (std::strcmp(code, "ArrowUp") == 0) return KC::ArrowUp;
    if (std::strcmp(code, "ArrowDown") == 0) return KC::ArrowDown;
    if (std::strcmp(code, "ArrowLeft") == 0) return KC::ArrowLeft;
    if (std::strcmp(code, "ArrowRight") == 0) return KC::ArrowRight;
    if (std::strcmp(code, "Backspace") == 0) return KC::Backspace;
    if (std::strcmp(code, "Delete") == 0) return KC::Delete;
    // "Digit0".."Digit9"
    if (std::strncmp(code, "Digit", 5) == 0 && code[5] >= '0' && code[5] <= '9' &&
        code[6] == '\0') {
        const int offset = code[5] - '0';
        return static_cast<KC>(static_cast<int>(KC::Digit0) + offset);
    }
    // "KeyA".."KeyZ"
    if (std::strncmp(code, "Key", 3) == 0 && code[3] >= 'A' && code[3] <= 'Z' &&
        code[4] == '\0') {
        const int offset = code[3] - 'A';
        return static_cast<KC>(static_cast<int>(KC::LetterA) + offset);
    }
    return KC::Unknown;
}

[[nodiscard]] backbone::ModMask map_mods(const EmscriptenKeyboardEvent* e) noexcept {
    backbone::ModMask mods = backbone::ModMask::None;
    if (e->shiftKey) mods = mods | backbone::ModMask::Shift;
    if (e->ctrlKey) mods = mods | backbone::ModMask::Ctrl;
    if (e->altKey) mods = mods | backbone::ModMask::Alt;
    if (e->metaKey) mods = mods | backbone::ModMask::Meta;
    return mods;
}

// Map a DOM KeyboardEvent.code to the ImGuiKey ImGui's InputText uses for editing
// and navigation. Modifier keys are submitted separately as ImGuiMod_* aggregates
// (see feed_imgui_keyboard), so they return ImGuiKey_None here, as do keys ImGui
// does not need.
[[nodiscard]] ImGuiKey map_imgui_key(const char* code) noexcept {
    if (code == nullptr) return ImGuiKey_None;
    if (std::strcmp(code, "Tab") == 0) return ImGuiKey_Tab;
    if (std::strcmp(code, "Enter") == 0) return ImGuiKey_Enter;
    if (std::strcmp(code, "NumpadEnter") == 0) return ImGuiKey_KeypadEnter;
    if (std::strcmp(code, "Escape") == 0) return ImGuiKey_Escape;
    if (std::strcmp(code, "Backspace") == 0) return ImGuiKey_Backspace;
    if (std::strcmp(code, "Delete") == 0) return ImGuiKey_Delete;
    if (std::strcmp(code, "ArrowLeft") == 0) return ImGuiKey_LeftArrow;
    if (std::strcmp(code, "ArrowRight") == 0) return ImGuiKey_RightArrow;
    if (std::strcmp(code, "ArrowUp") == 0) return ImGuiKey_UpArrow;
    if (std::strcmp(code, "ArrowDown") == 0) return ImGuiKey_DownArrow;
    if (std::strcmp(code, "Home") == 0) return ImGuiKey_Home;
    if (std::strcmp(code, "End") == 0) return ImGuiKey_End;
    if (std::strcmp(code, "Space") == 0) return ImGuiKey_Space;
    if (std::strcmp(code, "Period") == 0 || std::strcmp(code, "NumpadDecimal") == 0)
        return ImGuiKey_Period;  // '.' — the EV / probability decimal point
    if (std::strcmp(code, "Minus") == 0 || std::strcmp(code, "NumpadSubtract") == 0)
        return ImGuiKey_Minus;   // '-' — the leading minus on EV
    if (std::strncmp(code, "Digit", 5) == 0 && code[5] >= '0' && code[5] <= '9' &&
        code[6] == '\0') {
        return static_cast<ImGuiKey>(static_cast<int>(ImGuiKey_0) + (code[5] - '0'));
    }
    if (std::strncmp(code, "Key", 3) == 0 && code[3] >= 'A' && code[3] <= 'Z' &&
        code[4] == '\0') {
        return static_cast<ImGuiKey>(static_cast<int>(ImGuiKey_A) + (code[3] - 'A'));
    }
    return ImGuiKey_None;
}

// True when the DOM code names the Cmd (Meta) or Ctrl key — the two modifiers
// whose hold makes the browser withhold keyup for OTHER keys (the stuck-delete
// gotcha). Used on key-up to release any keys those modifiers stranded "down".
[[nodiscard]] bool is_meta_or_ctrl_code(const char* code) noexcept {
    if (code == nullptr) return false;
    return std::strcmp(code, "MetaLeft") == 0 || std::strcmp(code, "MetaRight") == 0 ||
           std::strcmp(code, "ControlLeft") == 0 || std::strcmp(code, "ControlRight") == 0 ||
           std::strcmp(code, "OSLeft") == 0 || std::strcmp(code, "OSRight") == 0;
}

// Feed a DOM key event into ImGui IO so InputText boxes edit and receive text.
// ImGui is fed UNCONDITIONALLY (the WantCaptureKeyboard gate governs only the
// second consumer, the backbone event router), mirroring how mouse IO is fed
// before the mouse gate. On key-down the produced character (e->key) is queued so
// the active box receives digits, '.', and '-' (Z09's per-box char filter keeps
// only the characters that box permits).
void feed_imgui_keyboard(const EmscriptenKeyboardEvent* e, bool down) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiMod_Ctrl, e->ctrlKey != 0);
    io.AddKeyEvent(ImGuiMod_Shift, e->shiftKey != 0);
    io.AddKeyEvent(ImGuiMod_Alt, e->altKey != 0);
    io.AddKeyEvent(ImGuiMod_Super, e->metaKey != 0);

    const ImGuiKey key = map_imgui_key(e->code);
    if (key != ImGuiKey_None) {
        io.AddKeyEvent(key, down);
    }

    if (down) {
        // e->key is the produced value: a single printable character for text keys
        // (digits, '.', '-', letters) or a multi-character name for non-text keys
        // ("Tab", "Enter", "ArrowUp"). Queue only the single-byte printable case.
        const char* k = e->key;
        if (k[0] != '\0' && k[1] == '\0') {
            const unsigned char c = static_cast<unsigned char>(k[0]);
            if (c >= 0x20 && c < 0x7f) {
                io.AddInputCharacter(static_cast<unsigned int>(c));
            }
        }

        // Mac/Windows chord gotcha: while Cmd (Meta) -- or Ctrl on Windows -- is
        // held, the browser withholds the keyup for the OTHER key, so it would
        // strand "down" and ImGui auto-repeats it forever (Cmd+Backspace deletes
        // continuously). Don't depend on the withheld keyup: release the key
        // immediately so a chord key is a single discrete press. Input trickling
        // (io.ConfigInputTrickleEventQueue, default on) applies the down this frame
        // -- the action fires once -- and this up on the next frame.
        if ((e->metaKey != 0 || e->ctrlKey != 0) && key != ImGuiKey_None) {
            io.AddKeyEvent(key, false);
        }
    }
}

// DOM mouse button (0 left, 1 middle, 2 right) -> ImGui button (0 left, 1
// right, 2 middle).
[[nodiscard]] int imgui_mouse_button(unsigned short dom_button) noexcept {
    if (dom_button == 2) return 1;
    if (dom_button == 1) return 2;
    return 0;
}

// True when a mouse event should reach the backbone event router (the screen +
// modal handler stack). ImGui::GetIO().WantCaptureMouse (from the last NewFrame)
// is set whenever the cursor is over an ImGui window or an ImGui item is active —
// i.e. over an open modal, or dragging one of its widgets — so gating the router
// feed on it here is the single arbitration point: a click over a modal never
// reaches a screen handler, and a modal's own click-outside dismissal only ever
// sees genuine outside clicks. The app's screens draw through the background draw
// list (not ImGui windows), so their buttons keep routing normally. This is the
// reusable root every Z11 modal inherits instead of re-implementing — and
// re-breaking — per-modal click-outside arbitration. ImGui IO is fed
// unconditionally above the gate, so the modal's widgets stay fully interactive.
[[nodiscard]] bool router_should_see_mouse() noexcept {
    return !ImGui::GetIO().WantCaptureMouse;
}

EM_BOOL on_key_down(int, const EmscriptenKeyboardEvent* e, void*) {
    feed_imgui_keyboard(e, /*down=*/true);
    const backbone::KeyCode code = map_key_code(e->code);
    // WantCaptureKeyboard (from the last NewFrame) is true while an InputText is
    // active. The gate is the single arbitration point: a key ImGui is consuming
    // for text editing is not ALSO dispatched as a screen command — except Tab /
    // Enter / Escape, which the focus_manager and screens own (see input_routing).
    const bool imgui_keyboard = ImGui::GetIO().WantCaptureKeyboard;
    if (router_should_see_key(imgui_keyboard, code)) {
        backbone::dispatch_key_event(
            {backbone::KeyEventType::KeyDown, code, map_mods(e)});
    }

    // While a text field is active, consume keys so the browser does not act on
    // them — but let Ctrl/Cmd chords (reload, close tab, copy) reach the browser.
    if (imgui_keyboard && e->ctrlKey == 0 && e->metaKey == 0) {
        return EM_TRUE;
    }
    // Otherwise consume the canvas navigation keys so the page does not scroll /
    // move focus out of the canvas / navigate back; everything else passes through.
    switch (code) {
        case backbone::KeyCode::Tab:
        case backbone::KeyCode::ArrowUp:
        case backbone::KeyCode::ArrowDown:
        case backbone::KeyCode::ArrowLeft:
        case backbone::KeyCode::ArrowRight:
        case backbone::KeyCode::Space:
        case backbone::KeyCode::Backspace:
            return EM_TRUE;
        default:
            return EM_FALSE;
    }
}

EM_BOOL on_key_up(int, const EmscriptenKeyboardEvent* e, void*) {
    // Releasing Cmd/Ctrl releases everything ImGui still thinks is held: a key
    // pressed during the modifier-hold may never have received its own keyup (the
    // browser withholds it), so it would strand "down". ClearInputKeys() releases
    // all keys/buttons; the feed below then re-asserts the live modifier state from
    // this event, so a Shift still physically held stays held.
    if (is_meta_or_ctrl_code(e->code)) {
        ImGui::GetIO().ClearInputKeys();
    }
    feed_imgui_keyboard(e, /*down=*/false);
    const backbone::KeyCode code = map_key_code(e->code);
    if (router_should_see_key(ImGui::GetIO().WantCaptureKeyboard, code)) {
        backbone::dispatch_key_event(
            {backbone::KeyEventType::KeyUp, code, map_mods(e)});
    }
    return EM_FALSE;
}

void feed_imgui_mouse_pos(const EmscriptenMouseEvent* e) {
    ImGui::GetIO().AddMousePosEvent(static_cast<float>(e->targetX),
                                    static_cast<float>(e->targetY));
}

EM_BOOL on_mouse_move(int, const EmscriptenMouseEvent* e, void*) {
    feed_imgui_mouse_pos(e);
    if (router_should_see_mouse()) {
        backbone::dispatch_mouse_event(
            {backbone::MouseEventType::MouseMove, static_cast<float>(e->targetX),
             static_cast<float>(e->targetY), 0, 0.0f});
    }
    return EM_FALSE;
}

EM_BOOL on_mouse_down(int, const EmscriptenMouseEvent* e, void*) {
    feed_imgui_mouse_pos(e);
    ImGui::GetIO().AddMouseButtonEvent(imgui_mouse_button(e->button), true);
    if (router_should_see_mouse()) {
        backbone::dispatch_mouse_event(
            {backbone::MouseEventType::MouseDown, static_cast<float>(e->targetX),
             static_cast<float>(e->targetY), static_cast<int>(e->button), 0.0f});
    }
    return EM_TRUE;
}

EM_BOOL on_mouse_up(int, const EmscriptenMouseEvent* e, void*) {
    feed_imgui_mouse_pos(e);
    ImGui::GetIO().AddMouseButtonEvent(imgui_mouse_button(e->button), false);
    if (router_should_see_mouse()) {
        backbone::dispatch_mouse_event(
            {backbone::MouseEventType::MouseUp, static_cast<float>(e->targetX),
             static_cast<float>(e->targetY), static_cast<int>(e->button), 0.0f});
    }
    return EM_TRUE;
}

EM_BOOL on_wheel(int, const EmscriptenWheelEvent* e, void*) {
    ImGui::GetIO().AddMouseWheelEvent(0.0f, static_cast<float>(-e->deltaY) * 0.01f);
    if (router_should_see_mouse()) {
        backbone::dispatch_mouse_event(
            {backbone::MouseEventType::Wheel, static_cast<float>(e->mouse.targetX),
             static_cast<float>(e->mouse.targetY), 0,
             static_cast<float>(e->deltaY)});
    }
    return EM_TRUE;
}

}  // namespace

bool platform_init() {
    // Ensure a full-window #canvas exists and the page chrome does not scroll.
    EM_ASM({
        var c = document.getElementById('canvas');
        if (!c) {
            c = document.createElement('canvas');
            c.id = 'canvas';
            document.body.appendChild(c);
        }
        c.style.display = 'block';
        c.oncontextmenu = function(ev) { ev.preventDefault(); };
        document.body.style.margin = '0';
        document.documentElement.style.overflow = 'hidden';
    });

    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.majorVersion = 2;  // WebGL2 / GLES3
    attrs.minorVersion = 0;
    attrs.alpha = EM_FALSE;
    attrs.depth = EM_FALSE;
    attrs.stencil = EM_FALSE;
    attrs.antialias = EM_TRUE;
    attrs.premultipliedAlpha = EM_TRUE;
    const EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx =
        emscripten_webgl_create_context("#canvas", &attrs);
    if (ctx <= 0) {
        return false;
    }
    emscripten_webgl_make_context_current(ctx);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // no settings persistence at this layer
    // Deliberately NOT setting ImGuiBackendFlags_RendererHasVtxOffset: WebGL2 has
    // no base-vertex draw, so ImGui must keep vertex offsets at zero.

    if (!gl_renderer_init()) {
        return false;
    }

    // DOM input -> backbone event_router (+ ImGui IO for mouse). Keys on the
    // window; mouse/wheel on the canvas. useCapture = false.
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr,
                                    EM_FALSE, on_key_down);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr,
                                  EM_FALSE, on_key_up);
    emscripten_set_mousemove_callback("#canvas", nullptr, EM_FALSE, on_mouse_move);
    emscripten_set_mousedown_callback("#canvas", nullptr, EM_FALSE, on_mouse_down);
    emscripten_set_mouseup_callback("#canvas", nullptr, EM_FALSE, on_mouse_up);
    emscripten_set_wheel_callback("#canvas", nullptr, EM_FALSE, on_wheel);
    return true;
}

bool platform_launch_is_mobile() noexcept {
    static const bool mobile = [] {
        const char* ua = emscripten_run_script_string("navigator.userAgent");
        return is_mobile_user_agent(ua != nullptr ? ua : "");
    }();
    return mobile;
}

CanvasDims platform_sync_viewport() noexcept {
    const int viewport_w = EM_ASM_INT({ return window.innerWidth | 0; });
    const int viewport_h = EM_ASM_INT({ return window.innerHeight | 0; });
    const double dpr = EM_ASM_DOUBLE({ return window.devicePixelRatio || 1.0; });

    const CanvasDims css = canvas_dims_from_viewport(viewport_w, viewport_h);
    const int fb_w = static_cast<int>(static_cast<double>(css.width) * dpr);
    const int fb_h = static_cast<int>(static_cast<double>(css.height) * dpr);
    emscripten_set_canvas_element_size("#canvas", fb_w, fb_h);
    EM_ASM(
        {
            var c = document.getElementById('canvas');
            if (c) {
                c.style.width = $0 + 'px';
                c.style.height = $1 + 'px';
            }
        },
        css.width, css.height);

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize =
        ImVec2(static_cast<float>(css.width), static_cast<float>(css.height));
    io.DisplayFramebufferScale =
        ImVec2(static_cast<float>(dpr), static_cast<float>(dpr));
    return css;
}

void platform_present(float r, float g, float b, float a) noexcept {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
    gl_renderer_render(ImGui::GetDrawData());
}

}  // namespace poker_trainer::bridge
