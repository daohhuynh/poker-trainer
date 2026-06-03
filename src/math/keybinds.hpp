#pragma once

#include "math/interrogator.hpp"

#include "backbone/focus_manager.hpp"
#include "engine/scenario.hpp"

#include <array>

// Keyboard routing for the math interrogator (Module 5). The number-key focus
// mapping and the focus-resolution logic live here as pure helpers; the event-
// router handler registration is in keybinds.cpp (install_interrogator).
//
// SEAM(Z12): the 1-6 mapping is the documented default from ARCHITECTURE's
// "Keyboard Shortcuts" (settings.hpp carries no keybind fields yet). When the
// Keyboard Shortcuts settings section lands (Z12, W4), the handler reads the
// user-configured mapping; until then this default is authoritative.

namespace poker_trainer::interrogator {

// Default number-key -> input mapping, indexed by (digit - 1):
// 1->Pot Odds, 2->Outs, 3->Equity, 4->EV, 5->Fold Probability, 6->Bet Size.
inline constexpr std::array<engine::InputId, 6> kDigitToInput = {
    engine::InputId::PotOdds,        // 1
    engine::InputId::Outs,           // 2
    engine::InputId::Equity,         // 3
    engine::InputId::Ev,             // 4
    engine::InputId::FoldProbability,// 5
    engine::InputId::BetSize,        // 6
};

// Resolve the focus target for number key `digit` (1..6) in the current
// scenario: the focus id of the first box (or the bet group) that digit maps
// to, or kNoFocus when no such input is present (e.g. "2"=Outs in an Aggressor
// scenario, or "6"=Bet Size in a Caller scenario). Multi-tier per-tier inputs
// resolve to the tier-0 box -- see the SEAM note in keybinds.cpp.
[[nodiscard]] backbone::FocusableId focus_target_for_digit(const InterrogatorState& state,
                                                           int digit) noexcept;

}  // namespace poker_trainer::interrogator
