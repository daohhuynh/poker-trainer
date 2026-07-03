// True Evaluator tests. The locked-V8.1 EV worked examples from Module 1 are the
// acceptance criteria and must reproduce the architecture's numbers exactly; the
// remainder cover pot odds, net call EV, and the Post-Round grader's margins.

#include "engine/evaluator.hpp"

#include "engine/scenario.hpp"

#include <array>
#include <cstdint>
#include <optional>

#include <gtest/gtest.h>

namespace pe = poker_trainer::engine;

namespace {

constexpr double kEps = 1e-9;

const pe::InputGrade* find(const pe::GradingResult& r, pe::InputId id) {
    for (const pe::InputGrade& g : r.inputs) {
        if (g.input == id && !g.tier_index.has_value()) {
            return &g;
        }
    }
    return nullptr;
}

const pe::InputGrade* find_tier(const pe::GradingResult& r, pe::InputId id, std::uint8_t tier) {
    for (const pe::InputGrade& g : r.inputs) {
        if (g.input == id && g.tier_index.has_value() && *g.tier_index == tier) {
            return &g;
        }
    }
    return nullptr;
}

}  // namespace

// ===== Acceptance criteria: the locked V8.1 worked examples (Module 1) =====

TEST(EvFormulas, PureBluffFoldAlwaysReturnsPot) {
    EXPECT_NEAR(pe::pure_bluff_ev(1.0, 100.0, 50.0), 100.0, kEps);
}

TEST(EvFormulas, PureBluffNeverFoldsReturnsMinusBet) {
    EXPECT_NEAR(pe::pure_bluff_ev(0.0, 100.0, 50.0), -50.0, kEps);
}

TEST(EvFormulas, ValueBetAlwaysCalledReturnsBet) {
    EXPECT_NEAR(pe::value_bet_ev(1.0, 50.0), 50.0, kEps);
}

TEST(EvFormulas, ValueBetNeverCalledReturnsZero) {
    EXPECT_NEAR(pe::value_bet_ev(0.0, 50.0), 0.0, kEps);
}

TEST(EvFormulas, SemiBluffFoldAlwaysReturnsPot) {
    // P(fold) = 1.0 -> the equity term vanishes (P(call) = 0).
    EXPECT_NEAR(pe::semi_bluff_ev(1.0, 0.4, 100.0, 50.0), 100.0, kEps);
}

TEST(EvFormulas, SemiBluffCalledExampleReturnsThirty) {
    // P(call) = 1.0, equity = 0.4, pot = 100, bet = 50 -> $30.
    EXPECT_NEAR(pe::semi_bluff_ev(0.0, 0.4, 100.0, 50.0), 30.0, kEps);
}

// ===== Pot odds and net call EV =====

TEST(EvFormulas, PotOddsMatchesTutorialExample) {
    // Tutorial: Pot Odds = 50 / (100 + 50) = 33% (33.33... before rounding).
    EXPECT_NEAR(pe::pot_odds_fraction(100.0, 50.0) * 100.0, 100.0 / 3.0, kEps);
}

TEST(EvFormulas, NetCallEvBreaksEvenAtTrueBreakEvenEquity) {
    // EV of calling is zero exactly at equity = bet / (pot + 2*bet).
    const double pot = 100.0;
    const double bet = 50.0;
    const double break_even = bet / (pot + 2.0 * bet);  // 0.25
    EXPECT_NEAR(pe::net_call_ev(break_even, pot, bet), 0.0, kEps);
    EXPECT_GT(pe::net_call_ev(break_even + 0.10, pot, bet), 0.0);
    EXPECT_LT(pe::net_call_ev(break_even - 0.10, pot, bet), 0.0);
}

TEST(EvFormulas, EvMarginUsesFivePercentWithHalfDollarFloor) {
    EXPECT_NEAR(pe::ev_margin(100.0), 5.0, kEps);   // 5% dominates
    EXPECT_NEAR(pe::ev_margin(4.0), 0.5, kEps);     // floor dominates (0.20 -> 0.50)
    EXPECT_NEAR(pe::ev_margin(-200.0), 10.0, kEps); // magnitude, not sign
}

// ===== Grading: Caller =====

namespace {

pe::ScenarioState make_caller() {
    pe::ScenarioState s{};
    s.type = pe::ScenarioType::Caller;
    s.pot = 100;
    s.faced_bet = 50;
    s.caller_pot_odds_pct = 100.0 / 3.0;  // 33.33
    s.caller_outs = 9;
    s.caller_equity_pct = 36.0;
    s.caller_ev = 22.0;
    return s;
}

}  // namespace

