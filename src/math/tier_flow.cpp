#include "math/tier_flow.hpp"

#include "math/input_boxes.hpp"
#include "math/interrogator.hpp"

#include "engine/scenario.hpp"

#include <cstddef>
#include <cstdint>

namespace poker_trainer::interrogator {

bool is_sequential(const InterrogatorState& state) noexcept {
    return state.scenario.has_value() && engine::is_aggressor(state.scenario->type) &&
           state.scenario->multi_tier;
}

bool is_last_tier(const InterrogatorState& state) noexcept {
    return static_cast<std::size_t>(state.current_tier) + 1U >= engine::kBetTierCount;
}

bool current_tier_required_filled(const InterrogatorState& state) noexcept {
    // The current screen's boxes are exactly this tier's required inputs (Fold, EV,
    // plus the tier-1 Equity-if-Called); the Bet Size group is not a box and never
    // gates. current_view_boxes already filters to the current tier.
    for (const NumericBox* box : current_view_boxes(state)) {
        if (!box_filled(*box)) {
            return false;
        }
    }
    return true;
}

EnterAction enter_action(const InterrogatorState& state) noexcept {
    if (!current_tier_required_filled(state)) {
        return EnterAction::None;
    }
    return is_last_tier(state) ? EnterAction::Submit : EnterAction::Advance;
}

std::uint8_t next_tier(std::uint8_t current) noexcept {
    const auto last = static_cast<std::uint8_t>(engine::kBetTierCount - 1U);
    return current < last ? static_cast<std::uint8_t>(current + 1) : last;
}

}  // namespace poker_trainer::interrogator
