#include "bridge/screen_dispatch.hpp"

#include "backbone/screen_state.hpp"

#include <array>
#include <cstddef>
#include <utility>

// Single-threaded: registration happens during zone init and dispatch happens
// in the main loop, both on the browser main thread. No synchronization needed.

namespace poker_trainer::bridge {

namespace {

std::array<ScreenRenderFn, backbone::kScreenCount> g_renderers{};
OverlayRenderFn g_overlay{};

[[nodiscard]] std::size_t slot(backbone::ScreenId screen) noexcept {
    return static_cast<std::size_t>(screen);
}

}  // namespace

void register_screen_renderer(backbone::ScreenId screen, ScreenRenderFn renderer) {
    g_renderers[slot(screen)] = std::move(renderer);
}

bool has_screen_renderer(backbone::ScreenId screen) noexcept {
    return static_cast<bool>(g_renderers[slot(screen)]);
}

bool render_screen(backbone::ScreenId screen) {
    const ScreenRenderFn& fn = g_renderers[slot(screen)];
    if (!fn) {
        return false;
    }
    fn();
    return true;
}

void register_overlay_renderer(OverlayRenderFn renderer) {
    g_overlay = std::move(renderer);
}

void render_overlay() {
    if (g_overlay) {
        g_overlay();
    }
}

void reset_screen_dispatch_for_testing() noexcept {
    for (ScreenRenderFn& fn : g_renderers) {
        fn = nullptr;
    }
    g_overlay = nullptr;
}

}  // namespace poker_trainer::bridge
