#include "temporal/delta_timer.hpp"

#include "backbone/animation_clock.hpp"
#include "backbone/screen_state.hpp"

#include "engine/scenario.hpp"
#include "settings/settings.hpp"

#include <cstdint>

#include <gtest/gtest.h>

namespace tt = poker_trainer::temporal;
namespace bb = poker_trainer::backbone;
namespace eng = poker_trainer::engine;
namespace st = poker_trainer::settings;

namespace {

tt::TimerState running_timer(std::uint64_t target, std::uint64_t elapsed, std::uint64_t last_now) {
    tt::TimerState s{};
    s.target_ms = target;
    s.elapsed_ms = elapsed;
    s.last_now_ms = last_now;
    s.running = true;
    return s;
}

eng::ScenarioState aggressor_flop() {
    eng::ScenarioState s{};
    s.type = eng::ScenarioType::AggressorValueBet;
    s.street = eng::Street::Flop;
    return s;
}

// ----- advance (pure state machine) -----

TEST(Advance, AccumulatesFrameDeltaWhileRunning) {
    tt::TimerState s = running_timer(60000, 0, 0);
    s = tt::advance(s, 1000, false);
    EXPECT_EQ(s.elapsed_ms, 1000ULL);
    EXPECT_FALSE(s.paused);
    EXPECT_EQ(s.last_now_ms, 1000ULL);
    s = tt::advance(s, 2500, false);
    EXPECT_EQ(s.elapsed_ms, 2500ULL);
}

TEST(Advance, ModalOpenFrameCountsItsOwnDeltaThenFreezes) {
    tt::TimerState s = running_timer(60000, 1000, 1000);
    // The frame the modal opens: not open last frame, so this delta still counts.
    s = tt::advance(s, 2000, true);
    EXPECT_EQ(s.elapsed_ms, 2000ULL);
    EXPECT_TRUE(s.paused);
    // Every later frame is frozen.
    s = tt::advance(s, 3000, true);
    EXPECT_EQ(s.elapsed_ms, 2000ULL);
    EXPECT_TRUE(s.paused);
}

TEST(Advance, ResumeContinuesFromFrozenValue) {
    tt::TimerState s = running_timer(60000, 2000, 3000);
    s.paused = true;  // a modal has been open
    // Close frame: paused was true, so its delta (the paused span) is dropped.
    s = tt::advance(s, 4000, false);
    EXPECT_EQ(s.elapsed_ms, 2000ULL);
    EXPECT_FALSE(s.paused);
    // Subsequent frames accumulate again from the frozen value.
    s = tt::advance(s, 5000, false);
    EXPECT_EQ(s.elapsed_ms, 3000ULL);
}

TEST(Advance, MultiplePauseResumeCycles) {
    tt::TimerState s = running_timer(60000, 0, 0);
    s = tt::advance(s, 1000, false);   // +1000 -> 1000
    s = tt::advance(s, 2000, true);    // open frame counts -> 2000, paused
    s = tt::advance(s, 5000, true);    // frozen -> 2000
    s = tt::advance(s, 6000, false);   // close frame, paused span dropped -> 2000
    s = tt::advance(s, 7000, false);   // +1000 -> 3000
    s = tt::advance(s, 8000, true);    // open frame counts -> 4000, paused
    s = tt::advance(s, 9000, false);   // close frame -> 4000
    s = tt::advance(s, 10000, false);  // +1000 -> 5000
    EXPECT_EQ(s.elapsed_ms, 5000ULL);
}

TEST(Advance, DisabledAccumulatesNothing) {
    tt::TimerState s = running_timer(60000, 0, 0);
    s.disabled = true;
    s = tt::advance(s, 5000, false);
    EXPECT_EQ(s.elapsed_ms, 0ULL);
}

TEST(Advance, NotRunningAccumulatesNothing) {
    tt::TimerState s{};  // running == false, e.g. after the submit-freeze
    s.target_ms = 60000;
    s.elapsed_ms = 4200;
    s.last_now_ms = 1000;
    s = tt::advance(s, 9000, false);
    EXPECT_EQ(s.elapsed_ms, 4200ULL);
}

TEST(Advance, BackwardsClockClampsDeltaToZero) {
    tt::TimerState s = running_timer(60000, 2000, 5000);
    s = tt::advance(s, 3000, false);  // now < last_now
    EXPECT_EQ(s.elapsed_ms, 2000ULL);
    EXPECT_EQ(s.last_now_ms, 3000ULL);
}

// ----- exposed time-result queries -----

class TimerQueryTest : public ::testing::Test {
 protected:
    void SetUp() override { tt::reset_timer_for_testing(); }
    void TearDown() override { tt::reset_timer_for_testing(); }
};

TEST_F(TimerQueryTest, WithinTargetIsTrueUnderTarget) {
    tt::TimerState s{};
    s.target_ms = 15000;
    s.elapsed_ms = 14999;
    tt::set_timer_state_for_testing(s);
    EXPECT_TRUE(tt::time_within_target());
    EXPECT_FALSE(tt::is_overtime());
    EXPECT_EQ(tt::overtime_ms(), 0ULL);
}

TEST_F(TimerQueryTest, WithinTargetIsInclusiveAtExactlyTarget) {
    tt::TimerState s{};
    s.target_ms = 15000;
    s.elapsed_ms = 15000;
    tt::set_timer_state_for_testing(s);
    EXPECT_TRUE(tt::time_within_target());
    EXPECT_FALSE(tt::is_overtime());
    EXPECT_EQ(tt::overtime_ms(), 0ULL);
}

TEST_F(TimerQueryTest, OverTargetReportsOvertime) {
    tt::TimerState s{};
    s.target_ms = 15000;
    s.elapsed_ms = 15001;
    tt::set_timer_state_for_testing(s);
    EXPECT_FALSE(tt::time_within_target());
    EXPECT_TRUE(tt::is_overtime());
    EXPECT_EQ(tt::overtime_ms(), 1ULL);
}

TEST_F(TimerQueryTest, QueriesEchoState) {
    tt::TimerState s{};
    s.target_ms = 22000;
    s.elapsed_ms = 9000;
    tt::set_timer_state_for_testing(s);
    EXPECT_EQ(tt::target_time_ms(), 22000ULL);
    EXPECT_EQ(tt::actual_time_ms(), 9000ULL);
    EXPECT_EQ(tt::timer_elapsed_ms(), 9000ULL);
}

TEST_F(TimerQueryTest, ResultInvalidWhileDisabled) {
    tt::TimerState s{};
    s.disabled = true;
    tt::set_timer_state_for_testing(s);
    EXPECT_FALSE(tt::time_result_valid());
    s.disabled = false;
    tt::set_timer_state_for_testing(s);
    EXPECT_TRUE(tt::time_result_valid());
}

TEST_F(TimerQueryTest, ManualPauseAndResumeToggleTheLatch) {
    tt::TimerState s{};
    s.target_ms = 60000;
    s.running = true;
    tt::set_timer_state_for_testing(s);
    tt::timer_pause();
    EXPECT_TRUE(tt::timer_state_for_testing().paused);
    tt::timer_resume();
    EXPECT_FALSE(tt::timer_state_for_testing().paused);
}

// ----- timer_start lifecycle (reads live settings + the tutorial phase) -----

class TimerStartTest : public ::testing::Test {
 protected:
    void SetUp() override {
        tt::reset_timer_for_testing();
        bb::reset_screen_state_for_testing();
        bb::reset_animation_clock_for_testing();
    }
    void TearDown() override {
        tt::reset_timer_for_testing();
        bb::reset_screen_state_for_testing();
        bb::reset_animation_clock_for_testing();
    }
};

TEST_F(TimerStartTest, StartsRunningAndComputesTarget) {
    tt::set_settings_source([] {
        st::Settings s{};
        s.gameplay.bet_sizing_engine_enabled = true;   // Aggressor -> multi-tier
        s.gameplay.time_pressure_custom_enabled = false;
        return s;
    });
    tt::timer_start(aggressor_flop());
    const tt::TimerState s = tt::timer_state_for_testing();
    EXPECT_TRUE(s.running);
    EXPECT_FALSE(s.disabled);
    EXPECT_EQ(s.elapsed_ms, 0ULL);
    EXPECT_EQ(s.target_ms, 27000ULL);  // Flop 18000 + 50% multi-tier
    EXPECT_TRUE(tt::time_result_valid());
}

TEST_F(TimerStartTest, DisabledDuringActiveTutorial) {
    tt::set_settings_source([] { return st::Settings{}; });
    bb::set_tutorial_state(bb::TutorialState{bb::TutorialPhase::Active, 1});
    tt::timer_start(aggressor_flop());
    const tt::TimerState s = tt::timer_state_for_testing();
    EXPECT_TRUE(s.disabled);
    EXPECT_FALSE(s.running);
    EXPECT_FALSE(tt::time_result_valid());
}

}  // namespace
