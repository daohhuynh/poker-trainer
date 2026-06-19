#include "screens/again_button.hpp"

namespace poker_trainer::screens {

AgainPressOutcome press_again(AgainButton& button) noexcept {
    if (button.state == AgainState::Default) {
        button.state = AgainState::Armed;
        return AgainPressOutcome::Armed;
    }
    // Armed -> commit. Reset so a stale "CONFIRM" never lingers if the button is
    // re-rendered before the screen transitions away.
    button.state = AgainState::Default;
    return AgainPressOutcome::Committed;
}

void reset_again(AgainButton& button) noexcept {
    button.state = AgainState::Default;
}

const char* again_label(AgainState state) noexcept {
    return state == AgainState::Armed ? "CONFIRM" : "AGAIN";
}

}  // namespace poker_trainer::screens
