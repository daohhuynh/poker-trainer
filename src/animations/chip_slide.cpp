#include "animations/chip_slide.hpp"

#include "render/render_constants.hpp"

#include <algorithm>
#include <cstdint>

namespace poker_trainer::animations {

float ease_out_cubic(float t) noexcept {
    const float c = std::clamp(t, 0.0f, 1.0f);
    const float inv = 1.0f - c;
    return 1.0f - inv * inv * inv;
}

float chip_push_progress(std::uint64_t spawn_ms, std::uint64_t now_ms) noexcept {
    if (now_ms <= spawn_ms) {
        return 0.0f;
    }
    const std::uint64_t elapsed = now_ms - spawn_ms;
    if (elapsed >= render::kChipPushDurationMs) {
        return 1.0f;
    }
    const float t = static_cast<float>(elapsed) / static_cast<float>(render::kChipPushDurationMs);
    return ease_out_cubic(t);
}

}  // namespace poker_trainer::animations
