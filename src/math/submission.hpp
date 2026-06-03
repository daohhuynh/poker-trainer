#pragma once

#include "math/interrogator.hpp"

#include "engine/scenario.hpp"

// Submission + grading bridge (Module 5). Z09 gathers the typed answers, hands
// them to Z01's locked evaluator, stores the result, and produces the pass/fail
// the dealer-expression layer consumes. It owns NO truth or margin math.

namespace poker_trainer::interrogator {

// Assemble UserAnswers from the typed boxes + bet selection. Unfilled boxes and
// an unselected Bet Size remain nullopt (graded incorrect by the evaluator).
// The single bet-size-independent Equity-if-Called value (Aggressor Semi-Bluff)
// is carried once and reused for every tier by the evaluator -- Module 5 output
// #3 (echoed at every tier tab in Z13's recap).
[[nodiscard]] engine::UserAnswers gather_answers(const InterrogatorState& state);

// True when every visible numeric box parses to a value AND (no bet group, or a
// tier is selected). The tutorial Enter override gates submission on this across
// all tiers; standard gameplay does not.
[[nodiscard]] bool all_visible_inputs_filled(const InterrogatorState& state);

// Grade the gathered answers against the cached scenario via Z01's evaluate()
// (the locked V8.1 truth + margins). Stores the result and math-pass in `state`
// and returns it. Returns an empty result when no scenario is cached.
engine::GradingResult grade(InterrogatorState& state);

// Module 5 output #2: combine math-correctness with the within-target-time flag.
// pass = all inputs correct AND time at/under target. The time component is
// Z10's (W4); callers inject it -- // SEAM(Z10).
[[nodiscard]] PassState compute_pass(const engine::GradingResult& result,
                                     bool within_target_time) noexcept;

// The Enter submission path (Z09's `on_submit` export). Fires the
// AnswersSubmitted bus event, then grades and stores. Returns the GradingResult.
// The Game->Post-Round transition (Z14) and the GradingComplete bus event (needs
// Z10's elapsed_ms) are integration seams fired by the transition layer, not
// here -- // SEAM(Z14), // SEAM(Z10).
engine::GradingResult on_submit(InterrogatorRuntime& runtime);

}  // namespace poker_trainer::interrogator
