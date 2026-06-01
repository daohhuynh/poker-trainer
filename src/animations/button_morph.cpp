#include "animations/button_morph.hpp"

#include "backbone/screen_state.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace poker_trainer::animations {

namespace {

[[nodiscard]] float clamp01(float t) noexcept { return std::clamp(t, 0.0f, 1.0f); }

[[nodiscard]] std::uint64_t elapsed_since(std::uint64_t start, std::uint64_t now) noexcept {
    return now >= start ? now - start : 0;
}

}  // namespace

// ----- Pure scalar math -----

float lerp(float a, float b, float t) noexcept { return a + (b - a) * t; }

float ease_in_out(float t) noexcept {
    t = clamp01(t);
    if (t < 0.5f) {
        return 4.0f * t * t * t;
    }
    const float f = -2.0f * t + 2.0f;
    return 1.0f - (f * f * f) * 0.5f;
}

float crossfade_alpha(float t) noexcept {
    t = clamp01(t);
    const float inv = 1.0f - t;
    return 1.0f - inv * inv;  // ease-out
}

float button_start_fraction(MorphButton button) noexcept {
    const auto i = static_cast<std::uint64_t>(static_cast<std::uint8_t>(button));
    return static_cast<float>(i * kStaggerMs) / static_cast<float>(kTotalMorphMs);
}

float button_local_t(float global_t, MorphButton button) noexcept {
    const float start = button_start_fraction(button);
    const float motion =
        static_cast<float>(kButtonMotionMs) / static_cast<float>(kTotalMorphMs);
    return clamp01((clamp01(global_t) - start) / motion);
}

float button_eased_progress(float global_t, MorphButton button) noexcept {
    return ease_in_out(button_local_t(global_t, button));
}

// ----- Pure relative-layout geometry -----

Rect lerp_rect(const Rect& a, const Rect& b, float t) noexcept {
    return Rect{lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.w, b.w, t), lerp(a.h, b.h, t)};
}

Rect logo_rect(Canvas canvas) noexcept {
    return Rect{0.03f * canvas.width, 0.03f * canvas.height,
                0.12f * canvas.width, 0.06f * canvas.height};
}

Rect home_icon_rect(Canvas canvas) noexcept {
    const float s = 0.05f * canvas.height;          // square icon, height-relative
    const float margin_x = 0.03f * canvas.width;
    const float margin_y = 0.03f * canvas.height;
    return Rect{canvas.width - margin_x - s, margin_y, s, s};
}

Rect standard_button_rect(Canvas canvas) noexcept {
    return Rect{0.04f * canvas.width, 0.11f * canvas.height,
                0.18f * canvas.width, 0.08f * canvas.height};
}

Rect root_grid_button_rect(MorphButton button, Canvas canvas) noexcept {
    const float grid_w = 0.44f * canvas.width;
    const float grid_h = 0.40f * canvas.height;
    const float grid_left = 0.5f * canvas.width - grid_w * 0.5f;
    const float grid_top = 0.5f * canvas.height - grid_h * 0.5f;
    const float cell_w = grid_w * 0.5f;
    const float cell_h = grid_h * 0.5f;
    const float pad_x = cell_w * 0.10f;
    const float pad_y = cell_h * 0.10f;

    // Play=TL, Settings=TR, Shop=BL, Help=BR of the grid.
    float col = 0.0f;
    float row = 0.0f;
    switch (button) {
        case MorphButton::Play:     col = 0.0f; row = 0.0f; break;
        case MorphButton::Settings: col = 1.0f; row = 0.0f; break;
        case MorphButton::Shop:     col = 0.0f; row = 1.0f; break;
        case MorphButton::Help:     col = 1.0f; row = 1.0f; break;
    }
    return Rect{grid_left + col * cell_w + pad_x, grid_top + row * cell_h + pad_y,
                cell_w - 2.0f * pad_x, cell_h - 2.0f * pad_y};
}

Rect mode_button_target_rect(MorphButton button, Canvas canvas) noexcept {
    if (button == MorphButton::Play) {
        return standard_button_rect(canvas);  // PLAY -> STANDARD, top-left region
    }
    // The three others shrink into icon slots left of the stationary Home icon.
    // Final cluster, left->right: Shop, Help, Settings, Home. So counting slots
    // from the right edge: Home=0, Settings=1, Help=2, Shop=3.
    const Rect home = home_icon_rect(canvas);
    const float s = home.w;
    const float gap = 0.012f * canvas.width;
    float slots_from_right = 0.0f;
    switch (button) {
        case MorphButton::Settings: slots_from_right = 1.0f; break;
        case MorphButton::Help:     slots_from_right = 2.0f; break;
        case MorphButton::Shop:     slots_from_right = 3.0f; break;
        case MorphButton::Play:     slots_from_right = 0.0f; break;  // unreachable
    }
    return Rect{home.x - slots_from_right * (s + gap), home.y, s, s};
}

Rect morph_button_rect(MorphButton button, float global_t, Canvas canvas) noexcept {
    return lerp_rect(root_grid_button_rect(button, canvas),
                     mode_button_target_rect(button, canvas),
                     button_eased_progress(global_t, button));
}

Rect mode_middle_button_rect(std::uint32_t index, Canvas canvas) noexcept {
    const float button_w = 0.16f * canvas.width;
    const float button_h = 0.09f * canvas.height;
    const float gap = 0.04f * canvas.width;
    const float total_w = 3.0f * button_w + 2.0f * gap;
    const float start_x = 0.5f * canvas.width - total_w * 0.5f;
    const float y = 0.5f * canvas.height - button_h * 0.5f;
    return Rect{start_x + static_cast<float>(index) * (button_w + gap), y, button_w, button_h};
}

// ----- Morph driver -----

void MorphController::start(std::uint64_t now_ms) noexcept {
    if (active()) {
        return;  // debounce: ignore a second start while a morph is in flight
    }
    start_ms_ = now_ms;
}

float MorphController::progress(std::uint64_t now_ms) const noexcept {
    if (!start_ms_.has_value()) {
        return 0.0f;
    }
    const std::uint64_t e = elapsed_since(*start_ms_, now_ms);
    return clamp01(static_cast<float>(e) / static_cast<float>(kTotalMorphMs));
}

float MorphController::crossfade(std::uint64_t now_ms) const noexcept {
    if (!start_ms_.has_value()) {
        return 0.0f;
    }
    const std::uint64_t e = elapsed_since(*start_ms_, now_ms);
    return crossfade_alpha(static_cast<float>(e) / static_cast<float>(kCrossfadeMs));
}

bool MorphController::is_complete(std::uint64_t now_ms) const noexcept {
    if (!start_ms_.has_value()) {
        return false;
    }
    return elapsed_since(*start_ms_, now_ms) >= kTotalMorphMs;
}

MorphTick advance_morph(MorphController& morph, std::uint64_t now_ms) noexcept {
    if (!morph.active()) {
        return MorphTick::Idle;
    }
    if (morph.is_complete(now_ms)) {
        morph.reset();
        backbone::set_screen(backbone::ScreenId::ModeSelection, std::nullopt);
        return MorphTick::JustCompleted;
    }
    return MorphTick::InProgress;
}

}  // namespace poker_trainer::animations

// MorphButton index ordering must match the staggered launch order assumed by
// button_start_fraction (Play=0, Settings=1, Shop=2, Help=3).
static_assert(static_cast<unsigned>(poker_trainer::animations::MorphButton::Play) == 0u);
static_assert(static_cast<unsigned>(poker_trainer::animations::MorphButton::Help) == 3u);
