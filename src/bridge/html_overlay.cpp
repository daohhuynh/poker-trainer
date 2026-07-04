#include "bridge/html_overlay.hpp"

// Binding-adjacent (DOM manipulation via EM_ASM), so the real body is compiled only under
// __EMSCRIPTEN__; the native branch is DOM-free no-ops. Every consumer (settings_modal in
// the native-testable `settings` library, platform.cpp in the wasm-only bridge_platform)
// links the same header, so the symbols resolve in both build configs.

#ifdef __EMSCRIPTEN__

#include <string>

#include <emscripten/emscripten.h>

#endif

namespace poker_trainer::bridge {

#ifdef __EMSCRIPTEN__

namespace {

// The url currently loaded into the iframe. Reassigning iframe.src every frame would reload
// the document continuously (flicker + scroll reset), so src is only set when this differs
// from the requested url. Cleared on hide, so the next show reloads (fresh scroll-to-top).
std::string g_current_url{};

// Last applied rect. The doc modal is fixed size/position, so the rect is stable frame to
// frame; skipping the DOM write when nothing changed keeps show_html_overlay (called every
// frame the modal is open) from churning the DOM. Sentinel < 0 forces the first write.
float g_x{-1.0f};
float g_y{-1.0f};
float g_w{-1.0f};
float g_h{-1.0f};

// Whether a document is currently shown (drives html_overlay_visible()).
bool g_visible{false};

}  // namespace

void show_html_overlay(std::string_view url, float x_px, float y_px, float w_px, float h_px) {
    const std::string requested{url};
    const bool url_changed = (requested != g_current_url);
    const bool rect_changed = (x_px != g_x || y_px != g_y || w_px != g_w || h_px != g_h);
    g_visible = true;
    if (!url_changed && !rect_changed) {
        return;  // already showing this doc at this rect — nothing to write this frame
    }
    g_current_url = requested;
    g_x = x_px;
    g_y = y_px;
    g_w = w_px;
    g_h = h_px;
    EM_ASM(
        {
            var f = document.getElementById('pt_html_overlay');
            if (!f) {
                f = document.createElement('iframe');
                f.id = 'pt_html_overlay';
                f.setAttribute('tabindex', '-1');  // never a native tab stop
                f.style.position = 'fixed';
                f.style.border = '0';
                f.style.margin = '0';
                f.style.padding = '0';
                // Display-only: clicks + wheel pass THROUGH to the canvas; the iframe never
                // receives pointer input and never takes keyboard focus, so the app keeps
                // focus (Escape / Tab / click-outside all keep working).
                f.style.pointerEvents = 'none';
                // The legal docs force background:transparent + black text; back the iframe
                // with an opaque light (parchment) surface so the text is legible over the
                // dark app. This is the single authorized hardcoded color (a DOM CSS value,
                // not a theme-tinted app element) — the doc must stay light in every theme.
                f.style.background = '#f7f3ea';
                f.style.zIndex = '10';
                document.body.appendChild(f);
            }
            if ($5) {  // url changed -> (re)load the document
                f.src = UTF8ToString($4);
            }
            f.style.display = 'block';
            f.style.left = $0 + 'px';
            f.style.top = $1 + 'px';
            f.style.width = $2 + 'px';
            f.style.height = $3 + 'px';
        },
        x_px, y_px, w_px, h_px, g_current_url.c_str(), url_changed ? 1 : 0);
}

void hide_html_overlay() {
    g_visible = false;
    g_current_url.clear();  // force a reload (fresh scroll-to-top) on the next show
    EM_ASM({
        var f = document.getElementById('pt_html_overlay');
        if (f) {
            f.style.display = 'none';
            f.src = 'about:blank';  // release the doc so it cannot paint through / hold memory
        }
    });
}

void scroll_html_overlay(float delta_px) {
    EM_ASM(
        {
            var f = document.getElementById('pt_html_overlay');
            if (f && f.contentWindow) {
                f.contentWindow.scrollBy(0, $0);
            }
        },
        delta_px);
}

bool html_overlay_visible() { return g_visible; }

#else  // native (test) build: DOM-free no-ops so the bridge library stays host-buildable

void show_html_overlay(std::string_view, float, float, float, float) {}
void hide_html_overlay() {}
void scroll_html_overlay(float) {}
bool html_overlay_visible() { return false; }

#endif

}  // namespace poker_trainer::bridge
