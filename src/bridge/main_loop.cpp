#include "bridge/main_loop.hpp"

#include "bridge/bridge_runtime.hpp"
#include "bridge/canvas_sizing.hpp"
#include "bridge/error_screen.hpp"
#include "bridge/loading_screen.hpp"
#include "bridge/platform.hpp"
#include "bridge/screen_dispatch.hpp"

#include "backbone/animation_clock.hpp"
#include "backbone/screen_state.hpp"

#include "assets/tier_config.hpp"
#include "theme/theme_tokens.hpp"

#include "imgui.h"

#include <cstdint>
#include <optional>

#include <emscripten/emscripten.h>

// Binding-adjacent main loop, held to -Wall -Wextra -Werror in bridge_platform.

namespace poker_trainer::bridge {

namespace {

double g_last_now_ms = 0.0;
bool g_have_last_now = false;

// Tier-1 finished: take the resolved boot route. Shared-scenario goes straight
// to Game with the parsed id; the normal path renders Root.
void on_tier1_complete(BridgeRuntime& rt) {
    if (rt.route == BootRoute::SharedScenario) {
        backbone::set_screen(backbone::ScreenId::Game, rt.shared_id);
    } else {
        backbone::set_screen(backbone::ScreenId::Root, std::nullopt);
    }
    rt.phase = BootPhase::Running;
    // SEAM(Z14): the ceremonial transition into Game (shared-scenario path) is
    // wired by Zone 14; Z05 performs only the state transition here.
}

void frame() {
    BridgeRuntime& rt = runtime();

    // 1. Advance the animation clock once per frame, in milliseconds.
    const double now = emscripten_get_now();
    double delta_ms = g_have_last_now ? (now - g_last_now_ms) : 0.0;
    if (delta_ms < 0.0) {
        delta_ms = 0.0;
    }
    g_last_now_ms = now;
    g_have_last_now = true;
    backbone::tick(static_cast<std::uint64_t>(delta_ms));

    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime =
        delta_ms > 0.0 ? static_cast<float>(delta_ms / 1000.0) : (1.0f / 60.0f);

    // 2. Pump asset loading (re-issues retries whose backoff has elapsed).
    if (rt.tier_loader != nullptr) {
        rt.tier_loader->poll();
    }

    // 3. Advance the boot phase: Tier-1 fatal failure -> Error screen; Tier-1
    //    complete -> take the boot route.
    if (rt.phase == BootPhase::Loading && rt.tier_loader != nullptr) {
        if (rt.tier_loader->has_error_screen_failure()) {
            backbone::set_screen(backbone::ScreenId::Error, std::nullopt);
            rt.phase = BootPhase::Running;
        } else if (rt.tier_loader->is_tier_complete(assets::AssetTier::Tier1)) {
            on_tier1_complete(rt);
        }
    }

    // 4. Sync the canvas to the viewport and build this frame.
    const CanvasDims dims = platform_sync_viewport();
    ImGui::NewFrame();

    const DisplayMode mode =
        resolve_display_mode(platform_launch_is_mobile(), dims.height);
    if (mode != DisplayMode::Normal) {
        render_fallback(mode);
    } else if (rt.phase == BootPhase::Loading) {
        render_loading_screen();
    } else {
        const backbone::ScreenId screen = backbone::read_screen_state().current;
        if (screen == backbone::ScreenId::Error) {
            render_error_screen();
        } else {
            // Zones 07/08/13 render through the dispatch registry; with none
            // registered, the frame is just the background clear below.
            render_screen(screen);
        }
    }

    ImGui::Render();
    const ImVec4 bg = theme::get_color(theme::ColorToken::BgPrimary);
    platform_present(bg.x, bg.y, bg.z, bg.w);
}

}  // namespace

void start_main_loop() {
    // fps = 0 -> drive from requestAnimationFrame. simulate_infinite_loop = false:
    // start_main_loop is called from the async IDBFS sync callback (not from
    // main()), so it must register the RAF loop and return normally rather than
    // throw to unwind the stack. The per-frame state above is file-scope static,
    // so it persists across the return; EXIT_RUNTIME=0 keeps the runtime alive
    // after app_init / main return so the RAF loop keeps firing.
    emscripten_set_main_loop(frame, 0, EM_FALSE);
}

}  // namespace poker_trainer::bridge
