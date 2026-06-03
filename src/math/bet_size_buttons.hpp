#pragma once

#include "math/interrogator.hpp"

#include "engine/scenario.hpp"

#include <optional>

// Bet Size group (Module 5): a single focus stop of four tier buttons
// (1/3 Pot, 1/2 Pot, Full Pot, Overbet). Selection is a single pick of the
// optimal sizing tier, graded once against the scenario's correct tier.

namespace poker_trainer::interrogator {

// True when the scenario presents a Bet Size group (every Aggressor scenario;
// never a Caller scenario).
[[nodiscard]] bool bet_group_present(const engine::ScenarioState& s) noexcept;

// Map a digit to its BetTier in visual order: 1=1/3, 2=1/2, 3=Full, 4=Overbet.
// nullopt for any digit outside 1..4.
[[nodiscard]] std::optional<engine::BetTier> bet_tier_for_digit(int digit) noexcept;

// Select a bet tier by digit while the group is focused. Returns true when the
// digit 1..4 selected a tier (the keypress is consumed); false otherwise. Per
// Module 5 the global 1-6 box-focus keybinds are suppressed while the group has
// focus, so the caller treats 5/6 as no-ops there -- handled in keybinds.cpp.
bool select_bet_tier_by_digit(BetSizeGroup& group, int digit) noexcept;

// Render the four-button bet-size row for `group` (no-op when not present).
// Selected button fills with accent_primary; when the group holds focus a 2px
// border_focus outline is drawn around the whole row's bounding box (not any
// single button), per Notes -- Keyboard Focus Behavior. Render-only; untested.
void render_bet_size_group(const BetSizeGroup& group);

}  // namespace poker_trainer::interrogator
