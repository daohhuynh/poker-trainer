// Scenario Generator tests: determinism (same id+settings -> identical state),
// replay stability across difficulty changes, the seed-encoded street weights,
// and the golden-tested generation invariants (decision margins, pot-odds
// spread, max-EV bet tier). The math truth recomputation here is independent of
// the generator's internal copy, so it doubles as a check on the stored truth.

#include "engine/generator.hpp"

#include "engine/evaluator.hpp"
#include "engine/fold_function.hpp"
#include "engine/hand_eval.hpp"
#include "engine/scenario.hpp"
#include "engine/scenario_id.hpp"

#include "settings/settings.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace pe = poker_trainer::engine;
namespace ps = poker_trainer::settings;

namespace {

constexpr double kEps = 1e-9;
constexpr std::uint64_t kSampleSeeds = 4000;

ps::Settings default_settings() {
    return ps::Settings{};  // street 15/35/30/20, difficulty [0.2,0.8], side pot 0.10, bet sizing on
}

std::uint8_t expected_board_count(pe::Street s) {
    switch (s) {
        case pe::Street::Preflop:
            return 0;
        case pe::Street::Flop:
            return 3;
        case pe::Street::Turn:
            return 4;
        case pe::Street::River:
            return 5;
    }
    return 0;
}

double recompute_tier_ev(const pe::ScenarioState& s, std::uint8_t t) {
    const double frac = pe::kBetTierFractions[t];
    const double pot = static_cast<double>(s.pot);
    const double bet = frac * pot;
    const double pf = pe::fold_probability(s.fold_baseline_f, frac);
    const double pc = 1.0 - pf;
    switch (s.type) {
        case pe::ScenarioType::AggressorPureBluff:
            return pe::pure_bluff_ev(pf, pot, bet);
        case pe::ScenarioType::AggressorValueBet:
            return pe::value_bet_ev(pc, bet);
        case pe::ScenarioType::AggressorSemiBluff:
            return pe::semi_bluff_ev(pf, s.aggressor_equity_pct / 100.0, pot, bet);
        default:
            return 0.0;
    }
}

}  // namespace

TEST(Generator, SameSeedAndSettingsAreIdentical) {
    const ps::Settings settings = default_settings();
    for (std::uint64_t id : {1ull, 2ull, 42ull, 1000ull, 123456789ull, 0xFFFFFFFFFFull}) {
        const pe::ScenarioState a = pe::generate_scenario(pe::ScenarioId{id}, settings);
        const pe::ScenarioState b = pe::generate_scenario(pe::ScenarioId{id}, settings);
        EXPECT_TRUE(a == b) << "id=" << id;
    }
}

TEST(Generator, PeekTypeMatchesGeneratedType) {
    // peek_type must equal the fully-generated type for every id (it is what Z05
    // filters on when selecting a mode-matched id), and be deterministic.
    const ps::Settings settings = default_settings();
    for (std::uint64_t id = 1; id <= kSampleSeeds; ++id) {
        const pe::ScenarioType peeked = pe::peek_type(pe::ScenarioId{id});
        EXPECT_EQ(peeked, pe::generate_scenario(pe::ScenarioId{id}, settings).type) << "id=" << id;
        EXPECT_EQ(peeked, pe::peek_type(pe::ScenarioId{id})) << "id=" << id;
    }
}

