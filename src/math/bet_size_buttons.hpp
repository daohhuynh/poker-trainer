#pragma once

#include "math/interrogator.hpp"

#include "engine/scenario.hpp"

#include <cstdint>
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

// The display label for a bet-size tier ("1/3 Pot", "1/2 Pot", "Full Pot",
// "Overbet"). Used both on the four buttons and in the multi-tier per-tier size
// indicator ("Tier 2 of 4 - Bet: 1/2 Pot").
[[nodiscard]] const char* bet_tier_label(engine::BetTier tier) noexcept;

// Select a bet tier by digit while the group is focused. Returns true when the
// digit 1..4 selected a tier (the keypress is consumed); false otherwise. Per
// Module 5 the global 1-6 box-focus keybinds are suppressed while the group has
// focus, so the caller treats 5/6 as no-ops there -- handled in keybinds.cpp.
bool select_bet_tier_by_digit(BetSizeGroup& group, int digit) noexcept;

// Mouse click on a bet-size button: select `tier` (identical to the 1-4 keys) and
// move the focus_manager outline onto the group, activating keyboard mode. This
// is the previously-missing mouse->select path; the render loop calls it when a
// button reports a click, giving clicks the same visual feedback as the keys.
void select_bet_tier_on_click(BetSizeGroup& group, engine::BetTier tier) noexcept;

// Render the four-button bet-size row for `group` (no-op when not present).
// Selected button fills with accent_primary; when the group holds focus the shared
// substrate draws a 2px ring (`ring_color` = the border_focus token, resolved by
// the caller) around the whole row's bounding box (not any single button), per
// Notes -- Keyboard Focus Behavior. A button click selects its tier
// (select_bet_tier_on_click). Render-only; untested.
void render_bet_size_group(BetSizeGroup& group, std::uint32_t ring_color);

}  // namespace poker_trainer::interrogator
