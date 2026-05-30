#include "engine/evaluator.hpp"

#include <algorithm>
#include <cmath>
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

InputGrade grade_bet_size(std::optional<BetTier> submitted, BetTier correct_tier) noexcept {
    InputGrade g{};
    g.input = InputId::BetSize;
    g.correct_value = static_cast<double>(static_cast<std::uint8_t>(correct_tier));
    g.margin = 0.0;  // exact tier match
    if (submitted.has_value()) {
        g.submitted = static_cast<double>(static_cast<std::uint8_t>(*submitted));
        g.correct = (*submitted == correct_tier);
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
        const auto grade_tier = [&](std::uint8_t t) {
            const AggressorTier& tier = state.tiers[t];
            grades.push_back(grade_probability(InputId::FoldProbability, t,
                                               answers.tier_fold_pct[t],
                                               tier.fold_probability * 100.0));
            grades.push_back(grade_ev(t, answers.tier_ev[t], tier.ev));
        };

        if (state.multi_tier) {
            for (std::uint8_t t = 0; t < kBetTierCount; ++t) {
                grade_tier(t);
            }
        } else {
            grade_tier(static_cast<std::uint8_t>(state.presented_tier));
        }

        if (state.type == ScenarioType::AggressorSemiBluff) {
            grades.push_back(grade_probability(InputId::Equity, std::nullopt,
                                               answers.equity_if_called_pct,
                                               state.aggressor_equity_pct));
        }

        grades.push_back(grade_bet_size(answers.selected_bet_tier, state.correct_bet_tier));
    }

    result.all_correct = std::all_of(grades.begin(), grades.end(),
                                     [](const InputGrade& g) { return g.correct; });
    return result;
}

bool is_pass(const GradingResult& result) noexcept {
    return result.all_correct;
}

}  // namespace poker_trainer::engine
