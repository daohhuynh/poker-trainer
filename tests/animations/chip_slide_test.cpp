// Zone 08 — Caller chip-push easing + progress unit tests. Pure scalar math.

#include "animations/chip_slide.hpp"

#include "render/render_constants.hpp"

#include <gtest/gtest.h>

namespace anim = poker_trainer::animations;
namespace rnd = poker_trainer::render;

TEST(EaseOutCubic, Endpoints) {
    EXPECT_FLOAT_EQ(anim::ease_out_cubic(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(anim::ease_out_cubic(1.0f), 1.0f);
}

TEST(EaseOutCubic, ClampsAndIsMonotonic) {
    EXPECT_FLOAT_EQ(anim::ease_out_cubic(-0.5f), 0.0f);
    EXPECT_FLOAT_EQ(anim::ease_out_cubic(2.0f), 1.0f);
    EXPECT_GT(anim::ease_out_cubic(0.5f), anim::ease_out_cubic(0.25f));
    // Ease-OUT: faster at the start, so half-way through time is already past 0.5.
    EXPECT_GT(anim::ease_out_cubic(0.5f), 0.5f);
}

TEST(ChipPushProgress, ZeroAtAndBeforeSpawn) {
    EXPECT_FLOAT_EQ(anim::chip_push_progress(1000, 1000), 0.0f);
    EXPECT_FLOAT_EQ(anim::chip_push_progress(1000, 900), 0.0f);
}

TEST(ChipPushProgress, OneAtAndAfterDuration) {
    EXPECT_FLOAT_EQ(anim::chip_push_progress(1000, 1000 + rnd::kChipPushDurationMs), 1.0f);
    EXPECT_FLOAT_EQ(anim::chip_push_progress(1000, 5000), 1.0f);  // clamped past the end
}

TEST(ChipPushProgress, MidwayIsStrictlyBetween) {
    const float p = anim::chip_push_progress(1000, 1000 + rnd::kChipPushDurationMs / 2);
    EXPECT_GT(p, 0.0f);
    EXPECT_LT(p, 1.0f);
}
