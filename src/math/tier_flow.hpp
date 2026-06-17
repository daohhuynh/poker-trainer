#pragma once

#include "math/interrogator.hpp"

#include <cstdint>

// Multi-tier Aggressor sequential flow (Module 5). When the Bet Sizing Engine is
// on (default), an Aggressor scenario is presented as four FIXED bet sizes
// (tier 1 = 1/3 pot, tier 2 = 1/2 pot, tier 3 = full pot, tier 4 = overbet), one
// tier per screen: the user computes Fold Probability + EV for each tier's given
// size, makes a single persistent Bet Size pick, and Enter advances tier-by-tier,
// submitting on the last tier. Caller and single-tier Aggressor scenarios are NOT
// sequential -- one screen, Enter submits all (unchanged).
//
// This header carries the PURE decision logic (no ImGui, no backbone side
// effects): the gate, the per-tier required-inputs check, and what Enter does. The
// state-mutating advance (re-register the focus list, default focus to Fold
// Probability, carry the Bet Size pick across screens) lives in keybinds.cpp
// alongside the other backbone wiring.

namespace poker_trainer::interrogator {

// True when the active scenario is presented sequentially: a multi-tier (Bet
// Sizing Engine on) Aggressor scenario. False for Caller, single-tier Aggressor,
// and an empty state.
[[nodiscard]] bool is_sequential(const InterrogatorState& state) noexcept;

// True when `current_tier` is the last tier (index kBetTierCount - 1), where Enter
// submits rather than advances.
[[nodiscard]] bool is_last_tier(const InterrogatorState& state) noexcept;

// True when every input REQUIRED to leave the current tier screen is filled: this
// tier's Fold Probability and EV, plus the tier-1 Equity-if-Called (Semi-Bluff).
// The Bet Size pick does NOT gate advancing -- an unpicked size simply grades wrong
// (existing spec), so it is excluded here.
[[nodiscard]] bool current_tier_required_filled(const InterrogatorState& state) noexcept;

// What Enter does from the math-input zone of a sequential scenario.
enum class EnterAction : std::uint8_t {
    None,     // this tier's required inputs are not all filled -> no-op
    Advance,  // advance to the next tier
    Submit,   // last tier -> gather all answers and submit
};

// Resolve Enter's effect for the current tier: None when the required inputs are
// unfilled, else Submit on the last tier and Advance otherwise.
[[nodiscard]] EnterAction enter_action(const InterrogatorState& state) noexcept;

// The next tier index, forward-only: current + 1, clamped at the last tier so the
// user can never return to revise a passed tier. Pure; the backbone re-registration
// is the caller's (advance_tier in keybinds.cpp).
[[nodiscard]] std::uint8_t next_tier(std::uint8_t current) noexcept;

}  // namespace poker_trainer::interrogator