TEST(Generator, KnownSeedReproducesKnownScenario) {
    // Regression lock on the frozen generation algorithm (RNG draw order + math).
    // Captured from the implementation under default settings; if any of these
    // change, a saved/shared scenario would no longer reconstruct. Suit codes:
    // 0=Clubs, 1=Diamonds, 2=Hearts, 3=Spades.
    using pe::Card;
    using pe::Suit;
    const ps::Settings st = default_settings();

    // id=1: a Caller, flop, wheel gutshot (4 outs) facing a 60-into-185 bet.
    {
        const pe::ScenarioState s = pe::generate_scenario(pe::ScenarioId{1}, st);
        EXPECT_EQ(s.type, pe::ScenarioType::Caller);
        EXPECT_EQ(s.position, pe::Position::UnderTheGun);
        EXPECT_EQ(s.street, pe::Street::Flop);
        EXPECT_EQ(s.board_count, 3);
        EXPECT_FALSE(s.side_pot);
        EXPECT_EQ(s.hole[0], (Card{5, Suit::Spades}));
        EXPECT_EQ(s.hole[1], (Card{3, Suit::Hearts}));
        EXPECT_EQ(s.board[0], (Card{14, Suit::Hearts}));
        EXPECT_EQ(s.board[1], (Card{8, Suit::Clubs}));
        EXPECT_EQ(s.board[2], (Card{2, Suit::Diamonds}));
        EXPECT_EQ(s.pot, 185);
        EXPECT_EQ(s.faced_bet, 60);
        EXPECT_EQ(s.effective_stack, 260);
        EXPECT_EQ(s.caller_outs, 4);
        EXPECT_NEAR(s.caller_pot_odds_pct, 24.4897959184, 1e-6);
        EXPECT_NEAR(s.caller_equity_pct, 16.0, 1e-9);
        EXPECT_NEAR(s.caller_ev, -11.2, 1e-6);
    }
    // id=3: a Value Bet, flop, pot 88, max-EV tier is the overbet.
    {
        const pe::ScenarioState s = pe::generate_scenario(pe::ScenarioId{3}, st);
        EXPECT_EQ(s.type, pe::ScenarioType::AggressorValueBet);
        EXPECT_EQ(s.position, pe::Position::Hijack);
        EXPECT_EQ(s.pot, 88);
        EXPECT_EQ(s.faced_bet, 0);
        EXPECT_NEAR(s.fold_baseline_f, 0.4758721087, 1e-9);
        EXPECT_EQ(s.correct_bet_tier, pe::BetTier::Overbet);
        EXPECT_NEAR(s.tiers[0].ev, 16.1077514770, 1e-6);
        EXPECT_NEAR(s.tiers[3].ev, 49.3848816466, 1e-6);
        EXPECT_NEAR(s.tiers[1].fold_probability, 0.4758721087, 1e-9);  // half pot -> F
    }
    // id=4: a Semi-Bluff, flop, 8-out draw (32% equity), max-EV tier is the overbet.
    {
        const pe::ScenarioState s = pe::generate_scenario(pe::ScenarioId{4}, st);
        EXPECT_EQ(s.type, pe::ScenarioType::AggressorSemiBluff);
        EXPECT_EQ(s.pot, 40);
        EXPECT_NEAR(s.aggressor_equity_pct, 32.0, 1e-9);
        EXPECT_NEAR(s.fold_baseline_f, 0.6775267356, 1e-9);
        EXPECT_EQ(s.correct_bet_tier, pe::BetTier::Overbet);
        EXPECT_NEAR(s.tiers[3].ev, 31.5833046982, 1e-6);
    }
    // id=7: a Pure Bluff, river, side-pot scenario, max-EV tier is the small bet.
    {
        const pe::ScenarioState s = pe::generate_scenario(pe::ScenarioId{7}, st);
        EXPECT_EQ(s.type, pe::ScenarioType::AggressorPureBluff);
        EXPECT_EQ(s.street, pe::Street::River);
        EXPECT_EQ(s.board_count, 5);
        EXPECT_TRUE(s.side_pot);
        EXPECT_EQ(s.pot, 66);
        EXPECT_NEAR(s.fold_baseline_f, 0.3686793942, 1e-9);
        EXPECT_EQ(s.correct_bet_tier, pe::BetTier::OneThirdPot);
        EXPECT_NEAR(s.tiers[0].ev, 8.2437866899, 1e-6);
        EXPECT_NEAR(s.tiers[3].ev, -13.4178999564, 1e-6);
    }
}

TEST(Generator, StructureIsStableAcrossDifficultyRange) {
    // The difficulty range only re-rolls F (drawn last); the locked structure
    // must be byte-identical regardless of it.
    ps::Settings tight = default_settings();
    tight.gameplay.difficulty_min = 0.45f;
    tight.gameplay.difficulty_max = 0.55f;
    for (std::uint64_t id = 1; id <= 1500; ++id) {
        const pe::ScenarioState a = pe::generate_scenario(pe::ScenarioId{id}, default_settings());
        const pe::ScenarioState b = pe::generate_scenario(pe::ScenarioId{id}, tight);
        EXPECT_EQ(a.type, b.type);
        EXPECT_EQ(a.position, b.position);
        EXPECT_EQ(a.street, b.street);
        EXPECT_EQ(a.hole, b.hole);
        EXPECT_EQ(a.board, b.board);
        EXPECT_EQ(a.board_count, b.board_count);
        EXPECT_EQ(a.pot, b.pot);
        EXPECT_EQ(a.effective_stack, b.effective_stack);
        EXPECT_EQ(a.side_pot, b.side_pot);
        EXPECT_EQ(a.faced_bet, b.faced_bet);
    }
}

