#pragma once

#include "engine/scenario.hpp"

// The True Evaluator (Module 1): the locked V8.1 EV formulas, pot odds, the
// Caller's net-call EV, and the Post-Round grader.
//
// The three Aggressor EV formulas are LOCKED at V8.1 (CLAUDE.md sec.5) and are
// defined in evaluator.cpp. Probabilities passed to them are fractions in
// [0, 1]; pot/bet are dollars; the return is dollars.

namespace poker_trainer::engine {

// ----- Locked V8.1 EV formulas (do not modify) -----

// Pure Bluff: EV = P(fold) * pot - P(call) * bet,  with P(call) = 1 - P(fold).
[[nodiscard]] double pure_bluff_ev(double p_fold, double pot, double bet) noexcept;

// Value Bet: EV = P(call) * bet.
[[nodiscard]] double value_bet_ev(double p_call, double bet) noexcept;

// Semi-Bluff: EV = P(fold) * pot + P(call) * [equity * (pot + 2*bet) - bet].
// `equity` is a fraction in [0, 1].
[[nodiscard]] double semi_bluff_ev(double p_fold, double equity, double pot, double bet) noexcept;

// ----- Caller math -----

// Net expected value of calling: EV = equity * (pot + bet) - (1 - equity) * bet.
// A fourth formula (the three above cover only the Aggressor; the gap was
// escalated and resolved to net call EV). `equity` is a fraction in [0, 1].
[[nodiscard]] double net_call_ev(double equity, double pot, double bet) noexcept;

// Pot odds as a fraction: bet / (pot + bet). Returns 0 when pot + bet <= 0.
[[nodiscard]] double pot_odds_fraction(double pot, double bet) noexcept;

// ----- Grading margins (Module 5) -----

// Probabilities (Pot Odds, Equity, Fold Probability): +/- 5 percentage points.
inline constexpr double kProbabilityMarginPp = 5.0;
// Dollar EV: +/- 5% relative, with a $0.50 absolute floor.
inline constexpr double kEvRelativeMargin = 0.05;
inline constexpr double kEvAbsoluteFloor = 0.50;

// The grading tolerance applied to a dollar EV answer: max($0.50, 5% * |ev|).
[[nodiscard]] double ev_margin(double correct_ev) noexcept;

// ----- Grading -----

// Grade `answers` against the truth stored in `state`, producing one InputGrade
// per input the scenario asks for (unfilled boxes and an unselected Bet Size are
// graded incorrect). `all_correct` is set when every graded input is correct.
[[nodiscard]] GradingResult evaluate(const ScenarioState& state, const UserAnswers& answers);

// True when every math input passed. This is the math component of a pass; the
// tomato-award / dealer-expression "pass" (Module 7) additionally requires the
// scenario time to be at or under target, which the engine does not track (Z10
// supplies it). Callers combine: is_pass(grade) && time_under_target.
[[nodiscard]] bool is_pass(const GradingResult& result) noexcept;

}  // namespace poker_trainer::engine
