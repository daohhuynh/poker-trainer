#include "engine/evaluator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace poker_trainer::engine {

// ===== Locked V8.1 EV formulas =====
// CLAUDE.md sec.5: these are frozen at V8.1 and verified at extreme values
// (see tests/engine/evaluator_test.cpp). Do not modify without owner approval.

double pure_bluff_ev(double p_fold, double pot, double bet) noexcept {
    return p_fold * pot - (1.0 - p_fold) * bet;
}

double value_bet_ev(double p_call, double bet) noexcept {
    return p_call * bet;
}

double semi_bluff_ev(double p_fold, double equity, double pot, double bet) noexcept {
    const double p_call = 1.0 - p_fold;
    return p_fold * pot + p_call * (equity * (pot + 2.0 * bet) - bet);
}

// ===== Caller math =====

double net_call_ev(double equity, double pot, double bet) noexcept {
    return equity * (pot + bet) - (1.0 - equity) * bet;
}

double pot_odds_fraction(double pot, double bet) noexcept {
    const double denom = pot + bet;
    if (denom <= 0.0) {
        return 0.0;
    }
    return bet / denom;
}

// ===== Grading =====

double ev_margin(double correct_ev) noexcept {
    return std::max(kEvAbsoluteFloor, kEvRelativeMargin * std::abs(correct_ev));
}

namespace {

bool within(double value, double target, double margin) noexcept {
    return std::abs(value - target) <= margin;
}

InputGrade grade_probability(InputId id,
                             std::optional<std::uint8_t> tier,
                             std::optional<double> submitted,
                             double correct_pct) noexcept {
    InputGrade g{};
    g.input = id;
    g.tier_index = tier;
    g.correct_value = correct_pct;
    g.submitted = submitted;
    g.margin = kProbabilityMarginPp;
    g.correct = submitted.has_value() && within(*submitted, correct_pct, kProbabilityMarginPp);
    return g;
}

InputGrade grade_ev(std::optional<std::uint8_t> tier,
                    std::optional<double> submitted,
                    double correct_ev) noexcept {
    InputGrade g{};
    g.input = InputId::Ev;
    g.tier_index = tier;
    g.correct_value = correct_ev;
    g.submitted = submitted;
    const double margin = ev_margin(correct_ev);
    g.margin = margin;
    g.correct = submitted.has_value() && within(*submitted, correct_ev, margin);
    return g;
}

InputGrade grade_outs(std::optional<int> submitted, int correct_outs) noexcept {
    InputGrade g{};
    g.input = InputId::Outs;
    g.correct_value = static_cast<double>(correct_outs);
    g.margin = 0.0;  // exact integer match
    if (submitted.has_value()) {
        g.submitted = static_cast<double>(*submitted);
        g.correct = (*submitted == correct_outs);
    }
    return g;
}

InputGrade grade_bet_size(std::optional<BetTier> submitted,
                          const std::array<AggressorTier, kBetTierCount>& tiers,
                          BetTier max_ev_tier) noexcept {
    // Tolerant tier grading: the max-EV tier is the reference, and a chosen tier
    // is correct iff its EV is within the EV grading tolerance of the max-EV
    // tier's EV -- the SAME propagated tolerance used to grade the EV numeric
    // answer (ev_margin). This absorbs the +/-5pp Rule-of-2&4 equity slop and
    // scales with pot size, so tiers statistically tied with the best pass.
    const double max_ev = tiers[static_cast<std::size_t>(max_ev_tier)].ev;
    const double tol = ev_margin(max_ev);
    InputGrade g{};
    g.input = InputId::BetSize;
    g.correct_value = static_cast<double>(static_cast<std::uint8_t>(max_ev_tier));
    g.margin = tol;  // EV-space tolerance band (informational for Z13's recap)
    if (submitted.has_value()) {
        g.submitted = static_cast<double>(static_cast<std::uint8_t>(*submitted));
        const double chosen_ev = tiers[static_cast<std::size_t>(*submitted)].ev;
        g.correct = chosen_ev >= max_ev - tol;
    }
    return g;
}

}  // namespace