TEST(Generator, BetSizingToggleChangesPresentationOnly) {
    ps::Settings off = default_settings();
    off.gameplay.bet_sizing_engine_enabled = false;
    for (std::uint64_t id = 1; id <= 1500; ++id) {
        const pe::ScenarioState on = pe::generate_scenario(pe::ScenarioId{id}, default_settings());
        const pe::ScenarioState od = pe::generate_scenario(pe::ScenarioId{id}, off);
        // Identity, economics, F, and the full tier truth are untouched by the toggle.
        EXPECT_EQ(on.type, od.type);
        EXPECT_EQ(on.pot, od.pot);
        EXPECT_EQ(on.fold_baseline_f, od.fold_baseline_f);
        EXPECT_EQ(on.tiers, od.tiers);
        EXPECT_EQ(on.correct_bet_tier, od.correct_bet_tier);
        EXPECT_TRUE(on.multi_tier);
        if (pe::is_aggressor(on.type)) {
            EXPECT_FALSE(od.multi_tier);
        }
    }
}

TEST(Generator, BoardCountMatchesStreet) {
    const ps::Settings settings = default_settings();
    for (std::uint64_t id = 1; id <= kSampleSeeds; ++id) {
        const pe::ScenarioState s = pe::generate_scenario(pe::ScenarioId{id}, settings);
        EXPECT_EQ(s.board_count, expected_board_count(s.street)) << "id=" << id;
    }
}

TEST(Generator, DrawingScenariosSpawnOnlyOnFlopOrTurn) {
    const ps::Settings settings = default_settings();
    for (std::uint64_t id = 1; id <= kSampleSeeds; ++id) {
        const pe::ScenarioState s = pe::generate_scenario(pe::ScenarioId{id}, settings);
        if (pe::needs_draw(s.type)) {
            EXPECT_TRUE(s.street == pe::Street::Flop || s.street == pe::Street::Turn) << "id=" << id;
        }
    }
}

TEST(Generator, CallerInvariantsHold) {
    const ps::Settings settings = default_settings();
    for (std::uint64_t id = 1; id <= kSampleSeeds; ++id) {
        const pe::ScenarioState s = pe::generate_scenario(pe::ScenarioId{id}, settings);
        if (s.type != pe::ScenarioType::Caller) {
            continue;
        }
        // Pot and bet present, on a real draw.
        EXPECT_GE(s.faced_bet, 1) << "id=" << id;
        EXPECT_GT(s.caller_outs, 0) << "id=" << id;
        // Stored truth matches an independent recomputation.
        const double eq = pe::equity_from_outs(s.caller_outs, s.street);
        EXPECT_NEAR(s.caller_equity_pct, eq, kEps) << "id=" << id;
        const double pot_odds = 100.0 * pe::pot_odds_fraction(static_cast<double>(s.pot),
                                                              static_cast<double>(s.faced_bet));
        EXPECT_NEAR(s.caller_pot_odds_pct, pot_odds, kEps) << "id=" << id;
        EXPECT_NEAR(s.caller_ev,
                    pe::net_call_ev(eq / 100.0, static_cast<double>(s.pot),
                                    static_cast<double>(s.faced_bet)),
                    1e-6) << "id=" << id;
        // Invariants: pot odds in band, decision margin clears the grading band.
        EXPECT_GE(s.caller_pot_odds_pct, pe::kPotOddsMinPct - kEps) << "id=" << id;
        EXPECT_LE(s.caller_pot_odds_pct, pe::kPotOddsMaxPct + kEps) << "id=" << id;
        EXPECT_GE(std::abs(s.caller_equity_pct - s.caller_pot_odds_pct),
                  pe::kCallerDecisionMarginPct - kEps) << "id=" << id;
    }
}

TEST(Generator, CallerPotOddsSpanTheTargetBand) {
    const ps::Settings settings = default_settings();
    int total = 0;
    int in_core_band = 0;  // 20..40
    double min_odds = 1e9;
    double max_odds = -1e9;
    for (std::uint64_t id = 1; id <= kSampleSeeds; ++id) {
        const pe::ScenarioState s = pe::generate_scenario(pe::ScenarioId{id}, settings);
        if (s.type != pe::ScenarioType::Caller) {
            continue;
        }
        ++total;
        min_odds = std::min(min_odds, s.caller_pot_odds_pct);
        max_odds = std::max(max_odds, s.caller_pot_odds_pct);
        if (s.caller_pot_odds_pct >= 20.0 && s.caller_pot_odds_pct <= 40.0) {
            ++in_core_band;
        }
    }
    ASSERT_GT(total, 100);
    // The bulk lands in the advertised 20-40% band, with real variety.
    EXPECT_GT(static_cast<double>(in_core_band) / static_cast<double>(total), 0.70);
    EXPECT_GE(min_odds, 20.0 - kEps);
    EXPECT_LT(min_odds, 28.0);  // some genuinely cheap calls
    EXPECT_GT(max_odds, 38.0);  // some genuinely expensive calls
}

