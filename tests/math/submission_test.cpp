// Zone 09 — submission + grading bridge.
//
// These exercise Z09's responsibility: assembling a UserAnswers from the typed
// boxes + bet selection and driving Z01's locked evaluator. The margin VALUES
// are Z01's (and unit-tested in evaluator_test); here we confirm Z09's parse +
// assembly path produces the correct grade at and beyond each boundary, that
// unfilled boxes / an unselected Bet Size grade wrong, that the bet-size-
// independent Equity-if-Called is carried once and reused across tiers, that
// Enter aggregates every visible input, and that math+time combine into pass.

#include "math/submission.hpp"
#include "math/input_boxes.hpp"
#include "math/interrogator.hpp"

#include "backbone/scenario_events.hpp"
#include "engine/scenario.hpp"

#include <cstdint>
#include <cstring>
#include <optional>

#include <gtest/gtest.h>

namespace it = poker_trainer::interrogator;
namespace eng = poker_trainer::engine;
namespace bb = poker_trainer::backbone;

namespace {

eng::ScenarioState caller_truth(double pot_odds, int outs, double equity, double ev) {
    eng::ScenarioState s{};
    s.id = eng::ScenarioId{42};
    s.type = eng::ScenarioType::Caller;
    s.caller_pot_odds_pct = pot_odds;
    s.caller_outs = outs;
    s.caller_equity_pct = equity;
    s.caller_ev = ev;
    return s;
}

// A multi-tier Semi-Bluff with known per-tier and bet-size-independent truth.
eng::ScenarioState semibluff_multitier_truth() {
    eng::ScenarioState s{};
    s.id = eng::ScenarioId{7};
    s.type = eng::ScenarioType::AggressorSemiBluff;
    s.multi_tier = true;
    s.aggressor_equity_pct = 45.0;
    const double folds[eng::kBetTierCount] = {0.30, 0.40, 0.50, 0.60};
    const double evs[eng::kBetTierCount] = {10.0, 12.0, 13.0, 8.0};
    for (std::size_t i = 0; i < eng::kBetTierCount; ++i) {
        s.tiers[i].tier = static_cast<eng::BetTier>(static_cast<std::uint8_t>(i));
        s.tiers[i].fold_probability = folds[i];
        s.tiers[i].ev = evs[i];
    }
    s.correct_bet_tier = eng::BetTier::FullPot;  // max EV (13.0)
    return s;
}

void set_box(it::InterrogatorState& state, eng::InputId id, std::optional<std::uint8_t> tier,
             const char* text) {
    for (it::NumericBox& b : state.boxes) {
        if (b.input == id && b.tier == tier) {
            std::strncpy(b.text.data(), text, b.text.size() - 1);
            return;
        }
    }
    ADD_FAILURE() << "no box for the requested input/tier";
}

const eng::InputGrade* find(const eng::GradingResult& r, eng::InputId id) {
    for (const eng::InputGrade& g : r.inputs) {
        if (g.input == id && !g.tier_index.has_value()) {
            return &g;
        }
    }
    return nullptr;
}

std::size_t count_grade(const eng::GradingResult& r, eng::InputId id) {
    std::size_t n = 0;
    for (const eng::InputGrade& g : r.inputs) {
        if (g.input == id) {
            ++n;
        }
    }
    return n;
}

}  // namespace

// ----- Probability ±5 percentage-point band (through Z09) -----

TEST(Grading, ProbabilityWithinFivePointsAtBoundary) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, caller_truth(30.0, 8, 40.0, 5.0));

    set_box(state, eng::InputId::PotOdds, std::nullopt, "35");  // exactly +5pp
    EXPECT_TRUE(find(it::grade(state), eng::InputId::PotOdds)->correct);

    set_box(state, eng::InputId::PotOdds, std::nullopt, "25");  // exactly -5pp
    EXPECT_TRUE(find(it::grade(state), eng::InputId::PotOdds)->correct);
}

TEST(Grading, ProbabilityBeyondFivePointsIsWrong) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, caller_truth(30.0, 8, 40.0, 5.0));
    set_box(state, eng::InputId::PotOdds, std::nullopt, "35.01");
    EXPECT_FALSE(find(it::grade(state), eng::InputId::PotOdds)->correct);
}