TEST(Grading, CallerAllCorrectWithinMargins) {
    const pe::ScenarioState s = make_caller();
    pe::UserAnswers a{};
    a.pot_odds_pct = 31.0;        // |31 - 33.33| = 2.33 <= 5
    a.outs = 9;                   // exact
    a.caller_equity_pct = 40.0;   // |40 - 36| = 4 <= 5
    a.caller_ev = 23.0;           // |23 - 22| = 1 <= max(0.5, 1.1) = 1.1
    const pe::GradingResult r = pe::evaluate(s, a);
    EXPECT_TRUE(r.all_correct);
    EXPECT_TRUE(pe::is_pass(r));
}

TEST(Grading, CallerProbabilityOutsideFivePointsIsWrong) {
    const pe::ScenarioState s = make_caller();
    pe::UserAnswers a{};
    a.pot_odds_pct = 39.0;  // |39 - 33.33| = 5.67 > 5
    a.outs = 9;
    a.caller_equity_pct = 36.0;
    a.caller_ev = 22.0;
    const pe::GradingResult r = pe::evaluate(s, a);
    EXPECT_FALSE(find(r, pe::InputId::PotOdds)->correct);
    EXPECT_FALSE(r.all_correct);
}

TEST(Grading, CallerOutsRequireExactMatch) {
    const pe::ScenarioState s = make_caller();
    pe::UserAnswers a{};
    a.pot_odds_pct = 33.0;
    a.outs = 8;  // off by one -> wrong
    a.caller_equity_pct = 36.0;
    a.caller_ev = 22.0;
    const pe::GradingResult r = pe::evaluate(s, a);
    EXPECT_FALSE(find(r, pe::InputId::Outs)->correct);
}

TEST(Grading, UnfilledInputsAreWrong) {
    const pe::ScenarioState s = make_caller();
    pe::UserAnswers a{};  // everything nullopt
    const pe::GradingResult r = pe::evaluate(s, a);
    EXPECT_FALSE(r.all_correct);
    for (const pe::InputGrade& g : r.inputs) {
        EXPECT_FALSE(g.correct);
        EXPECT_FALSE(g.submitted.has_value());
    }
}

// ===== Grading: Aggressor =====
//
// The per-tier breakeven input is Breakeven Fold % = bet / (pot + bet). Dollar EV is
// restored as a per-tier input (derivable now that the opponent fold % is shown):
// each tier's EV grades against the stored tiers[t].ev. Pure Bluff grades per-tier
// breakeven + per-tier EV + Bet Size; Value Bet grades Equity if Called + per-tier
// EV + Bet Size; Semi-Bluff grades all. Bet Size grades against the max-EV tier
// (tolerant).

namespace {

// Bet-tier pot fractions (1/3, 1/2, Full, Overbet), mirrored from the engine so the
// test can set tier.bet_dollars = fraction * pot and derive the correct breakeven.
constexpr std::array<double, pe::kBetTierCount> kFracs{1.0 / 3.0, 0.5, 1.0, 1.5};

// The correct Breakeven Fold % for a tier: 100 * bet / (pot + bet).
double breakeven_pct(double pot, double bet) {
    return 100.0 * pe::pot_odds_fraction(pot, bet);
}

// A multi-tier Aggressor with proper bet_dollars (so breakeven is well-defined) and
// caller-supplied per-tier EVs (internal only; they set the max-EV / correct tier).
pe::ScenarioState make_aggressor(pe::ScenarioType type,
                                 const std::array<double, pe::kBetTierCount>& evs,
                                 double pot = 100.0) {
    pe::ScenarioState s{};
    s.type = type;
    s.pot = static_cast<int>(pot);
    s.multi_tier = true;
    s.aggressor_equity_pct = 40.0;  // used by Value/Semi Equity-if-Called grading
    std::uint8_t best = 0;
    for (std::uint8_t t = 0; t < pe::kBetTierCount; ++t) {
        pe::AggressorTier tier{};
        tier.tier = static_cast<pe::BetTier>(t);
        tier.bet_fraction = kFracs[t];
        tier.bet_dollars = kFracs[t] * pot;
        tier.fold_probability = 0.30 + 0.05 * static_cast<double>(t);  // internal only
        tier.call_probability = 1.0 - tier.fold_probability;
        tier.ev = evs[t];
        s.tiers[t] = tier;
        if (evs[t] > evs[best]) {
            best = t;
        }
    }
    s.correct_bet_tier = static_cast<pe::BetTier>(best);
    s.presented_tier = s.correct_bet_tier;
    return s;
}

// Fill every tier's Breakeven Fold % with the exactly-correct value.
void fill_correct_breakeven(const pe::ScenarioState& s, pe::UserAnswers& a) {
    for (std::uint8_t t = 0; t < pe::kBetTierCount; ++t) {
        a.tier_breakeven_pct[t] = breakeven_pct(static_cast<double>(s.pot), s.tiers[t].bet_dollars);
    }
}

// Fill every tier's dollar EV with the exactly-correct stored value.
void fill_correct_ev(const pe::ScenarioState& s, pe::UserAnswers& a) {
    for (std::uint8_t t = 0; t < pe::kBetTierCount; ++t) {
        a.tier_ev[t] = s.tiers[t].ev;
    }
}

}  // namespace

