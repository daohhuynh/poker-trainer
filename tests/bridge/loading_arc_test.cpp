#include "bridge/loading_screen.hpp"

#include <gtest/gtest.h>

namespace br = poker_trainer::bridge;

TEST(LoadingArcFraction, ZeroLoadedIsEmpty) {
    EXPECT_FLOAT_EQ(br::loading_arc_fraction(0, 84), 0.0f);
}

TEST(LoadingArcFraction, FullyLoadedIsOne) {
    EXPECT_FLOAT_EQ(br::loading_arc_fraction(84, 84), 1.0f);
}

TEST(LoadingArcFraction, HalfLoadedIsHalf) {
    EXPECT_FLOAT_EQ(br::loading_arc_fraction(42, 84), 0.5f);
}

TEST(LoadingArcFraction, PartialIsProportional) {
    EXPECT_NEAR(br::loading_arc_fraction(1, 3), 1.0f / 3.0f, 1e-6f);
    EXPECT_NEAR(br::loading_arc_fraction(3, 4), 0.75f, 1e-6f);
}

TEST(LoadingArcFraction, EmptyTierReportsFull) {
    // total == 0 means "nothing to load" -> already complete (matches
    // assets::TierLoader::tier_progress reporting 1.0 for an empty tier).
    EXPECT_FLOAT_EQ(br::loading_arc_fraction(0, 0), 1.0f);
}

TEST(LoadingArcFraction, OverflowClampsToOne) {
    // Defensive: resolved should never exceed total, but if it does the
    // fraction clamps to 1.0 rather than exceeding the full revolution.
    EXPECT_FLOAT_EQ(br::loading_arc_fraction(100, 84), 1.0f);
}
