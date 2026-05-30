#pragma once

#include <cstdint>

namespace poker_trainer::backbone {

// Monotonic time since app start, in milliseconds. Never pauses — this
// is the single shared time source for all frame-level animations. Use
// it for absolute timing (e.g., "the dealer arrived 600 ms ago"). Owned
// and advanced by Z05 (once per frame, via tick).
[[nodiscard]] std::uint64_t total_ms_since_app_start() noexcept;

// Elapsed time of the most recent frame, in milliseconds, for
// frame-rate-independent animation steps. Returned as a float so
// sub-millisecond frame deltas are preserved.
//
// Note: this clock never pauses. Pausing scenario time while a modal is
// open is the Zone 10 Delta Timer's responsibility (Z10 subscribes to
// modal_opened/modal_closed); the animation clock keeps running so modal
// slide-in and dealer fade-in animations advance while a modal is open.
[[nodiscard]] float delta_ms_since_last_frame() noexcept;

// Advance the clock by the frame's elapsed time. Called once per frame
// by Z05. Records `delta_ms` as the most recent frame delta (returned by
// delta_ms_since_last_frame) and accumulates it into
// total_ms_since_app_start.
//
// This function is part of the Z05-internal API surface, exposed here so
// the integration test can drive the clock deterministically. No other
// zone should call this.
void tick(std::uint64_t delta_ms) noexcept;

// Reset all clock state to zero. Used by the integration test to
// restore a clean state between test cases. No other zone should
// call this.
void reset_animation_clock_for_testing() noexcept;

}  // namespace poker_trainer::backbone
