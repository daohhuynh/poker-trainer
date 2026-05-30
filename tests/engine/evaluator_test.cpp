// True Evaluator tests. The locked-V8.1 EV worked examples from Module 1 are the
// acceptance criteria and must reproduce the architecture's numbers exactly; the
// remainder cover pot odds, net call EV, and the Post-Round grader's margins.

#include "engine/evaluator.hpp"

#include "engine/scenario.hpp"

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

namespace {

pe::ScenarioState make_aggressor_multitier() {
    pe::ScenarioState s{};
    s.type = pe::ScenarioType::AggressorPureBluff;
    s.pot = 100;
    s.multi_tier = true;
    for (std::uint8_t t = 0; t < pe::kBetTierCount; ++t) {
        pe::AggressorTier tier{};
        tier.tier = static_cast<pe::BetTier>(t);
        tier.fold_probability = 0.30 + 0.05 * static_cast<double>(t);  // 0.30..0.45
        tier.call_probability = 1.0 - tier.fold_probability;
        tier.ev = 10.0 + static_cast<double>(t);  // 10..13, best is tier 3
        s.tiers[t] = tier;
    }
    s.correct_bet_tier = pe::BetTier::Overbet;
    s.presented_tier = pe::BetTier::Overbet;
    return s;
}

}  // namespace

TEST(Grading, AggressorPerTierAndBetSizeAllCorrect) {
    const pe::ScenarioState s = make_aggressor_multitier();
    pe::UserAnswers a{};
    for (std::uint8_t t = 0; t < pe::kBetTierCount; ++t) {
        a.tier_fold_pct[t] = s.tiers[t].fold_probability * 100.0;  // exact
        a.tier_ev[t] = s.tiers[t].ev;                              // exact
    }
    a.selected_bet_tier = pe::BetTier::Overbet;
    const pe::GradingResult r = pe::evaluate(s, a);
    EXPECT_TRUE(r.all_correct);
    EXPECT_EQ(find_tier(r, pe::InputId::FoldProbability, 0)->correct_value, 30.0);
}

TEST(Grading, AggressorWrongBetTierFailsBetSize) {
    const pe::ScenarioState s = make_aggressor_multitier();
    pe::UserAnswers a{};
    for (std::uint8_t t = 0; t < pe::kBetTierCount; ++t) {
        a.tier_fold_pct[t] = s.tiers[t].fold_probability * 100.0;
        a.tier_ev[t] = s.tiers[t].ev;
    }
    a.selected_bet_tier = pe::BetTier::HalfPot;  // not the max-EV tier
    const pe::GradingResult r = pe::evaluate(s, a);
    EXPECT_FALSE(find(r, pe::InputId::BetSize)->correct);
    EXPECT_FALSE(r.all_correct);
}

TEST(Grading, AggressorSingleTierGradesOnlyPresentedTier) {
    pe::ScenarioState s = make_aggressor_multitier();
    s.multi_tier = false;
    s.presented_tier = pe::BetTier::HalfPot;
    pe::UserAnswers a{};
    a.tier_fold_pct[static_cast<std::size_t>(pe::BetTier::HalfPot)] =
        s.tiers[static_cast<std::size_t>(pe::BetTier::HalfPot)].fold_probability * 100.0;
    a.tier_ev[static_cast<std::size_t>(pe::BetTier::HalfPot)] =
        s.tiers[static_cast<std::size_t>(pe::BetTier::HalfPot)].ev;
    a.selected_bet_tier = pe::BetTier::Overbet;
    const pe::GradingResult r = pe::evaluate(s, a);
    // Only the presented tier's fold/ev plus BetSize are graded (3 inputs).
    EXPECT_EQ(r.inputs.size(), 3u);
    EXPECT_EQ(find_tier(r, pe::InputId::FoldProbability, 0), nullptr);
    EXPECT_NE(find_tier(r, pe::InputId::FoldProbability, 1), nullptr);
}
