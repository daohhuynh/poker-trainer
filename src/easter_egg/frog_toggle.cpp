#include "easter_egg/frog_toggle.hpp"

namespace poker_trainer::easter_egg {

FrogClickOutcome register_dealer_click(FrogToggleState& state) noexcept {
    ++state.consecutive_clicks;
    if (state.consecutive_clicks >= kFrogToggleClicks) {
        state.consecutive_clicks = 0;
        state.frog_active = !state.frog_active;
        return FrogClickOutcome::Toggled;
    }
    return FrogClickOutcome::Counting;
}

void reset_click_count(FrogToggleState& state) noexcept { state.consecutive_clicks = 0; }

bool frog_active(const FrogToggleState& state) noexcept { return state.frog_active; }

}  // namespace poker_trainer::easter_egg
