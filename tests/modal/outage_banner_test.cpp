#include "modal/outage_banner.hpp"

#include <gtest/gtest.h>

// Zone 11 — Service Outage Banner pure state machine. Timing is driven by an
// explicit `now` so the slide/hold/dismiss/replace-in-place transitions are
// deterministic.

namespace {

using namespace poker_trainer::modal;

TEST(OutageBanner, TriggerFromHiddenSlidesIn) {
    BannerState s{};
    banner_trigger(s, "down", 1000);
    EXPECT_EQ(s.phase, BannerPhase::SlidingIn);
    EXPECT_TRUE(banner_visible(s));
    EXPECT_EQ(s.message, "down");
    const BannerGeometry g = banner_geometry(s, 1000);
    EXPECT_FLOAT_EQ(g.reveal, 0.0f);  // start of slide-in
    EXPECT_FLOAT_EQ(g.bar, 1.0f);
}

TEST(OutageBanner, SlideInCompletesToHoldingFullyRevealed) {
    BannerState s{};
    banner_trigger(s, "m", 0);
    banner_advance(s, static_cast<std::uint64_t>(kBannerSlideMs));
    EXPECT_EQ(s.phase, BannerPhase::Holding);
    const BannerGeometry g = banner_geometry(s, static_cast<std::uint64_t>(kBannerSlideMs));
    EXPECT_FLOAT_EQ(g.reveal, 1.0f);
    EXPECT_FLOAT_EQ(g.bar, 1.0f);
}

TEST(OutageBanner, CountdownBarDrainsLinearlyOverHold) {
    BannerState s{};
    banner_trigger(s, "m", 0);
    banner_advance(s, 300);  // Holding starts at 300
    const BannerGeometry g = banner_geometry(s, 300 + kBannerHoldMs / 2);
    EXPECT_NEAR(g.bar, 0.5f, 0.01f);
}

TEST(OutageBanner, HoldExpiresToSlidingOutWithDrainedBar) {
    BannerState s{};
    banner_trigger(s, "m", 0);
    banner_advance(s, 300);                 // Holding
    banner_advance(s, 300 + kBannerHoldMs);  // hold elapsed -> SlidingOut
    EXPECT_EQ(s.phase, BannerPhase::SlidingOut);
    EXPECT_FLOAT_EQ(banner_geometry(s, 300 + kBannerHoldMs).bar, 0.0f);
}

TEST(OutageBanner, SlidingOutCompletesToHidden) {
    BannerState s{};
    banner_trigger(s, "m", 0);
    banner_advance(s, 300);
    banner_advance(s, 5300);                                            // -> SlidingOut at 5300
    banner_advance(s, 5300 + static_cast<std::uint64_t>(kBannerSlideMs));  // slide-out done
    EXPECT_EQ(s.phase, BannerPhase::Hidden);
    EXPECT_FALSE(banner_visible(s));
}

TEST(OutageBanner, ClickDismissHaltsBarAndSlidesOut) {
    BannerState s{};
    banner_trigger(s, "m", 0);
    banner_advance(s, 300);            // Holding
    banner_dismiss(s, 300 + 1000);     // 1s into the 5s hold -> bar ~0.8
    EXPECT_EQ(s.phase, BannerPhase::SlidingOut);
    EXPECT_NEAR(s.bar_frozen, 0.8f, 0.01f);
    EXPECT_NEAR(banner_geometry(s, 300 + 1000).bar, 0.8f, 0.01f);  // frozen during slide-out
}

TEST(OutageBanner, RetriggerWhileVisibleReplacesInPlaceNoSlideInReplay) {
    BannerState s{};
    banner_trigger(s, "first", 0);
    banner_advance(s, 300);                // Holding
    banner_advance(s, 300 + 2500);         // mid-hold
    banner_trigger(s, "second", 300 + 2500);
    EXPECT_EQ(s.phase, BannerPhase::Holding);  // NOT SlidingIn
    EXPECT_EQ(s.message, "second");
    const BannerGeometry g = banner_geometry(s, 300 + 2500);
    EXPECT_FLOAT_EQ(g.reveal, 1.0f);  // already fully in view (no slide-in replay)
    EXPECT_FLOAT_EQ(g.bar, 1.0f);     // 5s + bar reset to full
}

TEST(OutageBanner, DismissWhenHiddenIsNoOp) {
    BannerState s{};
    banner_dismiss(s, 100);
    EXPECT_EQ(s.phase, BannerPhase::Hidden);
    EXPECT_FALSE(banner_visible(s));
}

}  // namespace