TEST(Generator, AggressorTierTruthAndMaxEvTier) {
    // The full four-tier truth matches an independent recomputation and the
    // correct (reference) tier is the true max-EV tier. There is no separation
    // invariant: tolerant bet-size grading absorbs near-ties.
    const ps::Settings settings = default_settings();
    for (std::uint64_t id = 1; id <= kSampleSeeds; ++id) {
        const pe::ScenarioState s = pe::generate_scenario(pe::ScenarioId{id}, settings);
        if (!pe::is_aggressor(s.type)) {
            continue;
        }
        EXPECT_GE(s.fold_baseline_f, settings.gameplay.difficulty_min - kEps) << "id=" << id;
        EXPECT_LE(s.fold_baseline_f, settings.gameplay.difficulty_max + kEps) << "id=" << id;

        std::uint8_t best = 0;
        for (std::uint8_t t = 0; t < pe::kBetTierCount; ++t) {
            EXPECT_NEAR(s.tiers[t].ev, recompute_tier_ev(s, t), 1e-6) << "id=" << id << " t=" << int{t};
            EXPECT_NEAR(s.tiers[t].fold_probability,
                        pe::fold_probability(s.fold_baseline_f, pe::kBetTierFractions[t]), kEps);
            if (s.tiers[t].ev > s.tiers[best].ev) {
                best = t;
            }
        }
        EXPECT_EQ(static_cast<std::uint8_t>(s.correct_bet_tier), best) << "id=" << id;

        if (s.type == pe::ScenarioType::AggressorSemiBluff) {
            EXPECT_NEAR(s.aggressor_equity_pct,
                        pe::equity_from_outs(pe::count_draw_outs(s.hole, s.board, s.board_count), s.street),
                        kEps) << "id=" << id;
        }
    }
}

TEST(Generator, PotIsInRange) {
    const ps::Settings settings = default_settings();
    for (std::uint64_t id = 1; id <= kSampleSeeds; ++id) {
        const pe::ScenarioState s = pe::generate_scenario(pe::ScenarioId{id}, settings);
        EXPECT_GE(s.pot, pe::kPotMinDollars);
        EXPECT_LE(s.pot, pe::kPotMaxDollars);
    }
}

TEST(Generator, TypeDistributionIsRoughlyHalfCaller) {
    const ps::Settings settings = default_settings();
    int callers = 0;
    for (std::uint64_t id = 1; id <= kSampleSeeds; ++id) {
        const pe::ScenarioState s = pe::generate_scenario(pe::ScenarioId{id}, settings);
        if (s.type == pe::ScenarioType::Caller) {
            ++callers;
        }
    }
    const double rate = static_cast<double>(callers) / static_cast<double>(kSampleSeeds);
    EXPECT_NEAR(rate, 0.50, 0.05);
}

TEST(Generator, StreetWeightsBiasStreetSelection) {
    // Among non-drawing Aggressor types (which can land on any street), a
    // pre-flop-heavy weighting yields far more pre-flop scenarios.
    ps::Settings preflop_heavy = default_settings();
    preflop_heavy.gameplay.street_weight_preflop = 85;
    preflop_heavy.gameplay.street_weight_flop = 5;
    preflop_heavy.gameplay.street_weight_turn = 5;
    preflop_heavy.gameplay.street_weight_river = 5;

    auto preflop_rate = [](const ps::Settings& cfg) {
        int preflop = 0;
        int eligible = 0;
        for (std::uint64_t id = 1; id <= kSampleSeeds; ++id) {
            const pe::ScenarioState s = pe::generate_scenario(pe::ScenarioId{id}, cfg);
            if (s.type == pe::ScenarioType::AggressorPureBluff ||
                s.type == pe::ScenarioType::AggressorValueBet) {
                ++eligible;
                if (s.street == pe::Street::Preflop) {
                    ++preflop;
                }
            }
        }
        return static_cast<double>(preflop) / static_cast<double>(eligible);
    };

    EXPECT_GT(preflop_rate(preflop_heavy), preflop_rate(default_settings()) + 0.20);
}
