#include "math/submission.hpp"

#include "math/input_boxes.hpp"
#include "math/interrogator.hpp"

#include "backbone/scenario_events.hpp"
#include "engine/evaluator.hpp"
#include "engine/scenario.hpp"

#include <cstddef>
#include <cstdint>

namespace poker_trainer::interrogator {

namespace {

[[nodiscard]] std::size_t tier_index(std::uint8_t tier) noexcept {
    return static_cast<std::size_t>(tier);
}

[[nodiscard]] bool box_filled(const NumericBox& box) noexcept {
    return (box.input == engine::InputId::Outs) ? parse_box_int(box).has_value()
                                                : parse_box_double(box).has_value();
}

}  // namespace

engine::UserAnswers gather_answers(const InterrogatorState& state) {
    engine::UserAnswers answers{};
    if (!state.scenario.has_value()) {
        return answers;
    }
    const bool caller = (state.scenario->type == engine::ScenarioType::Caller);

    for (const NumericBox& box : state.boxes) {
        switch (box.input) {
            case engine::InputId::PotOdds:
                answers.pot_odds_pct = parse_box_double(box);
                break;
            case engine::InputId::Outs:
                answers.outs = parse_box_int(box);
                break;
            case engine::InputId::Equity:
                // One Equity box per scenario: the Caller's Equity, or the
                // Aggressor Semi-Bluff's bet-size-independent Equity-if-Called.
                if (caller) {
                    answers.caller_equity_pct = parse_box_double(box);
                } else {
                    answers.equity_if_called_pct = parse_box_double(box);
                }
                break;
            case engine::InputId::Ev:
                if (box.tier.has_value()) {
                    answers.tier_ev[tier_index(*box.tier)] = parse_box_double(box);
                } else {
                    answers.caller_ev = parse_box_double(box);
                }
                break;
            case engine::InputId::FoldProbability:
                if (box.tier.has_value()) {
                    answers.tier_fold_pct[tier_index(*box.tier)] = parse_box_double(box);
                }
                break;
            case engine::InputId::BetSize:
                break;  // Bet Size is the group, not a numeric box.
        }
    }

    answers.selected_bet_tier = state.bet_group.selected;
    return answers;
}

bool all_visible_inputs_filled(const InterrogatorState& state) {
    for (const NumericBox& box : state.boxes) {
        if (!box_filled(box)) {
            return false;
        }
    }
    if (state.bet_group.present && !state.bet_group.selected.has_value()) {
        return false;
    }
    return true;
}

engine::GradingResult grade(InterrogatorState& state) {
    engine::GradingResult result{};
    if (state.scenario.has_value()) {
        result = engine::evaluate(*state.scenario, gather_answers(state));
    }
    state.last_math_pass = engine::is_pass(result);
    state.last_result = result;
    return result;
}

PassState compute_pass(const engine::GradingResult& result, bool within_target_time) noexcept {
    PassState pass{};
    pass.math_correct = engine::is_pass(result);
    pass.within_target_time = within_target_time;
    pass.overall_pass = pass.math_correct && within_target_time;
    return pass;
}

engine::GradingResult on_submit(InterrogatorRuntime& runtime) {
    InterrogatorState& state = runtime.state;
    if (!state.scenario.has_value()) {
        return grade(state);  // no active scenario: empty result, no bus events
    }
    const engine::ScenarioId id = state.scenario->id;

    // AnswersSubmitted fires first (the submission gesture, before grading runs).
    backbone::fire_answers_submitted(backbone::AnswersSubmittedEvent{id});

    const engine::GradingResult result = grade(state);

    // GradingComplete fires once the True Evaluator has produced the result. Z09
    // is the firer: it owns the evaluate() call, holds the GradingResult, and owns
    // the single SEAM(Z10) elapsed-time injection point (see the report's firer
    // decision). `passed` carries the math verdict (is_pass); consumers combine it
    // with elapsed_ms (against the target time) for the time-aware overall pass.
    const std::uint32_t elapsed_ms =
        runtime.elapsed_ms_source ? runtime.elapsed_ms_source() : 0U;  // SEAM(Z10)
    backbone::fire_grading_complete(
        backbone::GradingCompleteEvent{id, engine::is_pass(result), elapsed_ms});
    return result;
}

}  // namespace poker_trainer::interrogator
