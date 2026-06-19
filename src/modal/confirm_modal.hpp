#pragma once

#include "backbone/focus_manager.hpp"

#include <array>
#include <functional>
#include <string>

// Zone 11 — uniform confirmation modal (ARCHITECTURE L84 / Notes — Escape Key
// Behavior). Yes (red, state_fail) / No (grey, bg_button_default, DEFAULT focus) /
// X close, with a single shared focus list No -> Yes -> X close (default pointer 0
// = No, the safe default), wrapping. Space/Enter/click activate; Escape = No. No
// icon-pill header (the body provides context).
//
// All confirmation instances (leave-drill, leave-site, section reset, delete
// account, skip tutorial, reset all settings) reuse this one pattern + focus list;
// only the body text and the Yes action differ. This wave wires the leave-drill
// instance (Game X / Escape -> Yes returns to Mode Selection); the others are
// Z12/Z14 seams that will call open_confirm_modal with their own spec.

namespace poker_trainer::modal {

// The three confirmation focusables, shared across every confirmation instance.
inline constexpr backbone::FocusableId kConfirmNo =
    backbone::make_focusable_id("confirm.no");
inline constexpr backbone::FocusableId kConfirmYes =
    backbone::make_focusable_id("confirm.yes");
inline constexpr backbone::FocusableId kConfirmClose =
    backbone::make_focusable_id("confirm.close");

// Focus order (Tab) and default pointer. No is index 0 — the safe default.
inline constexpr std::array<backbone::FocusableId, 3> kConfirmFocusOrder{
    kConfirmNo, kConfirmYes, kConfirmClose};
inline constexpr std::size_t kConfirmDefaultFocusIndex = 0;

// One confirmation instance: the body text and the action Yes performs. No / X
// both simply dismiss (no action), so only the Yes action is carried.
struct ConfirmSpec {
    std::string body{};
    std::function<void()> on_yes{};
};

// Render the active confirmation modal as a centered overlay. Called from the modal
// overlay dispatch when the leave-drill (or any future) confirmation is the topmost
// modal. ImGui; browser-verified.
void render_confirm_modal(const ConfirmSpec& spec);

}  // namespace poker_trainer::modal
