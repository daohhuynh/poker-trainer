#include "backbone/animation_clock.hpp"

// Real Z05 implementation of the Global Animation Clock.
//
// This is the single shared time source for all frame-level animations
// (ARCHITECTURE Notes — Communication Backbone, Global Animation Clock). Z05's
// main loop calls tick() exactly once per frame with the elapsed milliseconds
// for that frame; every zone with animation reads total_ms_since_app_start()
// for absolute timing and delta_ms_since_last_frame() for frame-rate-independent
// steps. The clock never pauses — modal-driven pause of scenario time is the
// Z10 Delta Timer's concern, not this clock's (so modal slide-in and dealer
// fade-in keep advancing while a modal is open).
//
// Threading model: main-thread only. tick() is called from the browser RAF
// callback (the main thread); reads happen during the same-frame render on the
// same thread. There is no cross-thread access, so the state needs no
// synchronization — matching focus_manager's model. (The stub it replaces lives
// on in animation_clock_stub.cpp solely for the Phase 0 sign-off gate, which
// asserts the stub's no-op behavior.)

namespace poker_trainer::backbone {

namespace {

// Monotonic accumulated time since app start, in whole milliseconds.
std::uint64_t g_total_ms{0};

// The most recent frame's elapsed time, as passed to the last tick().
float g_last_delta_ms{0.0f};

}  // namespace

std::uint64_t total_ms_since_app_start() noexcept {
    return g_total_ms;
}

float delta_ms_since_last_frame() noexcept {
    return g_last_delta_ms;
}

void tick(std::uint64_t delta_ms) noexcept {
    g_total_ms += delta_ms;
    g_last_delta_ms = static_cast<float>(delta_ms);
}

void reset_animation_clock_for_testing() noexcept {
    g_total_ms = 0;
    g_last_delta_ms = 0.0f;
}

}  // namespace poker_trainer::backbone
