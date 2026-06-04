#include "bridge/game_launch.hpp"

#include "backbone/game_mode.hpp"
#include "backbone/scenario_events.hpp"
#include "backbone/screen_state.hpp"
#include "engine/generator.hpp"
#include "engine/rng_seed.hpp"
#include "engine/scenario.hpp"
#include "engine/scenario_id.hpp"

#include <gtest/gtest.h>

#include <optional>

namespace br = poker_trainer::bridge;
namespace bb = poker_trainer::backbone;
namespace eng = poker_trainer::engine;
namespace st = poker_trainer::settings;

namespace {
constexpr int kDraws = 2000;
}  // namespace

TEST(RejectLoop, StandardAcceptsAnyAndSeesBothTypes) {
    eng::RngEngine rng{12345ULL};
    int aggressor = 0;
    int caller = 0;
    for (int i = 0; i < kDraws; ++i) {
        const eng::ScenarioId id =
            br::select_scenario_id(bb::GameMode::Standard, std::nullopt, rng);
        ASSERT_TRUE(eng::is_valid(id));
        if (eng::is_aggressor(eng::peek_type(id))) {
            ++aggressor;
        } else {
            ++caller;
        }
    }
    // No filter: both types appear (proving Standard does not constrain type).
    EXPECT_GT(aggressor, 0);
    EXPECT_GT(caller, 0);
}

TEST(RejectLoop, AggressorSelectionsAreAllAggressor) {
    eng::RngEngine rng{777ULL};
    for (int i = 0; i < kDraws; ++i) {
        const eng::ScenarioId id =
            br::select_scenario_id(bb::GameMode::Aggressor, std::nullopt, rng);
        ASSERT_TRUE(eng::is_valid(id));
        EXPECT_TRUE(eng::is_aggressor(eng::peek_type(id)));
    }
}

TEST(RejectLoop, CallerSelectionsAreAllCaller) {
    eng::RngEngine rng{2024ULL};
    for (int i = 0; i < kDraws; ++i) {
        const eng::ScenarioId id =
            br::select_scenario_id(bb::GameMode::Caller, std::nullopt, rng);
        ASSERT_TRUE(eng::is_valid(id));
        EXPECT_EQ(eng::peek_type(id), eng::ScenarioType::Caller);
    }
}

TEST(RejectLoop, CustomYieldsOnlyTwoSidesAndHonorsSplit) {
    // 70% Aggressor / 30% Caller.
    eng::RngEngine rng{99887766ULL};
    const bb::CustomConfig cfg{/*aggressor_weight=*/70, /*caller_weight=*/30};
    int aggressor = 0;
    int caller = 0;
    for (int i = 0; i < kDraws; ++i) {
        const eng::ScenarioId id =
            br::select_scenario_id(bb::GameMode::Custom, cfg, rng);
        ASSERT_TRUE(eng::is_valid(id));
        const eng::ScenarioType type = eng::peek_type(id);
        // Every scenario type is exactly one of Aggressor or Caller.
        if (eng::is_aggressor(type)) {
            ++aggressor;
        } else {
            EXPECT_EQ(type, eng::ScenarioType::Caller);
            ++caller;
        }
    }
    // Both sides appear.
    EXPECT_GT(aggressor, 0);
    EXPECT_GT(caller, 0);
    // The split roughly honors the configured weights (generous tolerance; the
    // binomial std-dev over 2000 draws is ~1pp).
    const double aggressor_fraction = static_cast<double>(aggressor) / kDraws;
    EXPECT_NEAR(aggressor_fraction, 0.70, 0.07);
}

TEST(RejectLoop, CustomAllAggressorWeightNeverDrawsCaller) {
    eng::RngEngine rng{4242ULL};
    const bb::CustomConfig cfg{/*aggressor_weight=*/100, /*caller_weight=*/0};
    for (int i = 0; i < kDraws; ++i) {
        const eng::ScenarioId id =
            br::select_scenario_id(bb::GameMode::Custom, cfg, rng);
        EXPECT_TRUE(eng::is_aggressor(eng::peek_type(id)));
    }
}

TEST(RejectLoop, CustomAllCallerWeightNeverDrawsAggressor) {
    eng::RngEngine rng{5353ULL};
    const bb::CustomConfig cfg{/*aggressor_weight=*/0, /*caller_weight=*/100};
    for (int i = 0; i < kDraws; ++i) {
        const eng::ScenarioId id =
            br::select_scenario_id(bb::GameMode::Custom, cfg, rng);
        EXPECT_EQ(eng::peek_type(id), eng::ScenarioType::Caller);
    }
}

// ----- request_game_launch: generate -> store -> fire scenario_spawned -----

class LaunchTest : public ::testing::Test {
protected:
    void SetUp() override {
        bb::reset_screen_state_for_testing();
        bb::reset_scenario_events_for_testing();
        br::reset_game_launch_for_testing();
    }
    void TearDown() override {
        bb::reset_screen_state_for_testing();
        bb::reset_scenario_events_for_testing();
        br::reset_game_launch_for_testing();
    }
};

TEST_F(LaunchTest, StoresActiveScenarioAndFiresSpawnedThenEntersGame) {
    bool fired = false;
    eng::ScenarioId fired_id{};
    const bb::SubscriberHandle h = bb::subscribe_scenario_spawned(
        [&](const bb::ScenarioSpawnedEvent& ev) {
            fired = true;
            fired_id = ev.scenario_id;
        },
        "test");

    EXPECT_EQ(br::active_scenario(), nullptr);  // nothing launched yet
    br::request_game_launch(bb::GameMode::Caller, std::nullopt);

    // The single authoritative state exists, is a Caller (the launched mode), and
    // its id agrees with the screen state and the fired event — one id, one state.
    const eng::ScenarioState* active = br::active_scenario();
    ASSERT_NE(active, nullptr);
    EXPECT_EQ(active->type, eng::ScenarioType::Caller);

    const bb::ScreenStateSnapshot snap = bb::read_screen_state();
    EXPECT_EQ(snap.current, bb::ScreenId::Game);
    ASSERT_TRUE(snap.active_scenario.has_value());
    EXPECT_EQ(*snap.active_scenario, active->id);

    EXPECT_TRUE(fired);
    EXPECT_EQ(fired_id, active->id);
    bb::unsubscribe(h);
}

TEST_F(LaunchTest, LaunchSettingsSourceFeedsGeneration) {
    // The bet-sizing toggle (a generation input) flows through the injected
    // settings source: off -> the Aggressor scenario is single-tier.
    br::set_launch_settings_source([] {
        st::Settings s{};
        s.gameplay.bet_sizing_engine_enabled = false;
        return s;
    });
    br::request_game_launch(bb::GameMode::Aggressor, std::nullopt);
    const eng::ScenarioState* active = br::active_scenario();
    ASSERT_NE(active, nullptr);
    EXPECT_TRUE(eng::is_aggressor(active->type));
    EXPECT_FALSE(active->multi_tier);  // proves the injected settings reached generate_scenario
}

TEST_F(LaunchTest, SetActiveScenarioRoundTrips) {
    eng::ScenarioState s{};
    s.id = eng::ScenarioId{321};
    s.type = eng::ScenarioType::Caller;
    s.caller_pot_odds_pct = 99.0;  // a sentinel no generated Caller produces
    br::set_active_scenario(s);
    const eng::ScenarioState* active = br::active_scenario();
    ASSERT_NE(active, nullptr);
    EXPECT_EQ(active->id, eng::ScenarioId{321});
    EXPECT_DOUBLE_EQ(active->caller_pot_odds_pct, 99.0);
}
