#include "bridge/game_launch.hpp"

#include "backbone/game_mode.hpp"
#include "backbone/scenario_events.hpp"
#include "backbone/screen_state.hpp"
#include "engine/generator.hpp"
#include "engine/rng_seed.hpp"
#include "engine/scenario.hpp"
#include "engine/scenario_id.hpp"
#include "settings/settings.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <random>
#include <utility>

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

// The single authoritative active scenario, and the live-settings provider used
// to generate it. Main-thread only (the launch path and consumers all run on the
// browser main thread); no synchronization needed.
std::optional<engine::ScenarioState> g_active_scenario;
std::function<settings::Settings()> g_launch_settings_source;

// The injected Tier-2 required-asset readiness guard (unset -> always Ready) and
// the launch deferred while required assets are still downloading.
std::function<LaunchAssetReadiness()> g_launch_asset_guard;
struct PendingLaunch {
    backbone::GameMode mode;
    std::optional<backbone::CustomConfig> custom;
};
std::optional<PendingLaunch> g_pending_launch;

// The settings the launch generates under: the injected live provider, or the
// documented defaults when none is wired (tests / pre-integration).
[[nodiscard]] settings::Settings launch_settings() {
    return g_launch_settings_source ? g_launch_settings_source() : settings::Settings{};
}

[[nodiscard]] LaunchAssetReadiness launch_asset_readiness() {
    return g_launch_asset_guard ? g_launch_asset_guard() : LaunchAssetReadiness::Ready;
}

// The actual launch: select an id under the mode filter, generate the scenario
// once under the live settings, store it as the single source of truth, fire
// ScenarioSpawned, and transition to Game. Reached only once the required Tier-2
// assets are Ready (directly, or deferred via poll_pending_launch).
void do_launch(backbone::GameMode mode,
               std::optional<backbone::CustomConfig> custom) {
    const engine::ScenarioId id = select_scenario_id(mode, custom, master_rng());

    set_active_scenario(engine::generate_scenario(id, launch_settings()));

    backbone::fire_scenario_spawned(backbone::ScenarioSpawnedEvent{id});

    backbone::set_screen(backbone::ScreenId::Game, id);
    begin_ceremonial_transition_to_game();  // SEAM(Z14)
}

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
    // Tier-2 navigation guard: a required Game asset (table felt, game background,
    // card faces) that fatally failed blocks the launch and shows the Error screen;
    // one still downloading defers the launch until poll_pending_launch completes
    // it. The generator never sees the mode (the seed filter encodes the type).
    switch (launch_asset_readiness()) {
        case LaunchAssetReadiness::Failed:
            g_pending_launch.reset();
            backbone::set_screen(backbone::ScreenId::Error, std::nullopt);
            return;
        case LaunchAssetReadiness::Pending:
            g_pending_launch = PendingLaunch{mode, std::move(custom)};
            return;
        case LaunchAssetReadiness::Ready:
            break;
    }
    do_launch(mode, std::move(custom));
}

void poll_pending_launch() {
    if (!g_pending_launch.has_value()) {
        return;
    }
    switch (launch_asset_readiness()) {
        case LaunchAssetReadiness::Pending:
            return;  // still downloading; keep waiting
        case LaunchAssetReadiness::Failed: {
            g_pending_launch.reset();
            backbone::set_screen(backbone::ScreenId::Error, std::nullopt);
            return;
        }
        case LaunchAssetReadiness::Ready:
            break;
    }
    const PendingLaunch pending = std::move(*g_pending_launch);
    g_pending_launch.reset();
    do_launch(pending.mode, pending.custom);
}

void set_launch_settings_source(std::function<settings::Settings()> source) {
    g_launch_settings_source = std::move(source);
}

void set_launch_asset_guard(std::function<LaunchAssetReadiness()> guard) {
    g_launch_asset_guard = std::move(guard);
}

const engine::ScenarioState* active_scenario() noexcept {
    return g_active_scenario.has_value() ? &*g_active_scenario : nullptr;
}

void set_active_scenario(const engine::ScenarioState& scenario) {
    g_active_scenario = scenario;
}

void reset_game_launch_for_testing() noexcept {
    g_active_scenario.reset();
    g_launch_settings_source = nullptr;
    g_launch_asset_guard = nullptr;
    g_pending_launch.reset();
}

}  // namespace poker_trainer::bridge