TEST(Grading, BreakevenFoldGradesAgainstBetOverPotPlusBet) {
    // pot=100: tier fractions 1/3,1/2,1,3/2 -> breakeven 25%, 33.33%, 50%, 60%.
    const pe::ScenarioState s =
        make_aggressor(pe::ScenarioType::AggressorPureBluff, {10.0, 11.0, 12.0, 13.0});
    pe::UserAnswers a{};
    fill_correct_breakeven(s, a);
    const pe::GradingResult r = pe::evaluate(s, a);
    EXPECT_NEAR(find_tier(r, pe::InputId::FoldProbability, 0)->correct_value, 25.0, 1e-9);
    EXPECT_NEAR(find_tier(r, pe::InputId::FoldProbability, 1)->correct_value, 100.0 / 3.0, 1e-9);
    EXPECT_NEAR(find_tier(r, pe::InputId::FoldProbability, 2)->correct_value, 50.0, 1e-9);
    EXPECT_NEAR(find_tier(r, pe::InputId::FoldProbability, 3)->correct_value, 60.0, 1e-9);
}

TEST(Grading, BreakevenFoldWithinAndBeyondFivePoints) {
    const pe::ScenarioState s =
        make_aggressor(pe::ScenarioType::AggressorPureBluff, {10.0, 11.0, 12.0, 13.0});
    // Tier 2 breakeven is 50%. 45% is within +/-5pp; 44.9% is not.
    pe::UserAnswers a{};
    fill_correct_breakeven(s, a);
    a.tier_breakeven_pct[2] = 45.0;
    EXPECT_TRUE(find_tier(pe::evaluate(s, a), pe::InputId::FoldProbability, 2)->correct);
    a.tier_breakeven_pct[2] = 44.9;
    EXPECT_FALSE(find_tier(pe::evaluate(s, a), pe::InputId::FoldProbability, 2)->correct);
}

TEST(Grading, PureBluffGradesBreakevenAndEvPerTierAndBetSizeNoEquity) {
    const pe::ScenarioState s =
        make_aggressor(pe::ScenarioType::AggressorPureBluff, {10.0, 11.0, 12.0, 13.0});
    pe::UserAnswers a{};
    fill_correct_breakeven(s, a);
    fill_correct_ev(s, a);
    a.selected_bet_tier = pe::BetTier::Overbet;  // max EV (13.0)
    const pe::GradingResult r = pe::evaluate(s, a);
    EXPECT_TRUE(r.all_correct);
    // 4 breakeven + 4 per-tier EV + Bet Size = 9 inputs; no Equity.
    EXPECT_EQ(r.inputs.size(), 9u);
    EXPECT_EQ(find(r, pe::InputId::Equity), nullptr);
    for (std::uint8_t t = 0; t < pe::kBetTierCount; ++t) {
        EXPECT_NE(find_tier(r, pe::InputId::Ev, t), nullptr);
    }
}

TEST(Grading, PerTierEvGradesAgainstStoredTierEvWithinAndBeyondTolerance) {
    const pe::ScenarioState s =
        make_aggressor(pe::ScenarioType::AggressorPureBluff, {10.0, 20.0, 12.0, 13.0});
    // Tier 1's EV is 20.0, so ev_margin = max(0.5, 5% * 20) = 1.0.
    pe::UserAnswers a{};
    fill_correct_ev(s, a);
    a.tier_ev[1] = 20.9;  // within +/- 1.0
    EXPECT_TRUE(find_tier(pe::evaluate(s, a), pe::InputId::Ev, 1)->correct);
    a.tier_ev[1] = 21.1;  // beyond +/- 1.0
    EXPECT_FALSE(find_tier(pe::evaluate(s, a), pe::InputId::Ev, 1)->correct);
    EXPECT_NEAR(find_tier(pe::evaluate(s, a), pe::InputId::Ev, 1)->correct_value, 20.0, kEps);
}

