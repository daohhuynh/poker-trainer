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

// "Show Shop button" gate (boot wires it to the live setting). Unwired => visible.
std::function<bool()> g_shop_icon_visible{};

// Offline hint gate (boot wires it to authenticated && browser-offline). Unwired => false.
std::function<bool()> g_offline_hint{};

// Mode Selection -> Root reverse-morph handler (Zone 07 wires it). Unwired => plain cut.
std::function<void()> g_mode_to_root{};

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

void set_shop_icon_visible_gate(std::function<bool()> visible) {
    g_shop_icon_visible = std::move(visible);
}

bool shop_icon_visible() noexcept {
    // Unwired (native tests) => visible, so the with-Shop focus order is preserved.
    return !g_shop_icon_visible || g_shop_icon_visible();
}

void set_offline_hint_gate(std::function<bool()> hint) {
    g_offline_hint = std::move(hint);
}

bool offline_hint() noexcept {
    // Unwired (native tests / guests) => false, so the indicator relies on sync status alone.
    return static_cast<bool>(g_offline_hint) && g_offline_hint();
}

void set_mode_to_root_transition(std::function<void()> handler) {
    g_mode_to_root = std::move(handler);
}

void begin_mode_to_root_transition() {
    if (g_mode_to_root) {
        g_mode_to_root();  // Zone 07: play the reverse button morph (or instant on Reduce Motion)
        return;
    }
    // Unwired (native tests / pre-boot): navigate straight to Root with no morph.
    backbone::set_screen(backbone::ScreenId::Root, std::nullopt);
}

void reset_screen_dispatch_for_testing() noexcept {
    for (ScreenRenderFn& fn : g_renderers) {
        fn = nullptr;
    }
    g_overlay = nullptr;
    g_mode_to_root = nullptr;
}

}  // namespace poker_trainer::bridge
