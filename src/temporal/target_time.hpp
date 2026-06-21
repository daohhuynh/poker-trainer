#pragma once

#include "engine/scenario.hpp"
#include "settings/settings.hpp"

#include <cstdint>

// Zone 10 (Temporal) — per-scenario target-time computation (Module 6).
//
// Pure functions of a few primitives so the street table, the additive modifiers,
// and the custom override are unit-testable without the engine, the clock, or
// ImGui. The Delta Timer (delta_timer.hpp) computes a scenario's target once at
// timer_start via target_for_scenario().

namespace poker_trainer::temporal {

// Compute a scenario's target time in integer milliseconds.
//
// Default = a per-street base, with additive modifiers expressed as a percentage of
// that base: +50% when the scenario is multi-tier (an Aggressor with the Bet Sizing
// Engine on), +25% when it is a side pot. Bases are divisible by 4, so /2 and /4
// stay exact-integer (no floats).
//
// A set custom flat time REPLACES the scaled value entirely and is flat: no street
// base, no +50% / +25% modifiers. custom_seconds is already clamped to 1..300 by
// the settings validator.
[[nodiscard]] std::uint64_t compute_target_ms(engine::Street street,
                                              bool multi_tier,
                                              bool side_pot,
                                              bool custom_enabled,
                                              std::uint16_t custom_seconds) noexcept;

// Resolve compute_target_ms's inputs from a scenario and the live gameplay
// settings: multi_tier = is_aggressor(type) && bet_sizing_engine_enabled (a Caller
// is never multi-tier); side_pot from the scenario; the custom fields from settings.
[[nodiscard]] std::uint64_t target_for_scenario(
    const engine::ScenarioState& scenario,
    const settings::GameplaySettings& gameplay) noexcept;

}  // namespace poker_trainer::temporal
