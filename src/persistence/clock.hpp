#pragma once

#include <chrono>

namespace poker_trainer::persistence {

// Monotonic clock seam.
//
// Z04 schedules sync retry backoff and caches the Auth0 health-check result
// against a steady-clock timeline. Production wires SteadyClock (reads the
// real std::chrono::steady_clock); native unit tests inject a controllable
// clock so the backoff schedule and cache TTL are asserted deterministically
// with no real waits. Z04 logic never branches on which clock is installed —
// the clock is a constructor-injected dependency, swapped at the seam.
class Clock {
public:
    virtual ~Clock() = default;

    [[nodiscard]] virtual std::chrono::steady_clock::time_point now()
        const noexcept = 0;
};

// Production clock: delegates to std::chrono::steady_clock. This is the only
// place Z04 reads wall-monotonic time; everything else takes a Clock&.
class SteadyClock final : public Clock {
public:
    [[nodiscard]] std::chrono::steady_clock::time_point now()
        const noexcept override {
        return std::chrono::steady_clock::now();
    }
};

}  // namespace poker_trainer::persistence
