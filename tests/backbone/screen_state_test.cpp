#include "backbone/screen_state.hpp"

#include "engine/scenario_id.hpp"

#include <gtest/gtest.h>

namespace bb = poker_trainer::backbone;
namespace eng = poker_trainer::engine;

class ScreenStateTest : public ::testing::Test {
 protected:
    void SetUp() override { bb::reset_screen_state_for_testing(); }
    void TearDown() override { bb::reset_screen_state_for_testing(); }
};

TEST_F(ScreenStateTest, DefaultsToRootWithNoScenario) {
    const auto snap = bb::read_screen_state();
    EXPECT_EQ(snap.current, bb::ScreenId::Root);
    EXPECT_FALSE(snap.active_scenario.has_value());
    EXPECT_FALSE(bb::is_in_scenario());
}

TEST_F(ScreenStateTest, GameRetainsActiveScenario) {
    bb::set_screen(bb::ScreenId::Game, eng::ScenarioId{12345});
    const auto snap = bb::read_screen_state();
    EXPECT_EQ(snap.current, bb::ScreenId::Game);
    ASSERT_TRUE(snap.active_scenario.has_value());
    EXPECT_EQ(*snap.active_scenario, eng::ScenarioId{12345});
    EXPECT_TRUE(bb::is_in_scenario());
}

TEST_F(ScreenStateTest, PostRoundRetainsActiveScenario) {
    bb::set_screen(bb::ScreenId::PostRound, eng::ScenarioId{999});
    const auto snap = bb::read_screen_state();
    EXPECT_EQ(snap.current, bb::ScreenId::PostRound);
    ASSERT_TRUE(snap.active_scenario.has_value());
    EXPECT_EQ(*snap.active_scenario, eng::ScenarioId{999});
    EXPECT_TRUE(bb::is_in_scenario());
}

TEST_F(ScreenStateTest, NonScenarioScreenClearsActiveScenario) {
    bb::set_screen(bb::ScreenId::Game, eng::ScenarioId{12345});
    // Transitioning to a non-scenario screen clears the scenario even if a
    // value is (wrongly) passed.
    bb::set_screen(bb::ScreenId::ModeSelection, eng::ScenarioId{12345});
    const auto snap = bb::read_screen_state();
    EXPECT_EQ(snap.current, bb::ScreenId::ModeSelection);
    EXPECT_FALSE(snap.active_scenario.has_value());
    EXPECT_FALSE(bb::is_in_scenario());
}

TEST_F(ScreenStateTest, ErrorScreenIsRepresentable) {
    bb::set_screen(bb::ScreenId::Error, std::nullopt);
    EXPECT_EQ(bb::read_screen_state().current, bb::ScreenId::Error);
    EXPECT_FALSE(bb::is_in_scenario());
}

TEST_F(ScreenStateTest, TutorialStateIsOrthogonalAndSurvivesTransition) {
    bb::set_tutorial_state(bb::TutorialState{bb::TutorialPhase::Active, 3});
    bb::set_screen(bb::ScreenId::Game, eng::ScenarioId{777});
    const auto snap = bb::read_screen_state();
    EXPECT_EQ(snap.current, bb::ScreenId::Game);
    EXPECT_EQ(snap.tutorial_state.phase, bb::TutorialPhase::Active);
    EXPECT_EQ(snap.tutorial_state.active_step, 3);
    ASSERT_TRUE(snap.active_scenario.has_value());
    EXPECT_EQ(*snap.active_scenario, eng::ScenarioId{777});
}
