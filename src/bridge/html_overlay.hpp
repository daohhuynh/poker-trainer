#pragma once

#include <string_view>

// Zone 05 (Bridge) — a single reusable, NON-INTERACTIVE HTML overlay (an <iframe>)
// positioned over the WebGL canvas so the app can display a same-origin HTML document
// (the hosted legal docs) that ImGui cannot render itself. The overlay is display-only:
//
//   * pointer-events:none — every click / wheel falls THROUGH to the canvas, so the app
//     keeps mouse + keyboard focus (Escape / Tab / click-outside all still route through
//     the app's event router). The iframe never receives pointer input and is never
//     given focus (no .focus() call, tabindex=-1), so the app's focus system is intact.
//   * The app drives scrolling explicitly via scroll_html_overlay (forwarded from the
//     doc modal's arrow keys and the platform layer's wheel / PageUp-Down handling).
//
// Coordinates are CSS pixels, which equal ImGui screen coordinates 1:1 (DPR applies only
// to the GL framebuffer, never this layer — see platform.cpp::platform_sync_viewport), so
// a rect taken from ImGui lands exactly over the modal body.
//
// Emscripten-only: the real DOM implementation is compiled behind __EMSCRIPTEN__; the
// native (test) build gets no-ops so the bridge library stays DOM-free and host-buildable.

namespace poker_trainer::bridge {

// Show (create-or-reuse the single iframe) the document at `url`, positioned at the given
// CSS-pixel rect (the modal body's ImGui screen rect). `url` is (re)loaded only when it
// changes from the last shown url, so calling this every frame with a stable url just keeps
// it visible / repositions on resize (no reload, no scroll reset). The iframe carries an
// opaque light background: the legal docs render black text on a transparent body and would
// otherwise be invisible over the dark app.
void show_html_overlay(std::string_view url, float x_px, float y_px, float w_px, float h_px);

// Hide the overlay (display:none, src cleared). Called on every doc-modal close path.
void hide_html_overlay();

// Scroll the shown document by `delta_px` (positive = down). No-op when nothing is shown.
void scroll_html_overlay(float delta_px);

// True while an overlay document is shown. The platform layer gates its wheel / PageUp-Down
// forwarding on this (cheap C++ read) before touching the DOM.
[[nodiscard]] bool html_overlay_visible();

}  // namespace poker_trainer::bridge
