#pragma once

#include "math/interrogator.hpp"

#include "backbone/focus_manager.hpp"
#include "engine/scenario.hpp"

// Keyboard routing for the math interrogator (Module 5). The number-key focus
// mapping and the focus-resolution logic live here as pure helpers; the event-
// router handler registration is in keybinds.cpp (install_interrogator).
//
// POSITIONAL keybinds (ARCHITECTURE "Keyboard Shortcuts"): the number key maps to
// the input's POSITION in the on-screen focus order for the current scenario,
// 1..N top to bottom -- NOT a fixed input type. So the same number means different
// inputs in different scenarios (intended):
//   Caller:                          1=Pot Odds, 2=Outs, 3=Equity, 4=EV
//   Aggressor Pure Bluff:            1=Breakeven Fold %, 2=EV, 3=Bet Size
//   Aggressor Value Bet:             1=Equity if Called, 2=EV, 3=Bet Size
//   Aggressor Semi-Bluff:            1=Breakeven Fold %, 2=Equity if Called, 3=EV, 4=Bet Size
// Every Aggressor sub-type has a per-tier input (EV; Pure/Semi also Breakeven Fold %),
// so all three are sequential multi-tier when the Bet Sizing Engine is on: the order
// above is the CURRENT tier's view, with Equity-if-Called shown on every tier screen.
//
// SEAM(Z12): a user-configurable keybind Settings section (settings.hpp carries no
// keybind fields yet) is separate future work; until then this positional default
// is authoritative.

namespace poker_trainer::interrogator {

// Resolve the focus target for number key `digit` in the current scenario: the
// focus id of the input at POSITION `digit` (1-based) in the current screen's focus
// order (the visible numeric boxes top to bottom, then the bet-size group when
// present), or kNoFocus when no input sits at that position (e.g. "4" in a Caller's
// 4-stop view is EV, "5" is kNoFocus; "3" in a Pure Bluff is the Bet Size group).
[[nodiscard]] backbone::FocusableId focus_target_for_digit(const InterrogatorState& state,
                                                           int digit) noexcept;

}  // namespace poker_trainer::interrogator