// ----- EV: 5% relative with the $0.50 absolute floor (through Z09) -----

TEST(Grading, EvHalfDollarFloorOnSmallEv) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, caller_truth(30.0, 8, 40.0, 0.20));  // floor dominates
    set_box(state, eng::InputId::Ev, std::nullopt, "0.70");                // diff 0.50
    EXPECT_TRUE(find(it::grade(state), eng::InputId::Ev)->correct);
    set_box(state, eng::InputId::Ev, std::nullopt, "0.71");               // beyond the floor
    EXPECT_FALSE(find(it::grade(state), eng::InputId::Ev)->correct);
}

TEST(Grading, EvFivePercentOnLargeEv) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, caller_truth(30.0, 8, 40.0, 100.0));  // 5% = 5.0
    set_box(state, eng::InputId::Ev, std::nullopt, "105");
    EXPECT_TRUE(find(it::grade(state), eng::InputId::Ev)->correct);
    set_box(state, eng::InputId::Ev, std::nullopt, "105.1");
    EXPECT_FALSE(find(it::grade(state), eng::InputId::Ev)->correct);
}

TEST(Grading, NegativeEvParsesAndGrades) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, caller_truth(30.0, 8, 40.0, -12.0));  // margin 0.6
    set_box(state, eng::InputId::Ev, std::nullopt, "-12.5");
    EXPECT_TRUE(find(it::grade(state), eng::InputId::Ev)->correct);
}

// ----- Outs: exact integer match (through Z09) -----

TEST(Grading, OutsRequireExactMatch) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, caller_truth(30.0, 8, 40.0, 5.0));
    set_box(state, eng::InputId::Outs, std::nullopt, "8");
    EXPECT_TRUE(find(it::grade(state), eng::InputId::Outs)->correct);
    set_box(state, eng::InputId::Outs, std::nullopt, "7");
    EXPECT_FALSE(find(it::grade(state), eng::InputId::Outs)->correct);
}

// ----- Unfilled box / unselected bet size grade wrong -----

TEST(Grading, UnfilledBoxIsWrong) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, caller_truth(30.0, 8, 40.0, 5.0));
    set_box(state, eng::InputId::PotOdds, std::nullopt, "30");
    set_box(state, eng::InputId::Outs, std::nullopt, "8");
    set_box(state, eng::InputId::Equity, std::nullopt, "40");
    // EV left blank.
    const eng::GradingResult r = it::grade(state);
    EXPECT_FALSE(find(r, eng::InputId::Ev)->correct);
    EXPECT_FALSE(r.all_correct);
}

TEST(Grading, UnselectedBetSizeIsWrong) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, semibluff_multitier_truth());
    // Fill every numeric box correctly but never select a bet tier.
    set_box(state, eng::InputId::Equity, std::nullopt, "45");
    for (std::uint8_t t = 0; t < eng::kBetTierCount; ++t) {
        set_box(state, eng::InputId::FoldProbability, t,
                t == 0 ? "30" : t == 1 ? "40" : t == 2 ? "50" : "60");
        set_box(state, eng::InputId::Ev, t,
                t == 0 ? "10" : t == 1 ? "12" : t == 2 ? "13" : "8");
    }
    ASSERT_FALSE(state.bet_group.selected.has_value());
    const eng::GradingResult r = it::grade(state);
    EXPECT_FALSE(find(r, eng::InputId::BetSize)->correct);
    EXPECT_FALSE(r.all_correct);
}

// ----- Multi-tier: Equity-if-Called locked once, reused across all tiers -----

