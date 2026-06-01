#include "bridge/game_launch.hpp"

#include "backbone/game_mode.hpp"
#include "engine/generator.hpp"
#include "engine/rng_seed.hpp"
#include "engine/scenario.hpp"
#include "engine/scenario_id.hpp"

#include <gtest/gtest.h>

#include <optional>

namespace br = poker_trainer::bridge;
namespace bb = poker_trainer::backbone;
namespace eng = poker_trainer::engine;

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