GradingResult evaluate(const ScenarioState& state, const UserAnswers& answers) {
    GradingResult result{};
    auto& grades = result.inputs;

    if (state.type == ScenarioType::Caller) {
        grades.push_back(grade_probability(InputId::PotOdds, std::nullopt,
                                           answers.pot_odds_pct, state.caller_pot_odds_pct));
        grades.push_back(grade_outs(answers.outs, state.caller_outs));
        grades.push_back(grade_probability(InputId::Equity, std::nullopt,
                                           answers.caller_equity_pct, state.caller_equity_pct));
        grades.push_back(grade_ev(std::nullopt, answers.caller_ev, state.caller_ev));
    } else {
        // Aggressor inputs (all derivable now that the opponent fold % is shown):
        //   Pure Bluff:  Breakeven Fold % (per tier), EV (per tier), Bet Size.
        //   Value Bet:   Equity if Called, EV (per tier), Bet Size.
        //   Semi-Bluff:  Breakeven Fold % (per tier), Equity if Called, EV (per tier), Bet Size.
        // Breakeven Fold % = bet / (pot + bet) -- the aggressor's mirror of pot odds.
        // EV is the locked V8.1 formula output at the tier's bet using the shown F
        // (stored per tier in state.tiers[t].ev), so each tier's EV is derivable from
        // the shown fold %, the pot, and the tier's size. Bet Size grades against the
        // max-EV tier (correct_bet_tier).
        const bool asks_breakeven = (state.type == ScenarioType::AggressorPureBluff ||
                                     state.type == ScenarioType::AggressorSemiBluff);
        const bool asks_equity_if_called = (state.type == ScenarioType::AggressorValueBet ||
                                            state.type == ScenarioType::AggressorSemiBluff);
        const double pot_d = static_cast<double>(state.pot);

        const auto grade_breakeven = [&](std::uint8_t t) {
            const AggressorTier& tier = state.tiers[t];
            const double breakeven_pct = 100.0 * pot_odds_fraction(pot_d, tier.bet_dollars);
            grades.push_back(grade_probability(InputId::FoldProbability, t,
                                               answers.tier_breakeven_pct[t], breakeven_pct));
        };
        const auto grade_tier_ev = [&](std::uint8_t t) {
            grades.push_back(grade_ev(t, answers.tier_ev[t], state.tiers[t].ev));
        };
        const auto grade_equity_if_called = [&] {
            grades.push_back(grade_probability(InputId::Equity, std::nullopt,
                                               answers.equity_if_called_pct,
                                               state.aggressor_equity_pct));
        };

        if (state.multi_tier) {
            // Per tier: Breakeven Fold % then EV; the scenario-level Equity if Called
            // and Bet Size close the list (they render in the recap's Overall section).
            for (std::uint8_t t = 0; t < kBetTierCount; ++t) {
                if (asks_breakeven) {
                    grade_breakeven(t);
                }
                grade_tier_ev(t);
            }
            if (asks_equity_if_called) {
                grade_equity_if_called();
            }
        } else {
            // Single tier, one screen: grade in on-screen input order
            // (Breakeven Fold %, Equity if Called, EV) then Bet Size.
            const auto t = static_cast<std::uint8_t>(state.presented_tier);
            if (asks_breakeven) {
                grade_breakeven(t);
            }
            if (asks_equity_if_called) {
                grade_equity_if_called();
            }
            grade_tier_ev(t);
        }

        grades.push_back(grade_bet_size(answers.selected_bet_tier, state.tiers, state.correct_bet_tier));
    }

    result.all_correct = std::all_of(grades.begin(), grades.end(),
                                     [](const InputGrade& g) { return g.correct; });
    return result;
}

bool is_pass(const GradingResult& result) noexcept {
    return result.all_correct;
}

}  // namespace poker_trainer::engine
