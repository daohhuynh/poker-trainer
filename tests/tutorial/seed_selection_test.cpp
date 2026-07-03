#include "tutorial/tutorial.hpp"

#include "tutorial/forced_settings.hpp"

#include "engine/generator.hpp"
#include "engine/scenario.hpp"

#include "settings/settings.hpp"

#include <gtest/gtest.h>

namespace pt = poker_trainer::tutorial;
namespace eng = poker_trainer::engine;
namespace st = poker_trainer::settings;

// These tests LOCK the two hardcoded teaching seeds: they re-derive the scenarios
// against the real generator and assert the produced hands still match the spec's
// teaching criteria, so a generator change that broke the tutorial is caught here.
// The tutorial generates under the forced settings, so the fixtures apply them.

namespace {

[[nodiscard]] st::Settings forced_settings() {
    st::Settings s{};
    pt::apply_forced(s);  // HUD on, countdown off, bet sizing on (multi-tier)
    return s;
}

}  // namespace

TEST(TutorialSeeds, CallerPeekTypeIsCaller) {
    EXPECT_EQ(eng::peek_type(pt::kTutorialScenarioCallerSeed), eng::ScenarioType::Caller);
}

TEST(TutorialSeeds, CallerSeedProducesTheCleanTeachingHand) {
    const eng::ScenarioState s =
        eng::generate_scenario(pt::kTutorialScenarioCallerSeed, forced_settings());

    EXPECT_EQ(s.type, eng::ScenarioType::Caller);
    EXPECT_EQ(s.street, eng::Street::Flop);
    // The spec's worked example: pot $100, faced bet $50 → pot odds 50/(100+50) = 33%.
    EXPECT_EQ(s.pot, 100);
    EXPECT_EQ(s.faced_bet, 50);
    EXPECT_NEAR(s.caller_pot_odds_pct, 33.33, 0.1);
    // Simple outs / clean equity (4 outs → Rule of 4 → 16% on the flop).
    EXPECT_EQ(s.caller_outs, 4);
    EXPECT_NEAR(s.caller_equity_pct, 16.0, 0.01);

    // The exact hand backs the Outs callout's card naming (tutorial §4: "any 4 completes
    // your A-2-3-4-5 straight"). hole 2s 5s on board Ah 6h 3s -> the four 4s are the outs.
    // Locked here so a generator change that broke that specific copy is caught.
    EXPECT_EQ(s.hole[0], (eng::Card{2, eng::Suit::Spades}));
    EXPECT_EQ(s.hole[1], (eng::Card{5, eng::Suit::Spades}));
    ASSERT_EQ(s.board_count, 3);
    EXPECT_EQ(s.board[0], (eng::Card{14, eng::Suit::Hearts}));
    EXPECT_EQ(s.board[1], (eng::Card{6, eng::Suit::Hearts}));
    EXPECT_EQ(s.board[2], (eng::Card{3, eng::Suit::Spades}));
}

TEST(TutorialSeeds, AggressorPeekTypeIsPureBluff) {
    EXPECT_EQ(eng::peek_type(pt::kTutorialScenarioAggressorSeed),
              eng::ScenarioType::AggressorPureBluff);
}

TEST(TutorialSeeds, AggressorSeedStillMultiTierPureBluffOnRoundPot) {
    const eng::ScenarioState s =
        eng::generate_scenario(pt::kTutorialScenarioAggressorSeed, forced_settings());

    // Structural teaching properties still hold: a multi-tier Pure Bluff on a round
    // pot ($2400 -> $800/$1200/$2400/$3600) on the Turn.
    EXPECT_EQ(s.type, eng::ScenarioType::AggressorPureBluff);
    EXPECT_TRUE(s.multi_tier) << "Step 6 must be the multi-tier drill";
    EXPECT_EQ(s.pot, 2400);
    EXPECT_EQ(s.pot % 600, 0);

    // Under the situational-F model the opponent fold % is SHOWN per tier, so the
    // tutorial teaches reading it (not guessing) and derives EV / the best size from it.
    // The walkthrough walks tier 1 (1/3 pot, $800): breakeven 800/(2400+800) = 25%, the
    // shown fold is 46%, so the bluff profits (tier-1 EV +$678). The best size for this
    // hand is the Overbet (max-EV). These exact values back the live callout copy, and
    // lock the seed's DETERMINISM so a later generator change is caught.
    EXPECT_EQ(s.correct_bet_tier, eng::BetTier::Overbet);
    EXPECT_NEAR(s.fold_baseline_f, 0.5034517398728724, 1e-9);
    EXPECT_NEAR(s.tiers[0].bet_dollars, 800.0, 1e-6);
    EXPECT_NEAR(s.tiers[0].fold_probability, 0.4617850732, 1e-6);  // shown ~46%
    EXPECT_NEAR(s.tiers[0].ev, 677.7122342598583, 1e-6);
    EXPECT_NEAR(s.tiers[3].ev, 920.7104392372344, 1e-6);
}
