#pragma once

#include <cstdint>

// Zone 08 — Caller chip-push animation timing (ARCHITECTURE: the opponent's bet
// chips animate from the seat to their forward-pushed position over ~300 ms with
// an ease-out curve, once at scenario spawn, then static). Pure scalar math, a
// function of the spawn timestamp and the current animation-clock reading only —
// no ImGui, no clock read inside, so it is deterministic and unit-testable. The
// render TU converts the returned progress to an interpolated chip position.
//
// SFX is NOT driven here: Z03's spawn choreography fires the Chip Push / Side Pot
// Split cues at T=400 ms (audio/choreography.cpp). Z08 owns only the visual.

namespace poker_trainer::animations {

// Ease-out cubic, monotonic with ease_out(0)=0, ease_out(1)=1.
[[nodiscard]] float ease_out_cubic(float t) noexcept;

// The eased push progress in [0, 1] at `now_ms`, given the scenario spawned at
// `spawn_ms`. 0 at/before spawn, ramping to 1 over the ~300 ms duration, clamped
// at 1 thereafter (the chips then hold their forward position).
[[nodiscard]] float chip_push_progress(std::uint64_t spawn_ms, std::uint64_t now_ms) noexcept;

}  // namespace poker_trainer::animations
