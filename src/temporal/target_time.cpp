#include "temporal/target_time.hpp"

#include "engine/scenario.hpp"
#include "settings/settings.hpp"

#include <cstdint>

namespace poker_trainer::temporal {

namespace {

// Per-street base target, in milliseconds (Module 6). Pre-flop is 10s for both
// Aggressor and Caller; the betting streets get more. All divisible by 4 so the
// +50% (/2) and +25% (/4) modifiers stay exact integers.
[[nodiscard]] std::uint64_t base_target_ms(engine::Street street) noexcept {
    switch (street) {
        case engine::Street::Preflop: return 10000ULL;
        case engine::Street::Flop:    return 18000ULL;
        case engine::Street::Turn:    return 22000ULL;
        case engine::Street::River:   return 22000ULL;
    }
    return 10000ULL;  // unreachable: the switch above is exhaustive over Street
}

}  // namespace

std::uint64_t compute_target_ms(engine::Street street, bool multi_tier, bool side_pot,
                                bool custom_enabled, std::uint16_t custom_seconds) noexcept {
    if (custom_enabled) {
        // Flat override: the custom value replaces the scaled default outright.
        return static_cast<std::uint64_t>(custom_seconds) * 1000ULL;
    }
    const std::uint64_t base = base_target_ms(street);
    std::uint64_t target = base;
    if (multi_tier) {
        target += base / 2ULL;  // +50% of base
    }
    if (side_pot) {
        target += base / 4ULL;  // +25% of base
    }
    return target;
}

std::uint64_t target_for_scenario(const engine::ScenarioState& scenario,
                                  const settings::GameplaySettings& gameplay) noexcept {
    const bool multi_tier =
        engine::is_aggressor(scenario.type) && gameplay.bet_sizing_engine_enabled;
    return compute_target_ms(scenario.street, multi_tier, scenario.side_pot,
                             gameplay.time_pressure_custom_enabled,
                             gameplay.time_pressure_custom_seconds);
}

}  // namespace poker_trainer::temporal
