#pragma once

#include "theme/theme_tokens.hpp"

#include <cstdint>
#include <string>

// Zone 10 (Temporal) — the Visual Countdown (Module 6).
//
// The text + color mapping is a pure function so the undertime/overtime boundary
// and the ceil/floor rounding are unit-testable without a live ImGui frame; the
// draw itself is Game-screen chrome the Z08 renderer calls directly.

namespace poker_trainer::temporal {

// The countdown's rendered text + color for a given (elapsed, target).
struct CountdownDisplay {
    std::string text;         // "{n}s"
    theme::ColorToken color;  // text_secondary undertime; state_fail in overtime
};

// Undertime (elapsed < target): ceil of the remaining whole seconds, text_secondary
// (the target's value at spawn ticking down to "1s"). Overtime (elapsed >= target):
// floor of the elapsed-over seconds, state_fail, starting at "0s" and counting up
// ("1s","2s","3s",...). There is no '+' prefix; red is the sole overtime signal.
[[nodiscard]] CountdownDisplay format_countdown(std::uint64_t elapsed_ms,
                                                std::uint64_t target_ms);

// Render the Visual Countdown as Game-screen chrome (NOT a top-level overlay):
// Z08's Game renderer calls this directly, like it calls Z09's math inputs. Draws
// nothing unless countdown_should_render() is true (Show countdown timer on, timer
// not tutorial-disabled, a scenario active).
void render_countdown();

}  // namespace poker_trainer::temporal
