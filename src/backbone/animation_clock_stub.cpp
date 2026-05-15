#include "backbone/animation_clock.hpp"

namespace poker_trainer::backbone {

std::uint64_t wall_clock_ms() noexcept {
    return 0;
}

std::uint64_t animation_time_ms() noexcept {
    return 0;
}

bool is_animation_paused() noexcept {
    return false;
}

void pause() noexcept {
}

void resume() noexcept {
}

void tick(std::uint64_t /*delta_ms*/) noexcept {
}

void reset_animation_clock_for_testing() noexcept {
}

}  // namespace poker_trainer::backbone
