#include "temporal/target_time.hpp"

#include "engine/scenario.hpp"
#include "settings/settings.hpp"

#include <cstdint>

#include <gtest/gtest.h>

namespace tt = poker_trainer::temporal;
namespace eng = poker_trainer::engine;
namespace st = poker_trainer::settings;

namespace {

eng::ScenarioState make_scenario(eng::ScenarioType type, eng::Street street, bool side_pot) {
    eng::ScenarioState s{};
    s.type = type;
    s.street = street;
    s.side_pot = side_pot;
    return s;
}

st::GameplaySettings gameplay(bool bet_sizing, bool custom_enabled, std::uint16_t custom_seconds) {
    st::GameplaySettings g{};
    g.bet_sizing_engine_enabled = bet_sizing;
    g.time_pressure_custom_enabled = custom_enabled;
    g.time_pressure_custom_seconds = custom_seconds;
    return g;
}

// ----- compute_target_ms: street bases (no modifiers) -----

TEST(TargetTime, PreflopBaseIsTenSeconds) {
    EXPECT_EQ(tt::compute_target_ms(eng::Street::Preflop, false, false, false, 0), 10000ULL);
}
TEST(TargetTime, FlopBaseIsEighteenSeconds) {
    EXPECT_EQ(tt::compute_target_ms(eng::Street::Flop, false, false, false, 0), 18000ULL);
}
TEST(TargetTime, TurnBaseIsTwentyTwoSeconds) {
    EXPECT_EQ(tt::compute_target_ms(eng::Street::Turn, false, false, false, 0), 22000ULL);
}
TEST(TargetTime, RiverBaseIsTwentyTwoSeconds) {
    EXPECT_EQ(tt::compute_target_ms(eng::Street::River, false, false, false, 0), 22000ULL);
}

// ----- compute_target_ms: additive modifiers -----

TEST(TargetTime, MultiTierAddsFiftyPercentOfBase) {
    EXPECT_EQ(tt::compute_target_ms(eng::Street::Flop, true, false, false, 0), 27000ULL);
}
TEST(TargetTime, SidePotAddsTwentyFivePercentOfBase) {
    EXPECT_EQ(tt::compute_target_ms(eng::Street::Flop, false, true, false, 0), 22500ULL);
}
TEST(TargetTime, MultiTierAndSidePotStack) {
    EXPECT_EQ(tt::compute_target_ms(eng::Street::Flop, true, true, false, 0), 31500ULL);
}
TEST(TargetTime, PreflopWithBothModifiers) {
    EXPECT_EQ(tt::compute_target_ms(eng::Street::Preflop, true, true, false, 0), 17500ULL);
}

// ----- compute_target_ms: custom override replaces-and-is-flat -----

TEST(TargetTime, CustomReplacesScaledValueAndIgnoresModifiers) {
    EXPECT_EQ(tt::compute_target_ms(eng::Street::River, true, true, true, 30), 30000ULL);
}
TEST(TargetTime, CustomMinimumIsOneSecond) {
    EXPECT_EQ(tt::compute_target_ms(eng::Street::Flop, false, false, true, 1), 1000ULL);
}
TEST(TargetTime, CustomMaximumIsThreeHundredSeconds) {
    EXPECT_EQ(tt::compute_target_ms(eng::Street::Flop, false, false, true, 300), 300000ULL);
}

// ----- target_for_scenario: multi_tier resolution + side pot + custom -----

TEST(TargetForScenario, CallerIsNeverMultiTierEvenWithBetSizingOn) {
    const auto s = make_scenario(eng::ScenarioType::Caller, eng::Street::Flop, false);
    EXPECT_EQ(tt::target_for_scenario(s, gameplay(true, false, 0)), 18000ULL);
}
TEST(TargetForScenario, AggressorWithBetSizingIsMultiTier) {
    const auto s = make_scenario(eng::ScenarioType::AggressorValueBet, eng::Street::Flop, false);
    EXPECT_EQ(tt::target_for_scenario(s, gameplay(true, false, 0)), 27000ULL);
}
TEST(TargetForScenario, AggressorWithBetSizingOffIsSingleTier) {
    const auto s = make_scenario(eng::ScenarioType::AggressorValueBet, eng::Street::Flop, false);
    EXPECT_EQ(tt::target_for_scenario(s, gameplay(false, false, 0)), 18000ULL);
}
TEST(TargetForScenario, AggressorSidePotStacksBothModifiers) {
    const auto s = make_scenario(eng::ScenarioType::AggressorSemiBluff, eng::Street::Turn, true);
    EXPECT_EQ(tt::target_for_scenario(s, gameplay(true, false, 0)), 38500ULL);  // 22000 + 11000 + 5500
}
TEST(TargetForScenario, CustomOverridesRegardlessOfScenario) {
    const auto s = make_scenario(eng::ScenarioType::AggressorSemiBluff, eng::Street::Turn, true);
    EXPECT_EQ(tt::target_for_scenario(s, gameplay(true, true, 45)), 45000ULL);
}

}  // namespace
