#include "bridge/game_launch.hpp"

#include "backbone/game_mode.hpp"
#include "backbone/screen_state.hpp"
#include "engine/generator.hpp"
#include "engine/rng_seed.hpp"
#include "engine/scenario.hpp"
#include "engine/scenario_id.hpp"

#include <cstdint>
#include <optional>
#include <random>

namespace poker_trainer::bridge {

namespace {

// Draw a uniformly-random non-zero 64-bit candidate id.
[[nodiscard]] engine::ScenarioId draw_candidate(engine::RngEngine& rng) noexcept {
    std::uniform_int_distribution<std::uint64_t> dist(
        engine::kMinScenarioIdValue, engine::kMaxScenarioIdValue);
    return engine::ScenarioId{dist(rng)};
}

// For Custom: draw the desired Aggressor/Caller side once per launch so the
// configured split is honored regardless of the natural type distribution.
// Returns true for the Aggressor side.
[[nodiscard]] bool draw_aggressor_side(
    const std::optional<backbone::CustomConfig>& custom,
    engine::RngEngine& rng) noexcept {
    const backbone::CustomConfig cfg = custom.value_or(backbone::CustomConfig{});
    std::uniform_int_distribution<int> dist(0, 99);
    return dist(rng) < static_cast<int>(cfg.aggressor_weight);
}

// Does an id's seed-locked type satisfy the mode filter?
[[nodiscard]] bool accepts(backbone::GameMode mode, bool want_aggressor,
                           engine::ScenarioType type) noexcept {
    switch (mode) {
        case backbone::GameMode::Standard:
            return true;
        case backbone::GameMode::Aggressor:
            return engine::is_aggressor(type);
        case backbone::GameMode::Caller:
            return type == engine::ScenarioType::Caller;
        case backbone::GameMode::Custom:
            return want_aggressor ? engine::is_aggressor(type)
                                  : (type == engine::ScenarioType::Caller);
    }
    return true;  // Unreachable; the switch is exhaustive.
}

// The process-wide master id stream. Seeded once from a non-deterministic
// source: fresh launches are intentionally random (only saved/shared ids are
// deterministic). Main-thread only.
[[nodiscard]] engine::RngEngine& master_rng() {
    static engine::RngEngine engine_state = [] {
        std::random_device rd;
        const std::uint64_t hi = static_cast<std::uint64_t>(rd());
        const std::uint64_t lo = static_cast<std::uint64_t>(rd());
        return engine::RngEngine{(hi << 32) ^ lo};
    }();
    return engine_state;
}

// SEAM(Z14): the ceremonial Mode Selection -> Game transition animation. Zone 14
// wires the fade/slide; Z05 performs only the screen-state transition. No-op
// until Z14 lands.
void begin_ceremonial_transition_to_game() noexcept {}

}  // namespace

engine::ScenarioId select_scenario_id(
    backbone::GameMode mode,
    std::optional<backbone::CustomConfig> custom,
    engine::RngEngine& rng) noexcept {
    const bool want_aggressor = (mode == backbone::GameMode::Custom)
                                    ? draw_aggressor_side(custom, rng)
                                    : false;
    engine::ScenarioId last{engine::kMinScenarioIdValue};
    for (int attempt = 0; attempt < kMaxSelectAttempts; ++attempt) {
        const engine::ScenarioId candidate = draw_candidate(rng);
        last = candidate;
        if (accepts(mode, want_aggressor, engine::peek_type(candidate))) {
            return candidate;
        }
    }
    // Attempt cap exhausted (astronomically unlikely): fall back to the last
    // candidate so the launch always terminates with a usable id.
    return last;
}

void request_game_launch(backbone::GameMode mode,
                         std::optional<backbone::CustomConfig> custom) {
    const engine::ScenarioId id = select_scenario_id(mode, custom, master_rng());
    backbone::set_screen(backbone::ScreenId::Game, id);
    begin_ceremonial_transition_to_game();  // SEAM(Z14)
}

}  // namespace poker_trainer::bridge
