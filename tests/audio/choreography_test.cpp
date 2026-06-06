// Zone 03 spawn-choreography decision + scheduler: which SFX fire when, at what
// gain, and that a fresh spawn supersedes an unfired prior schedule. The 400ms
// delay is asserted as data here; at runtime it is driven by the animation clock.

#include "audio/choreography.hpp"

#include "audio/audio_paths.hpp"

#include "engine/scenario.hpp"

#include <gtest/gtest.h>

namespace {

using poker_trainer::audio::ChoreographyScheduler;
using poker_trainer::audio::choreograph;
using poker_trainer::audio::kChipCueGain;
using poker_trainer::audio::kChoreographyDelayMs;
using poker_trainer::audio::ScheduledSfx;
using poker_trainer::audio::SfxId;
using poker_trainer::engine::ScenarioType;

TEST(Choreograph, CallerSchedulesDealThenChipPush) {
    const auto schedule = choreograph(ScenarioType::Caller, /*side_pot=*/false, 1000);
    ASSERT_EQ(schedule.size(), 2u);
    EXPECT_EQ(schedule[0].id, SfxId::CardDeal);
    EXPECT_EQ(schedule[0].fire_at_ms, 1000u);
    EXPECT_FLOAT_EQ(schedule[0].gain, 1.0f);
    EXPECT_EQ(schedule[1].id, SfxId::ChipPush);
    EXPECT_EQ(schedule[1].fire_at_ms, 1000u + kChoreographyDelayMs);
    EXPECT_FLOAT_EQ(schedule[1].gain, kChipCueGain);
}

TEST(Choreograph, SidePotSchedulesDealThenSplit) {
    for (ScenarioType type :
         {ScenarioType::Caller, ScenarioType::AggressorValueBet}) {
        const auto schedule = choreograph(type, /*side_pot=*/true, 500);
        ASSERT_EQ(schedule.size(), 2u);
        EXPECT_EQ(schedule[0].id, SfxId::CardDeal);
        EXPECT_EQ(schedule[1].id, SfxId::SidePotSplit);
        EXPECT_EQ(schedule[1].fire_at_ms, 500u + kChoreographyDelayMs);
        EXPECT_FLOAT_EQ(schedule[1].gain, kChipCueGain);
    }
}

TEST(Choreograph, SidePotTakesPrecedenceOverCallerChipPush) {
    const auto schedule = choreograph(ScenarioType::Caller, /*side_pot=*/true, 0);
    ASSERT_EQ(schedule.size(), 2u);
    EXPECT_EQ(schedule[1].id, SfxId::SidePotSplit);  // not ChipPush
}

TEST(Choreograph, AggressorNonSidePotSchedulesDealOnly) {
    for (ScenarioType type : {ScenarioType::AggressorPureBluff,
                              ScenarioType::AggressorValueBet,
                              ScenarioType::AggressorSemiBluff}) {
        const auto schedule = choreograph(type, /*side_pot=*/false, 2000);
        ASSERT_EQ(schedule.size(), 1u);
        EXPECT_EQ(schedule[0].id, SfxId::CardDeal);
        EXPECT_FLOAT_EQ(schedule[0].gain, 1.0f);
    }
}

TEST(ChoreographyScheduler, FiresEntriesAtTheirTime) {
    ChoreographyScheduler scheduler;
    scheduler.schedule(ScenarioType::Caller, /*side_pot=*/false, 1000);
    EXPECT_EQ(scheduler.pending_count(), 2u);

    const auto at_spawn = scheduler.poll(1000);
    ASSERT_EQ(at_spawn.size(), 1u);
    EXPECT_EQ(at_spawn[0].id, SfxId::CardDeal);

    EXPECT_TRUE(scheduler.poll(1399).empty());  // chip push not due yet

    const auto at_delay = scheduler.poll(1400);
    ASSERT_EQ(at_delay.size(), 1u);
    EXPECT_EQ(at_delay[0].id, SfxId::ChipPush);
    EXPECT_EQ(scheduler.pending_count(), 0u);
}

TEST(ChoreographyScheduler, PollReturnsBothWhenBothDue) {
    ChoreographyScheduler scheduler;
    scheduler.schedule(ScenarioType::Caller, /*side_pot=*/false, 1000);
    const auto due = scheduler.poll(5000);  // both fire times have passed
    ASSERT_EQ(due.size(), 2u);
    EXPECT_EQ(due[0].id, SfxId::CardDeal);   // in fire order
    EXPECT_EQ(due[1].id, SfxId::ChipPush);
    EXPECT_EQ(scheduler.pending_count(), 0u);
}

TEST(ChoreographyScheduler, NewScenarioSupersedesPriorSchedule) {
    ChoreographyScheduler scheduler;
    scheduler.schedule(ScenarioType::Caller, /*side_pot=*/false, 1000);
    // A new scenario spawns before the first one's cues fired.
    scheduler.schedule(ScenarioType::AggressorValueBet, /*side_pot=*/false, 2000);
    EXPECT_EQ(scheduler.pending_count(), 1u);  // aggressor non-side-pot -> deal only
    EXPECT_TRUE(scheduler.poll(1999).empty());  // the old 1000ms deal is gone
    const auto due = scheduler.poll(2000);
    ASSERT_EQ(due.size(), 1u);
    EXPECT_EQ(due[0].id, SfxId::CardDeal);
    EXPECT_EQ(due[0].fire_at_ms, 2000u);
}

}  // namespace
