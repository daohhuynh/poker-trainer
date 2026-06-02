#include "backbone/animation_clock.hpp"

// No-op animation-clock stub. NOT dead code and NOT a sign the real clock is
// untested: the real Z05 clock (animation_clock.cpp) is the production source
// and is covered by backbone_test. This stub exists solely so the sealed
// phase0_integration_test — which asserts the clock's no-op Phase 0 behavior —
// keeps its own copy of the symbols (see its CMake target). Do not delete; do
// not wire into the app or backbone library.

namespace poker_trainer::backbone {

std::uint64_t total_ms_since_app_start() noexcept {
    return 0;
}

float delta_ms_since_last_frame() noexcept {
    return 0.0f;
}

void tick(std::uint64_t /*delta_ms*/) noexcept {
}

void reset_animation_clock_for_testing() noexcept {
}

}  // namespace poker_trainer::backbone
