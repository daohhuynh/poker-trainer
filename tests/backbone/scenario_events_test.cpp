#include "backbone/scenario_events.hpp"

#include "engine/scenario_id.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace bb = poker_trainer::backbone;
namespace eng = poker_trainer::engine;

class ScenarioEventsTest : public ::testing::Test {
 protected:
    void SetUp() override { bb::reset_scenario_events_for_testing(); }
    void TearDown() override { bb::reset_scenario_events_for_testing(); }
};

TEST_F(ScenarioEventsTest, SubscribeThenFireDelivers) {
    bool received = false;
    eng::ScenarioId got{0};
    bb::subscribe_scenario_spawned(
        [&](const bb::ScenarioSpawnedEvent& e) {
            received = true;
            got = e.scenario_id;
        },
        "sub");
    bb::fire_scenario_spawned({eng::ScenarioId{99}});
    EXPECT_TRUE(received);
    EXPECT_EQ(got, eng::ScenarioId{99});
}

TEST_F(ScenarioEventsTest, FanOutToAllSubscribersInRegistrationOrder) {
    std::vector<int> order;
    bb::subscribe_scenario_spawned(
        [&](const bb::ScenarioSpawnedEvent&) { order.push_back(0); }, "first");
    bb::subscribe_scenario_spawned(
        [&](const bb::ScenarioSpawnedEvent&) { order.push_back(1); }, "second");
    bb::subscribe_scenario_spawned(
        [&](const bb::ScenarioSpawnedEvent&) { order.push_back(2); }, "third");

    bb::fire_scenario_spawned({eng::ScenarioId{1}});
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 0);
    EXPECT_EQ(order[1], 1);
    EXPECT_EQ(order[2], 2);
}

TEST_F(ScenarioEventsTest, UnsubscribeStopsDelivery) {
    int count = 0;
    const auto handle = bb::subscribe_scenario_spawned(
        [&](const bb::ScenarioSpawnedEvent&) { ++count; }, "sub");
    bb::fire_scenario_spawned({eng::ScenarioId{1}});
    EXPECT_EQ(count, 1);
    bb::unsubscribe(handle);
    bb::fire_scenario_spawned({eng::ScenarioId{2}});
    EXPECT_EQ(count, 1);  // no further delivery
}

TEST_F(ScenarioEventsTest, EventTypesAreIsolated) {
    bool spawned_seen = false;
    bool submitted_seen = false;
    bb::subscribe_scenario_spawned(
        [&](const bb::ScenarioSpawnedEvent&) { spawned_seen = true; }, "spawn");
    bb::subscribe_answers_submitted(
        [&](const bb::AnswersSubmittedEvent&) { submitted_seen = true; }, "submit");

    bb::fire_scenario_spawned({eng::ScenarioId{1}});
    EXPECT_TRUE(spawned_seen);
    EXPECT_FALSE(submitted_seen);

    spawned_seen = false;
    bb::fire_answers_submitted({eng::ScenarioId{1}});
    EXPECT_FALSE(spawned_seen);
    EXPECT_TRUE(submitted_seen);
}

TEST_F(ScenarioEventsTest, GradingCompleteCarriesPayload) {
    bool passed = false;
    std::uint32_t elapsed = 0;
    bb::subscribe_grading_complete(
        [&](const bb::GradingCompleteEvent& e) {
            passed = e.passed;
            elapsed = e.elapsed_ms;
        },
        "grade");
    bb::fire_grading_complete({eng::ScenarioId{5}, /*passed=*/true, /*elapsed_ms=*/4200});
    EXPECT_TRUE(passed);
    EXPECT_EQ(elapsed, 4200u);
}
