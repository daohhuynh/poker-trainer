// Zone 08 — Frog easter-egg click-counter state machine unit tests. Pure logic.

#include "easter_egg/frog_toggle.hpp"

#include <gtest/gtest.h>

namespace eg = poker_trainer::easter_egg;

namespace {

// Click `n` times, returning the outcome of the final click.
eg::FrogClickOutcome click_n(eg::FrogToggleState& s, int n) {
    eg::FrogClickOutcome out = eg::FrogClickOutcome::Counting;
    for (int i = 0; i < n; ++i) {
        out = eg::register_dealer_click(s);
    }
    return out;
}

}  // namespace

TEST(FrogToggle, CountsUntilThresholdThenToggles) {
    eg::FrogToggleState s{};
    EXPECT_EQ(click_n(s, eg::kFrogToggleClicks - 1), eg::FrogClickOutcome::Counting);
    EXPECT_FALSE(s.frog_active);
    EXPECT_EQ(s.consecutive_clicks, eg::kFrogToggleClicks - 1);

    EXPECT_EQ(eg::register_dealer_click(s), eg::FrogClickOutcome::Toggled);
    EXPECT_TRUE(s.frog_active);
    EXPECT_EQ(s.consecutive_clicks, 0);  // resets on a completed toggle
}

TEST(FrogToggle, SecondThresholdTogglesBackToButler) {
    eg::FrogToggleState s{};
    EXPECT_EQ(click_n(s, eg::kFrogToggleClicks), eg::FrogClickOutcome::Toggled);
    EXPECT_TRUE(s.frog_active);
    EXPECT_EQ(click_n(s, eg::kFrogToggleClicks), eg::FrogClickOutcome::Toggled);
    EXPECT_FALSE(s.frog_active);  // toggled back
}

TEST(FrogToggle, ResetClearsCountButKeepsActiveState) {
    eg::FrogToggleState s{};
    // Complete one toggle so frog is active, then partially click again.
    click_n(s, eg::kFrogToggleClicks);
    ASSERT_TRUE(s.frog_active);
    click_n(s, 5);
    EXPECT_EQ(s.consecutive_clicks, 5);

    eg::reset_click_count(s);  // new scenario
    EXPECT_EQ(s.consecutive_clicks, 0);
    EXPECT_TRUE(s.frog_active);          // active Butler/Frog state persists
    EXPECT_TRUE(eg::frog_active(s));
}

TEST(FrogToggle, FreshStateIsButler) {
    eg::FrogToggleState s{};
    EXPECT_FALSE(eg::frog_active(s));
    EXPECT_EQ(s.consecutive_clicks, 0);
    EXPECT_FALSE(s.tier4_requested);
}
