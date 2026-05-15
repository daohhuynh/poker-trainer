#pragma once

#include <cstdint>

namespace poker_trainer::backbone {

// Monotonic wall-clock time since app start, in milliseconds. Never
// pauses. Suitable for animations that must run regardless of modal
// state (loading screen, offline indicator pulse, outage banner
// countdown bar).
[[nodiscard]] std::uint64_t wall_clock_ms() noexcept;

// Pausable animation time, in milliseconds. Pauses while any modal
// is open; resumes when all modals close. Suitable for the bulk of
// animations (button morphs, modal slides, chip pushes, dealer
// fade-ins, countdown ticks). When paused, repeated calls return
// the same value until resume() is called.
[[nodiscard]] std::uint64_t animation_time_ms() noexcept;

// Returns true if animation_time_ms() is currently paused.
[[nodiscard]] bool is_animation_paused() noexcept;

// Pause animation_time_ms(). Idempotent: calling pause() while
// already paused is a no-op. Called by Z11 when a modal opens.
// Multiple modal opens stack: the clock stays paused until all
// modals have closed.
void pause() noexcept;

// Resume animation_time_ms(). Idempotent: calling resume() while
// already running is a no-op. Called by Z11 when the last modal
// closes. The internal pause counter decrements; when it hits
// zero, the clock resumes.
void resume() noexcept;

// Advance the clock by the given delta. Called once per frame by
// Z05 with the frame's elapsed time. The implementation maintains
// separate counters for wall-clock and animation time; both
// advance by `delta_ms` unless the animation time is paused.
//
// This function is part of the Z05-internal API surface, exposed
// here so the integration test can drive the clock deterministically.
// No other zone should call this.
void tick(std::uint64_t delta_ms) noexcept;

// Reset all clock state to zero. Used by the integration test to
// restore a clean state between test cases. No other zone should
// call this.
void reset_for_testing() noexcept;

}  // namespace poker_trainer::backbone
