// Pins the invariant that broke: every Game-screen tutorial spotlight must actually
// contain the element its callout is talking about.
//
// The Game rects used to be hand-copied canvas fractions duplicating
// render::compute_layout. When the table was reshaped -- symmetric racetrack, new
// centre, new radii -- the fractions stayed put and the spotlights silently ended up
// lighting empty room. Nothing caught it because nothing asserted the relationship.
//
// These tests derive the expected element positions from compute_layout, the same
// source the renderer uses, so a future layout change either keeps the spotlights
// aligned or fails here.

#include "tutorial/overlay.hpp"

#include "animations/button_morph.hpp"
#include "render/layout.hpp"
#include "render/opponents.hpp"
#include "tutorial/step_sequencer.hpp"

#include <gtest/gtest.h>

namespace {

namespace tut = poker_trainer::tutorial;
namespace an = poker_trainer::animations;
namespace rnd = poker_trainer::render;

using tut::SpotTarget;

// The canvases worth checking: the reference design size, a small laptop, and 4K.
// A rect that is right at one size and wrong at another is the same class of bug.
constexpr an::Canvas kCanvases[] = {
    {1920.0f, 1080.0f},
    {1280.0f, 720.0f},
    {3840.0f, 2160.0f},
};

[[nodiscard]] bool contains(const an::Rect& r, float x, float y) noexcept {
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

[[nodiscard]] an::Rect rect_for(SpotTarget t, an::Canvas c) {
    const auto r = tut::spotlight_rect(t, c);
    EXPECT_TRUE(r.has_value());
    return r.value_or(an::Rect{});
}

TEST(TutorialSpotlights, EveryGameTargetResolves) {
    constexpr SpotTarget kGameTargets[] = {
        SpotTarget::GameChipLegend,     SpotTarget::GamePotBlinds,
        SpotTarget::GameOppFold,        SpotTarget::GameCommunityCards,
        SpotTarget::GameHoleCards,      SpotTarget::GameOpponents,
        SpotTarget::GamePushedChips,    SpotTarget::GameClusterX,
        SpotTarget::GameOppEmptyChips,  SpotTarget::GameMathPotOdds,
        SpotTarget::GameMathOuts,       SpotTarget::GameMathEquity,
        SpotTarget::GameMathEv,         SpotTarget::GameMathFold,
        SpotTarget::GameMathTierEv,     SpotTarget::GameMathBetSize,
    };
    for (const an::Canvas c : kCanvases) {
        for (const SpotTarget t : kGameTargets) {
            const an::Rect r = rect_for(t, c);
            EXPECT_GT(r.w, 0.0f);
            EXPECT_GT(r.h, 0.0f);
            // On screen, not off in the margins.
            EXPECT_LT(r.x, c.width);
            EXPECT_LT(r.y, c.height);
            EXPECT_GT(r.x + r.w, 0.0f);
            EXPECT_GT(r.y + r.h, 0.0f);
        }
    }
}

TEST(TutorialSpotlights, HoleCardsCoverTheHeroSeat) {
    for (const an::Canvas c : kCanvases) {
        const rnd::GameLayout L = rnd::compute_layout(c.width, c.height);
        const rnd::Pt hero = rnd::seat_spot(L, 0).pos;
        const an::Rect r = rect_for(SpotTarget::GameHoleCards, c);
        // The fan is drawn with its bottom on the seat point, so the seat x must be
        // inside horizontally and the seat y at or just below the rect's bottom.
        EXPECT_GE(hero.x, r.x);
        EXPECT_LE(hero.x, r.x + r.w);
        EXPECT_GE(hero.y, r.y);
        EXPECT_LE(hero.y, r.y + r.h + 1.0f);
    }
}

TEST(TutorialSpotlights, OpponentRingCoversEveryOpponentSeat) {
    for (const an::Canvas c : kCanvases) {
        const rnd::GameLayout L = rnd::compute_layout(c.width, c.height);
        const an::Rect r = rect_for(SpotTarget::GameOpponents, c);
        for (int slot = 1; slot <= 5; ++slot) {
            const rnd::Pt p = rnd::seat_spot(L, slot).pos;
            EXPECT_TRUE(contains(r, p.x, p.y))
                << "opponent slot " << slot << " at (" << p.x << "," << p.y
                << ") outside spotlight " << r.x << "," << r.y << " " << r.w << "x" << r.h;
        }
        // The hero must NOT be in it -- this step is about the opponents.
        const rnd::Pt hero = rnd::seat_spot(L, 0).pos;
        EXPECT_FALSE(contains(r, hero.x, hero.y));
    }
}

TEST(TutorialSpotlights, OppFoldCoversItsOnFeltAnchor) {
    for (const an::Canvas c : kCanvases) {
        const rnd::GameLayout L = rnd::compute_layout(c.width, c.height);
        // game_screen.cpp draws it at table_center.y - table_ry * 0.62.
        const float ax = L.table_center.x;
        const float ay = L.table_center.y - L.table_ry * 0.62f;
        EXPECT_TRUE(contains(rect_for(SpotTarget::GameOppFold, c), ax, ay));
    }
}

TEST(TutorialSpotlights, PushedChipsCoverTheActiveSeatToPotCorridor) {
    for (const an::Canvas c : kCanvases) {
        const rnd::GameLayout L = rnd::compute_layout(c.width, c.height);
        const rnd::Pt seat = rnd::seat_spot(L, rnd::active_opponent_slot()).pos;
        // The rest position game_screen lerps the pushed chips to.
        const float fx = seat.x + (L.pot.x - seat.x) * 0.55f;
        const float fy = seat.y + (L.pot.y - seat.y) * 0.55f;
        EXPECT_TRUE(contains(rect_for(SpotTarget::GamePushedChips, c), fx, fy));
        // The Aggressor's empty-chip cue points at the same corridor.
        EXPECT_TRUE(contains(rect_for(SpotTarget::GameOppEmptyChips, c), fx, fy));
    }
}

TEST(TutorialSpotlights, LegendAndInfoLinesSitOnTheInfoAnchorAndDoNotOverlap) {
    for (const an::Canvas c : kCanvases) {
        const rnd::GameLayout L = rnd::compute_layout(c.width, c.height);
        const an::Rect legend = rect_for(SpotTarget::GameChipLegend, c);
        const an::Rect lines = rect_for(SpotTarget::GamePotBlinds, c);
        EXPECT_TRUE(contains(legend, L.info_anchor.x, L.info_anchor.y));
        // The info lines stack directly beneath the legend, not on top of it.
        EXPECT_GE(lines.y, legend.y + legend.h - 1.0f);
    }
}

TEST(TutorialSpotlights, ClusterCoversAllFourIcons) {
    for (const an::Canvas c : kCanvases) {
        const rnd::GameLayout L = rnd::compute_layout(c.width, c.height);
        const float box = L.w * 0.024f;
        const float gap = box * 0.35f;
        const an::Rect r = rect_for(SpotTarget::GameClusterX, c);
        for (int i = 0; i < 4; ++i) {
            const float cx = L.cluster_anchor.x + static_cast<float>(i) * (box + gap) + box * 0.5f;
            const float cy = L.cluster_anchor.y + box * 0.5f;
            EXPECT_TRUE(contains(r, cx, cy)) << "cluster icon " << i << " outside the spotlight";
        }
    }
}

TEST(TutorialSpotlights, BoardReadoutIsTopCentreNotTheOnFeltCopy) {
    for (const an::Canvas c : kCanvases) {
        const rnd::GameLayout L = rnd::compute_layout(c.width, c.height);
        const an::Rect r = rect_for(SpotTarget::GameCommunityCards, c);
        // Horizontally centred on the canvas...
        const float mid = r.x + r.w * 0.5f;
        EXPECT_NEAR(mid, c.width * 0.5f, c.width * 0.02f);
        // ...and in the top band, clear of the felt: the readout is what the user
        // actually reads, the on-felt cards being largely cosmetic.
        EXPECT_LT(r.y + r.h, L.table_center.y - L.table_ry);
    }
}

TEST(TutorialSpotlights, RectsTrackTheTableRatherThanTheCanvas) {
    // The regression in one assertion: move the table, and the spotlights must move
    // with it. compute_layout places the felt centre at a fixed fraction, so scaling
    // the canvas must scale the on-felt spotlights by the same factor -- a rect built
    // from stale hardcoded fractions would not hold this at BOTH sizes.
    const an::Canvas a{1600.0f, 1000.0f};
    const an::Canvas b{3200.0f, 2000.0f};
    const an::Rect ra = rect_for(SpotTarget::GameOppFold, a);
    const an::Rect rb = rect_for(SpotTarget::GameOppFold, b);
    EXPECT_NEAR(rb.x, ra.x * 2.0f, 1.0f);
    EXPECT_NEAR(rb.y, ra.y * 2.0f, 1.0f);
}

}  // namespace
