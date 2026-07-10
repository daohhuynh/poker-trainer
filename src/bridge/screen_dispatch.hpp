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

// ----- Persistent-cluster Shop-icon visibility gate (Settings: "Show Shop button") -----
//
// The persistent cluster (Zone 11) renders across the Mode Selection / Game / Post-Round
// screens. When the user hides the Shop button, the Shop icon must not draw, must not
// hit-test, and must be skipped in every screen's Tab order. Like the ambient / hover-tilt
// gates, boot wires a single live-settings reader once; the cluster render + hit-test and
// each screen's focus-list registration consult it. Lives in the bridge (not Zone 11) so the
// Game focus list — built in Zone 09, which does not depend on Zone 11 — can read the same
// gate. Defaults to VISIBLE when unwired (native tests), preserving the with-Shop focus order.
void set_shop_icon_visible_gate(std::function<bool()> visible);
[[nodiscard]] bool shop_icon_visible() noexcept;

// ----- Offline hint (browser connectivity) for the offline indicator -----
//
// The offline sync indicator normally shows on a failed sync ATTEMPT (SyncStatus::
// SyncFailing). That means simply going offline — without a state change to fail-push —
// surfaces nothing. Boot wires this hint to "authenticated AND the browser reports offline"
// (navigator.onLine), and the indicator ORs it with the sync status so a logged-in user who
// drops connectivity sees the indicator immediately. Lives in the bridge so the Zone 11
// indicator can read it without a Zone 04 / platform dependency. Default false (unwired /
// native tests / guests).
void set_offline_hint_gate(std::function<bool()> hint);
[[nodiscard]] bool offline_hint() noexcept;

// ----- Mode Selection -> Root reverse-morph seam (Zone 07) -----
//
// Going Root -> Mode Selection plays the button morph (2x2 grid -> STANDARD + top-right
// icon cluster). Returning Mode Selection -> Root should play that morph in REVERSE. The
// morph controller is owned by Zone 07 (ScreensRuntime), but the return-to-root triggers
// live in two zones — the persistent-cluster Home icon (Zone 11) and Mode Selection's own
// Escape / Home-key handler (Zone 07). Both call begin_mode_to_root_transition() instead
// of set_screen(Root); Zone 07 wires set_mode_to_root_transition at install to start the
// reverse morph (Reduce Motion collapses it to an instant cut). Unwired (native tests /
// pre-boot) => a plain set_screen(Root), so navigation still works with no morph.
void set_mode_to_root_transition(std::function<void()> handler);
void begin_mode_to_root_transition();

// Clear all registrations (screen renderers + overlay + transition seam). Used by tests.
void reset_screen_dispatch_for_testing() noexcept;

}  // namespace poker_trainer::bridge
