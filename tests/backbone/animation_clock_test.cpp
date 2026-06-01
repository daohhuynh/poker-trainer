#include "backbone/animation_clock.hpp"

#include <gtest/gtest.h>

namespace bb = poker_trainer::backbone;

class AnimationClockTest : public ::testing::Test {
 protected:
    void SetUp() override { bb::reset_animation_clock_for_testing(); }
    void TearDown() override { bb::reset_animation_clock_for_testing(); }
};

TEST_F(AnimationClockTest, StartsAtZero) {
    EXPECT_EQ(bb::total_ms_since_app_start(), 0u);
    EXPECT_FLOAT_EQ(bb::delta_ms_since_last_frame(), 0.0f);
}

TEST_F(AnimationClockTest, TickAccumulatesTotal) {
    bb::tick(16);
    EXPECT_EQ(bb::total_ms_since_app_start(), 16u);
    bb::tick(17);
    EXPECT_EQ(bb::total_ms_since_app_start(), 33u);
    bb::tick(16);
    EXPECT_EQ(bb::total_ms_since_app_start(), 49u);
}

TEST_F(AnimationClockTest, DeltaReflectsMostRecentFrame) {
    bb::tick(16);
    EXPECT_FLOAT_EQ(bb::delta_ms_since_last_frame(), 16.0f);
    bb::tick(33);
    EXPECT_FLOAT_EQ(bb::delta_ms_since_last_frame(), 33.0f);
}

TEST_F(AnimationClockTest, ZeroDeltaAdvancesNothingButResetsDelta) {
    bb::tick(50);
    bb::tick(0);
    EXPECT_EQ(bb::total_ms_since_app_start(), 50u);
    EXPECT_FLOAT_EQ(bb::delta_ms_since_last_frame(), 0.0f);
}

TEST_F(AnimationClockTest, ResetReturnsToZero) {
    bb::tick(1000);
    bb::reset_animation_clock_for_testing();
    EXPECT_EQ(bb::total_ms_since_app_start(), 0u);
    EXPECT_FLOAT_EQ(bb::delta_ms_since_last_frame(), 0.0f);
}

TEST_F(AnimationClockTest, AccumulatesManyFramesDeterministically) {
    // 100 frames of 16ms + a final 17ms frame.
    for (int i = 0; i < 100; ++i) {
        bb::tick(16);
    }
    EXPECT_EQ(bb::total_ms_since_app_start(), 1600u);
    bb::tick(17);
    EXPECT_EQ(bb::total_ms_since_app_start(), 1617u);
    EXPECT_FLOAT_EQ(bb::delta_ms_since_last_frame(), 17.0f);
}