TEST(Grading, EquityIfCalledGradedOnceAcrossAllTiers) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, semibluff_multitier_truth());
    set_box(state, eng::InputId::Equity, std::nullopt, "45");
    for (std::uint8_t t = 0; t < eng::kBetTierCount; ++t) {
        set_box(state, eng::InputId::FoldProbability, t,
                t == 0 ? "30" : t == 1 ? "40" : t == 2 ? "50" : "60");
        set_box(state, eng::InputId::Ev, t,
                t == 0 ? "10" : t == 1 ? "12" : t == 2 ? "13" : "8");
    }
    state.bet_group.selected = eng::BetTier::FullPot;

    const eng::GradingResult r = it::grade(state);
    // One Equity grade for the whole multi-tier scenario (not one per tier).
    EXPECT_EQ(count_grade(r, eng::InputId::Equity), 1u);
    const eng::InputGrade* equity = find(r, eng::InputId::Equity);
    ASSERT_NE(equity, nullptr);
    EXPECT_TRUE(equity->correct);
    EXPECT_DOUBLE_EQ(equity->correct_value, 45.0);
    // Per-tier inputs are graded for every tier.
    EXPECT_EQ(count_grade(r, eng::InputId::FoldProbability), eng::kBetTierCount);
    EXPECT_EQ(count_grade(r, eng::InputId::Ev), eng::kBetTierCount);
}

// ----- Enter submits ALL visible inputs at once -----

TEST(Submission, OnSubmitAggregatesAllInputsAndFiresBusEvent) {
    bb::reset_scenario_events_for_testing();
    bool fired = false;
    eng::ScenarioId fired_id{};
    const bb::SubscriberHandle h = bb::subscribe_answers_submitted(
        [&](const bb::AnswersSubmittedEvent& ev) {
            fired = true;
            fired_id = ev.scenario_id;
        },
        "test");

    it::InterrogatorRuntime runtime{};
    it::configure_for_scenario(runtime.state, semibluff_multitier_truth());
    set_box(runtime.state, eng::InputId::Equity, std::nullopt, "45");
    for (std::uint8_t t = 0; t < eng::kBetTierCount; ++t) {
        set_box(runtime.state, eng::InputId::FoldProbability, t,
                t == 0 ? "30" : t == 1 ? "40" : t == 2 ? "50" : "60");
        set_box(runtime.state, eng::InputId::Ev, t,
                t == 0 ? "10" : t == 1 ? "12" : t == 2 ? "13" : "8");
    }
    runtime.state.bet_group.selected = eng::BetTier::FullPot;

    const eng::GradingResult r = it::on_submit(runtime);
    EXPECT_TRUE(r.all_correct);
    EXPECT_TRUE(runtime.state.last_math_pass);
    ASSERT_TRUE(runtime.state.last_result.has_value());
    EXPECT_TRUE(runtime.state.last_result->all_correct);
    EXPECT_TRUE(fired);
    EXPECT_EQ(fired_id, eng::ScenarioId{7});

    bb::unsubscribe(h);
    bb::reset_scenario_events_for_testing();
}

TEST(Submission, AllVisibleInputsFilledReflectsBetSelection) {
    it::InterrogatorState state{};
    it::configure_for_scenario(
        state, [] {
            eng::ScenarioState s{};
            s.type = eng::ScenarioType::AggressorPureBluff;
            s.multi_tier = false;
            s.presented_tier = eng::BetTier::HalfPot;
            return s;
        }());
    set_box(state, eng::InputId::FoldProbability, static_cast<std::uint8_t>(1), "40");
    set_box(state, eng::InputId::Ev, static_cast<std::uint8_t>(1), "12");
    EXPECT_FALSE(it::all_visible_inputs_filled(state));  // bet size not yet selected
    state.bet_group.selected = eng::BetTier::HalfPot;
    EXPECT_TRUE(it::all_visible_inputs_filled(state));
}

// ----- Pass/fail: all-correct AND within-target-time (Z10 injected) -----

TEST(PassFail, OverallPassRequiresMathAndTime) {
    eng::GradingResult pass{};
    pass.all_correct = true;
    eng::GradingResult fail{};
    fail.all_correct = false;

    EXPECT_TRUE(it::compute_pass(pass, /*within_target_time=*/true).overall_pass);
    EXPECT_FALSE(it::compute_pass(pass, /*within_target_time=*/false).overall_pass);
    EXPECT_FALSE(it::compute_pass(fail, /*within_target_time=*/true).overall_pass);

    const it::PassState ps = it::compute_pass(pass, false);
    EXPECT_TRUE(ps.math_correct);
    EXPECT_FALSE(ps.within_target_time);
}
