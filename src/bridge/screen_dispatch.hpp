#pragma once

#include "backbone/screen_state.hpp"

#include <functional>

// Render-dispatch registry (the cross-zone render SEAM).
//
// Zones 07 (Root / Mode Selection), 08 (Game), and 13 (Post-Round) own the
// rendering of their screens, but Z05's main loop must invoke "the active
// screen" each frame without depending on those zones. This registry is the
// indirection: each screen-owning zone registers a render callback for its
// ScreenId during its init, and the main loop looks the active screen up and
// calls it. With no renderer registered for the active screen, the main loop
// clears to the background only (Z05 itself renders the Loading and Error
// screens directly, not through this table).

namespace poker_trainer::bridge {

// A screen's per-frame render callback. Invoked by the main loop with the
// active screen's id resolved from the screen-state singleton. Takes no
// arguments: a renderer reads whatever it needs (the active scenario, theme,
// focus state) from the backbone and its own zone state.
using ScreenRenderFn = std::function<void()>;

// Register (or replace) the render callback for a screen. Called by the
// screen-owning zone during its initialization. Passing a null function clears
// any existing registration for that screen.
void register_screen_renderer(backbone::ScreenId screen, ScreenRenderFn renderer);

// True if a non-null renderer is registered for the screen.
[[nodiscard]] bool has_screen_renderer(backbone::ScreenId screen) noexcept;

// Invoke the registered renderer for `screen` if one exists. Returns true if a
// renderer ran, false if none was registered (the caller then clears to the
// background only). The main loop calls this with the active screen each frame.
bool render_screen(backbone::ScreenId screen);

// ----- Top-level overlay seam (Zone 11) -----
//
// Modals and the Service Outage Banner are a single top-level overlay rendered
// ABOVE the active screen each frame (per the event-router priority stack:
// tutorial > modal > screen). The main loop invokes the registered overlay right
// after the active screen, so it composites on top without the bridge depending on
// Zone 11 (function-pointer indirection, exactly like register_screen_renderer).
using OverlayRenderFn = std::function<void()>;

// Register (or replace) the single overlay renderer. Passing a null function clears
// it. Called once by Zone 11's install.
void register_overlay_renderer(OverlayRenderFn renderer);

// Invoke the registered overlay if one exists. Called by the main loop each frame
// after the active screen renders. A no-op when no overlay is registered.
void render_overlay();

// Clear all registrations (screen renderers + overlay). Used by tests.
void reset_screen_dispatch_for_testing() noexcept;

}  // namespace poker_trainer::bridge