TEST(Grading, ValueBetGradesEquityIfCalledAndEvAndBetSizeNoBreakeven) {
    const pe::ScenarioState s =
        make_aggressor(pe::ScenarioType::AggressorValueBet, {10.0, 11.0, 12.0, 13.0});
    pe::UserAnswers a{};
    a.equity_if_called_pct = 40.0;               // matches aggressor_equity_pct
    fill_correct_ev(s, a);
    a.selected_bet_tier = pe::BetTier::Overbet;   // max EV
    const pe::GradingResult r = pe::evaluate(s, a);
    EXPECT_TRUE(r.all_correct);
    // Value Bet has NO per-tier breakeven: Equity + 4 per-tier EV + Bet Size = 6.
    EXPECT_EQ(r.inputs.size(), 6u);
    EXPECT_NE(find(r, pe::InputId::Equity), nullptr);
    EXPECT_EQ(find_tier(r, pe::InputId::FoldProbability, 0), nullptr);
    EXPECT_NE(find_tier(r, pe::InputId::Ev, 0), nullptr);
    EXPECT_DOUBLE_EQ(find(r, pe::InputId::Equity)->correct_value, 40.0);
}

TEST(Grading, SemiBluffGradesBreakevenAndEquityAndEvAndBetSize) {
    const pe::ScenarioState s =
        make_aggressor(pe::ScenarioType::AggressorSemiBluff, {10.0, 11.0, 12.0, 13.0});
    pe::UserAnswers a{};
    fill_correct_breakeven(s, a);
    a.equity_if_called_pct = 40.0;
    fill_correct_ev(s, a);
    a.selected_bet_tier = pe::BetTier::Overbet;
    const pe::GradingResult r = pe::evaluate(s, a);
    EXPECT_TRUE(r.all_correct);
    // 4 breakeven + 4 per-tier EV + 1 Equity + Bet Size = 10; Equity graded once.
    EXPECT_EQ(r.inputs.size(), 10u);
    std::size_t equity_count = 0;
    for (const pe::InputGrade& g : r.inputs) {
        if (g.input == pe::InputId::Equity) ++equity_count;
    }
    EXPECT_EQ(equity_count, 1u);
}

TEST(Grading, AggressorNonMaxTierWithinEvToleranceIsAccepted) {
    // Max-EV tier is Full Pot (13.0); Half Pot (12.9) is within ev_margin(13)=0.65,
    // so selecting it is graded correct (statistically tied with the best).
    const pe::ScenarioState s = make_aggressor(pe::ScenarioType::AggressorPureBluff,
                                               {9.0, 12.9, 13.0, 8.0});
    pe::UserAnswers a{};
    fill_correct_breakeven(s, a);
    fill_correct_ev(s, a);
    a.selected_bet_tier = pe::BetTier::HalfPot;
    const pe::GradingResult r = pe::evaluate(s, a);
    EXPECT_TRUE(find(r, pe::InputId::BetSize)->correct);
    EXPECT_TRUE(r.all_correct);
    EXPECT_EQ(find(r, pe::InputId::BetSize)->correct_value,
              static_cast<double>(static_cast<std::uint8_t>(pe::BetTier::FullPot)));
}

TEST(Grading, AggressorDominatedTierIsRejected) {
    // One Third Pot (9.0) is far below 13.0 - 0.65, so it is graded wrong.
    const pe::ScenarioState s = make_aggressor(pe::ScenarioType::AggressorPureBluff,
                                               {9.0, 12.9, 13.0, 8.0});
    pe::UserAnswers a{};
    fill_correct_breakeven(s, a);
    fill_correct_ev(s, a);
    a.selected_bet_tier = pe::BetTier::OneThirdPot;
    const pe::GradingResult r = pe::evaluate(s, a);
    EXPECT_FALSE(find(r, pe::InputId::BetSize)->correct);
    EXPECT_FALSE(r.all_correct);
}

TEST(Grading, AggressorSingleTierGradesOnlyPresentedTier) {
    pe::ScenarioState s = make_aggressor(pe::ScenarioType::AggressorPureBluff,
                                         {10.0, 11.0, 12.0, 13.0});
    s.multi_tier = false;
    s.presented_tier = pe::BetTier::HalfPot;
    const auto pt = static_cast<std::size_t>(pe::BetTier::HalfPot);
    pe::UserAnswers a{};
    a.tier_breakeven_pct[pt] =
        breakeven_pct(static_cast<double>(s.pot), s.tiers[pt].bet_dollars);
    a.tier_ev[pt] = s.tiers[pt].ev;
    a.selected_bet_tier = pe::BetTier::Overbet;
    const pe::GradingResult r = pe::evaluate(s, a);
    // Only the presented tier's breakeven + EV + Bet Size are graded (3 inputs).
    EXPECT_EQ(r.inputs.size(), 3u);
    EXPECT_EQ(find_tier(r, pe::InputId::FoldProbability, 0), nullptr);
    EXPECT_NE(find_tier(r, pe::InputId::FoldProbability, 1), nullptr);
    EXPECT_NE(find_tier(r, pe::InputId::Ev, 1), nullptr);
    EXPECT_TRUE(r.all_correct);
}
