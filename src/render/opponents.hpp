#pragma once

#include "render/chips.hpp"
#include "render/layout.hpp"

#include "engine/scenario.hpp"

#include <cstdint>
#include <span>

struct ImDrawList;

// Zone 08 — opponents around the table (ARCHITECTURE Game Screen). Position
// labels (UTG/HJ/CO/BTN/SB/BB) at each seat and the opponent's effective stack as
// a greedy chip-column cluster on the felt (the SAME decomposition the pot uses),
// plus a floating HUD amount (Cash or BB) above it. Also the scenario-type visual
// state: Caller pushes chips forward + a floating bet; Aggressor leaves the chip
// area empty (the pot reflects only blinds/antes). No text labels announce the
// type — the table state is the cue.

namespace poker_trainer::render {

// The opponent's chip area state, derived from the scenario type alone.
enum class OpponentChipState : std::uint8_t {
    PushedForward = 0,  // Caller: the active opponent has bet; chips sit forward.
    Empty = 1,          // Aggressor: opponent has not acted; chip area is empty.
};

// Standard 6-max seat abbreviation for a position.
[[nodiscard]] const char* position_abbrev(engine::Position position) noexcept;

// A deterministic, per-seat COSMETIC display stack for an opponent. The engine
// models a SINGLE effective stack for the implied-odds math (ARCHITECTURE Module 1
// "Stack Size Generator"); opponent stack precision does not enter grading
// (ARCHITECTURE: "the math drill does not depend on opponent stack precision"), so
// each opponent shows its own varied stack derived from the scenario id + seat —
// realistic and reproducible per id, without touching the seed-locked model or the
// graded math (which keeps using scenario.effective_stack). Returned as a round
// big-blind multiple (whole dollars). Pure (unit-tested); never read by the engine.
[[nodiscard]] int opponent_display_stack(const engine::ScenarioState& scenario,
                                         engine::Position seat) noexcept;

// Map the scenario type to the opponent chip-area state (Caller -> PushedForward,
// any Aggressor sub-type -> Empty).
[[nodiscard]] OpponentChipState opponent_chip_state(engine::ScenarioType type) noexcept;

// The seat slot of the active opponent whose bet is pushed forward in a Caller
// scenario. Deterministic (the engine does not name a specific bettor): the seat
// directly across the oval from the hero, so the push reads clearly toward the
// pot. Meaningful only for Caller scenarios.
[[nodiscard]] int active_opponent_slot() noexcept;

// ----- Render (game render TU only) -----

// Draw every seat's position label (always) and, for non-hero seats, the opponent's
// effective stack as a greedy chip-column cluster on the felt over `denom_set`
// (the active legend denominations) plus a floating HUD amount above it (only when
// show_hud). The hero seat (slot 0) shows its label only; its hole cards are drawn
// separately at the bottom-right nearest the camera.
void draw_opponent_seats(ImDrawList* dl, const GameLayout& layout,
                         const engine::ScenarioState& scenario,
                         std::span<const Denomination> denom_set, bool cash_mode, bool show_hud);

// Mark the all-in side-pot source seat: a colored ring around the seat slot and
// an "ALL-IN" label. Drawn for side-pot scenarios on the active opponent slot,
// whose chip area is left empty (their whole stack is in the pot).
void draw_all_in_marker(ImDrawList* dl, const GameLayout& layout, int slot);

}  // namespace poker_trainer::render
