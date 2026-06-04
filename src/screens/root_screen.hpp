#pragma once

// Zone 07 — Root screen.
//
// Logo (top-left), a middle 2x2 button grid (Play=TL, Settings=TR, Shop=BL,
// Help=BR of the grid), and a Home icon (top-right). On Root, Home reloads the
// page. Play triggers the Root -> Mode Selection morph (animations/button_morph).

#include "animations/button_morph.hpp"
#include "backbone/focus_manager.hpp"

#include <array>

namespace poker_trainer::screens {

// ----- Escape behavior --------------------------------------------------------
//
// Notes — Escape Key Behavior: on Root, Escape does nothing (the user is already
// at the application root). The handler still consumes the key so nothing below
// it acts on it.
[[nodiscard]] inline bool on_root_escape() noexcept { return true; }

// ----- Focus list -------------------------------------------------------------
//
// Order per ARCHITECTURE: Play -> Settings -> Shop -> Help -> Home, wrapping back
// to Play; default pointer 0 (Play).

inline constexpr backbone::FocusableId kFocusRootPlay =
    backbone::make_focusable_id("root.play");
inline constexpr backbone::FocusableId kFocusRootSettings =
    backbone::make_focusable_id("root.settings");
inline constexpr backbone::FocusableId kFocusRootShop =
    backbone::make_focusable_id("root.shop");
inline constexpr backbone::FocusableId kFocusRootHelp =
    backbone::make_focusable_id("root.help");
inline constexpr backbone::FocusableId kFocusRootHome =
    backbone::make_focusable_id("root.home");

inline constexpr std::array<backbone::FocusableId, 5> kRootFocusOrder{
    kFocusRootPlay, kFocusRootSettings, kFocusRootShop, kFocusRootHelp, kFocusRootHome,
};

// Register the Root focus list as the base context. Called on screen entry
// (idempotent — register_focus_list replaces the active list).
inline void register_root_focus_list() noexcept {
    backbone::register_focus_list(backbone::ScreenId::Root, kRootFocusOrder);
}

// ----- Keyboard activation (Space / Enter) ------------------------------------
//
// Space (and Enter on this non-Game screen) activates the focused element, same
// as a click. Only Play has a keyboard activation action — it starts the Root ->
// Mode morph; Settings/Shop/Help open modals (Z11 seam) and Home reloads (Z05
// seam), neither wired this wave, so activating them is a no-op like clicking them
// is today. Pure (no ImGui / router), so the mapping is unit-tested directly.
[[nodiscard]] constexpr bool root_focus_activates_morph(backbone::FocusableId id) noexcept {
    return id == kFocusRootPlay;
}

// ----- Render + handler wiring -------------------------------------------------

// ZONES.md export. Draws the Root body (logo, 2x2 grid, Home icon). Stateless.
void render_root_screen();

// Draws one frame of the in-flight Root -> Mode Selection morph at global
// progress `global_t` in [0, 1]: the static logo / Home icon plus the four
// grid buttons interpolated toward their Mode Selection targets (Play ->
// STANDARD; Settings/Shop/Help -> the top-right cluster slots). Stateless; the
// caller (Z05's main loop, via the screen-dispatch registry) owns the
// MorphController and supplies the progress. Drawn instead of render_root_screen
// while a morph is active.
void render_root_morph_frame(float global_t);

// Install the Root event-router handlers (Escape -> no-op; Play -> start the
// morph on the caller-owned MorphController; Settings/Shop/Help -> open their
// modals [Zone 11 seam]; Home -> reload the page [Zone 05 seam]). Deferred wiring
// seam: takes the main-loop-owned morph controller by reference, since the no-arg
// render export and CLAUDE.md section 10 (no global state) give Zone 07 no place
// to own it. Called by Zone 05's main loop, not this wave.
void install_root_handlers(animations::MorphController& morph);

}  // namespace poker_trainer::screens
