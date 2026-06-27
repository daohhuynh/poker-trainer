#pragma once

#include "modal/modals.hpp"

#include "backbone/focus_manager.hpp"

#include "assets/asset_paths.hpp"

// Zone 11 — internal modal shell helpers + per-modal render entry points. Included
// only by the Z11 render translation units (they already depend on ImGui via the
// theme library). The shared shell gives every modal the consistent chrome from
// ARCHITECTURE's Modal Popup Conventions: centered overlay, X close, click-outside
// dismissal, the optional top-left icon-plus-name pill header, and the tutorial
// lock (40% opacity + "Locked during tutorial" banner).

namespace poker_trainer::modal {

// The installed app-root runtime, or nullptr before install_modals (native tests).
// Z11-internal accessor (mirrors bridge::runtime()); free functions null-guard it.
[[nodiscard]] ModalRuntime* modal_runtime();

// Canonical frame size (viewport fractions) for the pill-header cluster modals —
// Settings, Shop, Help, and the Leaderboard view. All render at this single fixed
// size regardless of content (it is the size Help renders at), so the frame and the
// X close never move between them; bodies that overflow scroll, bodies that underflow
// leave empty space. Confirmation modals, the Custom popup, and the clipboard
// fallback keep their own compact sizing and do NOT use these.
inline constexpr float kClusterModalWidthFrac = 0.5f;
inline constexpr float kClusterModalHeightFrac = 0.7f;

// Begin a centered modal window sized to fractions of the viewport. Returns true
// when the window body is visible (caller draws the body, then calls modal_end()).
[[nodiscard]] bool modal_begin_centered(const char* imgui_id, float w_frac, float h_frac);
void modal_end();

// Screen-space anchor returned by modal_draw_pill_header so a caller can lay out
// inline trailing content (e.g. the Leaderboard's "refreshes daily" annotation) just
// to the RIGHT of the pill without re-deriving the pill geometry. Two plain floats so
// this header stays free of an ImGui (ImVec2) dependency.
struct PillHeaderAnchor {
    float trailing_x{0.0f};  // screen x just right of the pill, with a small gap
    float text_y{0.0f};      // screen y of the pill's text top (align trailing text here)
};

// Top-left icon-plus-name pill header (Settings / Help / Shop). The icon glyph
// (text_primary) sits to the LEFT of a rounded pill (bg_button_default fill) holding
// the name in text_primary. Advances the ImGui cursor below the header. Returns the
// anchor for inline trailing text to the right of the pill; most callers ignore it.
PillHeaderAnchor modal_draw_pill_header(assets::AssetId icon, const char* name);

// The modal's own X close button (top-right of the modal, distinct from the cluster
// X). `close_focus` is the focusable id of the X for this modal (focus ring). True
// when clicked.
[[nodiscard]] bool modal_draw_x_close(backbone::FocusableId close_focus);

// True when this frame's click should dismiss the modal (a left click outside the
// modal window), honoring the opening-frame guard so the click that opened the
// modal never immediately dismisses it.
[[nodiscard]] bool modal_click_outside_dismissed();

// The tutorial lock. When is_modal_locked(), interactive controls render at ~40%
// opacity and their clicks are silently suppressed; the X close and scrolling stay
// live. Wrap interactive body controls in modal_begin_locked_controls() /
// modal_end_locked_controls(), and draw modal_draw_lock_banner() at the top.
[[nodiscard]] bool modal_is_locked();
void modal_draw_lock_banner();
void modal_begin_locked_controls();
void modal_end_locked_controls();

// ----- Per-modal render entry points (dispatched by render_modal_overlay) -----
void render_help_modal();
void render_settings_shell();
void render_shop_shell();
void render_leaderboard_view(ModalRuntime& runtime);

}  // namespace poker_trainer::modal
