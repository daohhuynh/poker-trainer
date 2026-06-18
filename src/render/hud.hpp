#pragma once

#include "engine/scenario.hpp"

#include <string>

struct ImDrawList;

// Zone 08 — HUD overlay (ARCHITECTURE Game Screen). The top-left info stack is the
// denomination legend (drawn by the chip layer), then the pot size, then the
// blinds. The Caller floating bet (call amount) sits near the pushed bet chips.
// Honors Show/Hide HUD: when off, the floating NUMBERS hide (call amount, pot
// total, blinds, opponent stack numbers) while the legend, the pushed-forward
// chips, the cards, and the position labels stay. The toggle UI is Z12's; Z08
// only reads settings.show_hud.

namespace poker_trainer::render {

// Format a whole-dollar amount for display: "$<n>" in Cash mode, "<n> BB" (the
// big-blind multiple) in Big Blinds mode. A non-integer BB multiple shows one
// decimal. Used by the HUD and the opponent stack numbers (Units toggle).
[[nodiscard]] std::string format_amount(int dollars, bool cash_mode, int big_blind);

// Draw the pot size left-aligned at (x, y) in the top-left info stack (only when
// show_hud). Returns the line height advanced.
float draw_pot_size(ImDrawList* dl, float x, float y, const engine::ScenarioState& scenario,
                    bool cash_mode, bool show_hud);

// Draw the blinds left-aligned at (x, y) in the top-left info stack (only when
// show_hud). Returns the line height advanced.
float draw_blinds(ImDrawList* dl, float x, float y, const engine::ScenarioState& scenario,
                  bool cash_mode, bool show_hud);

// Draw the Caller floating bet amount centered at (anchor_x, anchor_y), adjacent
// to the pushed bet chips (only when show_hud). The chips themselves are drawn by
// the chip layer and stay visible regardless.
void draw_floating_bet(ImDrawList* dl, float anchor_x, float anchor_y, int bet_dollars,
                       bool cash_mode, int big_blind, bool show_hud);

}  // namespace poker_trainer::render
