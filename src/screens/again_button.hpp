#pragma once

#include <cstdint>

// Zone 13 — the Again button's double-confirm state machine (Post-Round Screen,
// bottom-right). ARCHITECTURE Post-Round "Again button": default label "AGAIN";
// the first press (Enter or click) arms it (label -> "CONFIRM", color darkens via
// bg_button_armed); the second press while armed commits, firing AgainPressed and
// returning to the Game screen with a fresh scenario. The armed state persists
// indefinitely (no timeout). Exit operates independently of the armed state.
//
// The state machine is pure and unit-tested here; the button RENDER (an ImGui draw
// into the screen's draw list) lives in the Post-Round render path, which reads
// state() and label_for() from this machine.

namespace poker_trainer::screens {

// The two visible states of the Again button.
enum class AgainState : std::uint8_t {
    Default = 0,  // label "AGAIN", bg_button_default
    Armed = 1,    // label "CONFIRM", bg_button_armed (darkened)
};

// The outcome of a press, returned by press_again so the caller knows whether to
// navigate. Arming does NOT navigate; committing does.
enum class AgainPressOutcome : std::uint8_t {
    Armed = 0,      // Default -> Armed: stay on the Post-Round Screen
    Committed = 1,  // Armed -> commit: fire AgainPressed + return to Game
};

// The live Again button state for the current Post-Round Screen instance.
struct AgainButton {
    AgainState state{AgainState::Default};
};

// Apply one press (Enter while focused, or a mouse click). Default -> Armed
// (returns Armed); Armed -> commit (returns Committed and resets to Default so a
// re-rendered button shows "AGAIN" again, though the screen transitions away on
// commit).
[[nodiscard]] AgainPressOutcome press_again(AgainButton& button) noexcept;

// Reset to the default (unarmed) state. Called on each fresh Post-Round entry so a
// new scenario's recap starts with "AGAIN", never a stale "CONFIRM".
void reset_again(AgainButton& button) noexcept;

// The label for a given state ("AGAIN" / "CONFIRM").
[[nodiscard]] const char* again_label(AgainState state) noexcept;

}  // namespace poker_trainer::screens
