#pragma once

#include "bridge/canvas_sizing.hpp"

// Emscripten platform layer: owns the WebGL2 context, the Dear ImGui context,
// browser input wiring (DOM events -> backbone event_router + ImGui IO), and the
// canvas<->viewport binding. Emscripten-only; never compiled into the native
// test build. Written in-house so the app needs no SDL/GLFW dependency — only
// emscripten's bundled html5 + WebGL APIs.

namespace poker_trainer::bridge {

// Create the WebGL2 context, the ImGui context, the GL renderer, and register
// DOM input callbacks. Returns false if the WebGL2 context or GL renderer could
// not be created.
[[nodiscard]] bool platform_init();

// True if the launch User-Agent identified a mobile browser. Evaluated once and
// cached.
[[nodiscard]] bool platform_launch_is_mobile() noexcept;

// Sync the canvas drawing buffer and ImGui DisplaySize/FramebufferScale to the
// current viewport, and return the CSS-pixel canvas dims for this frame.
CanvasDims platform_sync_viewport() noexcept;

// The current canvas pixel dimensions (ZONES.md Z05 export canvas_dims()). The
// dims last computed by platform_sync_viewport — the canvas always matches the
// viewport, so this is the live canvas size. Zero before the first frame syncs.
[[nodiscard]] CanvasDims canvas_dims() noexcept;

// Clear the framebuffer to the given RGBA and present the current ImGui draw
// data. Call after ImGui::Render().
void platform_present(float r, float g, float b, float a) noexcept;

// Install the navigator.onLine watch: an initial read plus 'online'/'offline' event
// listeners that keep a JS flag live. Call once at boot. Pairs with browser_online(), which
// reads that flag. Wasm-only (the query lives here, under the binding warning baseline).
void install_online_watch();

// True when the browser reports connectivity (navigator.onLine). Reads the listener-
// maintained flag (cheap); defaults to true (online) when unknown, so the offline indicator
// never shows spuriously before the watch is installed. Wasm-only.
[[nodiscard]] bool browser_online();

// Install the matchMedia('(prefers-reduced-motion: reduce)') watch: an initial read plus a
// change listener that keeps a JS-side flag live. Call once at boot. Pairs with
// refresh_os_reduced_motion, which samples that flag into bridge::set_os_reduced_motion so
// the effective reduce-motion state honors the OS setting at runtime. Wasm-only (the query
// lives here, under the binding warning baseline that permits EM_ASM).
void install_os_reduced_motion_watch();

// Sample the OS prefers-reduced-motion flag and feed it to bridge::set_os_reduced_motion.
// Cheap (reads a listener-maintained JS boolean, not a fresh matchMedia); call once per
// frame so a mid-session OS change re-evaluates live.
void refresh_os_reduced_motion();

// Enable/disable the browser's native "Leave site? Changes may not be saved" confirmation
// on a navigation-away attempt (back button, tab close, Cmd/Ctrl+W, reload). Installs a
// single `beforeunload` listener on first call and flips an active flag: while active the
// listener calls preventDefault()/returnValue so the browser shows ITS OWN native dialog —
// modern browsers do not let an in-canvas modal reliably block beforeunload. Idempotent
// (only touches the DOM when the flag changes); the caller drives it from the live app
// state (Game screen + active scenario + the "Confirm before leaving site" setting).
void set_leave_confirmation_active(bool active);

}  // namespace poker_trainer::bridge
