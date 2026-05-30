#include "backbone/animation_clock.hpp"

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
