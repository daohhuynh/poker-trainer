#pragma once

#include "backbone/game_mode.hpp"
#include "engine/rng_seed.hpp"
#include "engine/scenario.hpp"
#include "engine/scenario_id.hpp"
#include "settings/settings.hpp"

#include <cstdint>
#include <functional>
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
// stream, generate the ScenarioState once (with the live settings), store it as
// the single authoritative active scenario, fire scenario_spawned, then transition
// the screen state toward Game with that id. Custom mode honors the supplied split
// weights.
void request_game_launch(backbone::GameMode mode,
                         std::optional<backbone::CustomConfig> custom);

// ----- Live-settings source (the launch's generation input) -----

// Inject the app's live-settings provider, used by request_game_launch to
// generate the scenario. Boot wires this to the same provider it injects into
// Zone 09 (InterrogatorRuntime::settings_source) so both read one source of
// truth for settings. Unset -> Settings{} defaults (keeps the pure bridge lib
// self-contained for tests).
void set_launch_settings_source(std::function<settings::Settings()> source);

// ----- Tier-2 required-asset navigation guard (ARCHITECTURE Module 3 Tier 2) ---
//
// The Game screen cannot render its table without certain Tier-2 PNGs (the table
// felt, game background, and card faces). Tier 2 loads in the background after
// Root renders, so when the user clicks Play one of three things is true of the
// required assets:

// The readiness of the required Tier-2 assets at launch time.
enum class LaunchAssetReadiness : std::uint8_t {
    // All required assets are Loaded -> launch immediately.
    Ready = 0,
    // None failed, but some are still downloading -> defer the launch and
    // complete it from poll_pending_launch once they resolve (the spec's
    // "finish loading needed Tier 2 assets and then transition").
    Pending = 1,
    // A required asset fatally failed -> block the navigation and show the Error
    // screen instead of launching (the spec's deferred Tier-2 failure handling).
    Failed = 2,
};

// Inject the readiness guard. request_game_launch (and poll_pending_launch)
// consult it before transitioning to Game. Unset -> always Ready, so the pure
// bridge library and its tests launch synchronously with no asset registry.
void set_launch_asset_guard(std::function<LaunchAssetReadiness()> guard);

// Drive a deferred launch. If a launch is pending (the guard reported Pending
// when the user clicked Play), re-evaluate readiness and either complete the
// launch (Ready), surface the Error screen (Failed), or keep waiting (Pending).
// A no-op when no launch is pending. The bridge calls this once per frame.
void poll_pending_launch();

// ----- Single authoritative active scenario -----
//
// The active ScenarioState is generated exactly once at launch and stored here.
// Every consumer (Z08 render, Z09 grading, Z13 recap) reads it instead of
// regenerating from the seed, so the whole app shares one state under one set of
// settings (the ScenarioSpawned bus event carries only the id; the full state
// lives here).

// The authoritative active ScenarioState, or nullptr when none has been launched.
[[nodiscard]] const engine::ScenarioState* active_scenario() noexcept;

// Replace the authoritative active scenario. Called by request_game_launch (and
// any future transition/replay path that produces a fresh state); also used by
// tests that drive consumers without going through the full launch.
void set_active_scenario(const engine::ScenarioState& scenario);

// Clear the active scenario + the injected settings source. Used by tests.
void reset_game_launch_for_testing() noexcept;

}  // namespace poker_trainer::bridge
