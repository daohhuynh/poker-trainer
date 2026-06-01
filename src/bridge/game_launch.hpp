#pragma once

#include "backbone/game_mode.hpp"
#include "engine/rng_seed.hpp"
#include "engine/scenario_id.hpp"

#include <optional>

// The mode -> scenario-id seam (the master ID stream + reject loop).
//
// request_game_launch is the entry point Zone 07 calls when the user picks a
// launch option on Mode Selection (STANDARD / Aggressor / Caller, or Play in the
// Custom popup). It owns the master id stream: it draws candidate 64-bit ids,
// asks the engine for each id's seed-locked type (engine::peek_type), and
// accepts one per the mode's filter, then transitions the screen state toward
// Game. The mode is a filter over candidate seeds — it is NEVER passed into the
// generator (see backbone::GameMode).
//
//   Standard  -> accept any id (the ~50/50 mix is the natural type distribution)
//   Aggressor -> accept only is_aggressor(peek_type(id))
//   Caller    -> accept only peek_type(id) == Caller
//   Custom    -> draw an Aggressor/Caller side per the split weights, then
//                accept the next id whose type matches that side
//
// The ceremonial Mode Selection -> Game transition animation is Zone 14's; Z05
// performs only the state transition here (see the SEAM(Z14) stub in the .cpp).

namespace poker_trainer::bridge {

// Maximum candidate draws per launch before falling back to the last candidate.
// Far above what any non-degenerate type distribution needs: even a type that
// is only ~1% of the seed space is found within this cap with overwhelming
// probability. The fallback guarantees the loop always terminates.
inline constexpr int kMaxSelectAttempts = 4096;

// Draw a scenario id matching the mode filter, using the provided RNG. Pure and
// deterministic for a given RNG state, so tests can seed it and assert the
// filter holds. `custom` is consulted only for GameMode::Custom (defaulting to
// 50/50 if absent); the other modes ignore it.
[[nodiscard]] engine::ScenarioId select_scenario_id(
    backbone::GameMode mode,
    std::optional<backbone::CustomConfig> custom,
    engine::RngEngine& rng) noexcept;

// Launch a game in the given mode: select a matching scenario id from the master
// stream and transition the screen state toward Game with that id. Custom mode
// honors the supplied split weights.
void request_game_launch(backbone::GameMode mode,
                         std::optional<backbone::CustomConfig> custom);

}  // namespace poker_trainer::bridge
