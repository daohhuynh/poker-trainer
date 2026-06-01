// Zone 07 — button morph pure-math + driver unit tests.

#include "animations/button_morph.hpp"

#include "backbone/screen_state.hpp"

#include <cmath>

#include <gtest/gtest.h>

namespace anim = poker_trainer::animations;
namespace bb = poker_trainer::backbone;

namespace {

void expect_rect_near(const anim::Rect& got, const anim::Rect& want, float eps = 1e-3f) {
    EXPECT_NEAR(got.x, want.x, eps);
    EXPECT_NEAR(got.y, want.y, eps);
    EXPECT_NEAR(got.w, want.w, eps);
    EXPECT_NEAR(got.h, want.h, eps);
}

}  // namespace

// ----- Easing -----

TEST(EaseInOut, Endpoints) {
    EXPECT_FLOAT_EQ(anim::ease_in_out(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(anim::ease_in_out(1.0f), 1.0f);
    EXPECT_NEAR(anim::ease_in_out(0.5f), 0.5f, 1e-5f);
}

TEST(EaseInOut, MonotonicAndClamped) {
    EXPECT_FLOAT_EQ(anim::ease_in_out(-1.0f), 0.0f);
    EXPECT_FLOAT_EQ(anim::ease_in_out(2.0f), 1.0f);
    float prev = anim::ease_in_out(0.0f);
    for (int i = 1; i <= 20; ++i) {
        const float t = static_cast<float>(i) / 20.0f;
        const float cur = anim::ease_in_out(t);
        EXPECT_GE(cur, prev) << "ease must be monotonic non-decreasing at t=" << t;
        prev = cur;
    }
}

TEST(EaseInOut, NotLinear) {
    // A linear ramp would give 0.25 at t=0.25; the smooth curve must differ.
    EXPECT_GT(std::abs(anim::ease_in_out(0.25f) - 0.25f), 1e-3f);
}

// ----- Crossfade alpha -----

TEST(CrossfadeAlpha, EndpointsMonotonicClamped) {
    EXPECT_FLOAT_EQ(anim::crossfade_alpha(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(anim::crossfade_alpha(1.0f), 1.0f);
    EXPECT_FLOAT_EQ(anim::crossfade_alpha(-0.5f), 0.0f);
    EXPECT_FLOAT_EQ(anim::crossfade_alpha(3.0f), 1.0f);
    float prev = anim::crossfade_alpha(0.0f);
    for (int i = 1; i <= 20; ++i) {
        const float cur = anim::crossfade_alpha(static_cast<float>(i) / 20.0f);
        EXPECT_GE(cur, prev);
        prev = cur;
    }
}

// ----- Per-button stagger -----

TEST(ButtonStagger, OffsetsAreOrderedAndCorrect) {
    using anim::MorphButton;
    EXPECT_FLOAT_EQ(anim::button_start_fraction(MorphButton::Play), 0.0f);
    EXPECT_LT(anim::button_start_fraction(MorphButton::Play),
              anim::button_start_fraction(MorphButton::Settings));
    EXPECT_LT(anim::button_start_fraction(MorphButton::Settings),
              anim::button_start_fraction(MorphButton::Shop));
    EXPECT_LT(anim::button_start_fraction(MorphButton::Shop),
              anim::button_start_fraction(MorphButton::Help));
    // 50ms stagger over a 450ms total.
    EXPECT_NEAR(anim::button_start_fraction(MorphButton::Settings), 50.0f / 450.0f, 1e-5f);
    EXPECT_NEAR(anim::button_start_fraction(MorphButton::Help), 150.0f / 450.0f, 1e-5f);
}

TEST(ButtonLocalT, ZeroAtStartOneAtEnd) {
    using anim::MorphButton;
    EXPECT_FLOAT_EQ(anim::button_local_t(0.0f, MorphButton::Play), 0.0f);
    EXPECT_FLOAT_EQ(anim::button_local_t(0.0f, MorphButton::Help), 0.0f);
    // Each button reaches local 1 by global 1.
    EXPECT_FLOAT_EQ(anim::button_local_t(1.0f, MorphButton::Play), 1.0f);
    EXPECT_FLOAT_EQ(anim::button_local_t(1.0f, MorphButton::Help), 1.0f);
    // A button has not started at its own stagger offset.
    EXPECT_FLOAT_EQ(
        anim::button_local_t(anim::button_start_fraction(MorphButton::Help), MorphButton::Help),
        0.0f);
}

// ----- Lerp -----

TEST(Lerp, Endpoints) {
    EXPECT_FLOAT_EQ(anim::lerp(2.0f, 8.0f, 0.0f), 2.0f);
    EXPECT_FLOAT_EQ(anim::lerp(2.0f, 8.0f, 1.0f), 8.0f);
    EXPECT_FLOAT_EQ(anim::lerp(2.0f, 8.0f, 0.5f), 5.0f);
}

TEST(MorphButtonRect, LandsAtStartAndTargetEndpoints) {
    const anim::Canvas c{1920.0f, 1080.0f};
    // At global 0, Play sits exactly at its Root grid rect.
    expect_rect_near(anim::morph_button_rect(anim::MorphButton::Play, 0.0f, c),
                     anim::root_grid_button_rect(anim::MorphButton::Play, c));
    // At global 1, the last button (Help) lands at its Mode target rect.
    expect_rect_near(anim::morph_button_rect(anim::MorphButton::Help, 1.0f, c),
                     anim::mode_button_target_rect(anim::MorphButton::Help, c));
    // Play -> STANDARD target is the standard button rect.
    expect_rect_near(anim::mode_button_target_rect(anim::MorphButton::Play, c),
                     anim::standard_button_rect(c));
}

// ----- MorphController -----

TEST(MorphController, ProgressAndCompletion) {
    anim::MorphController m;
    EXPECT_FALSE(m.active());
    EXPECT_FLOAT_EQ(m.progress(123), 0.0f);

    m.start(1000);
    EXPECT_TRUE(m.active());
    EXPECT_FLOAT_EQ(m.progress(1000), 0.0f);
    EXPECT_NEAR(m.progress(1000 + anim::kTotalMorphMs / 2), 0.5f, 1e-3f);
    EXPECT_FLOAT_EQ(m.progress(1000 + anim::kTotalMorphMs), 1.0f);
    EXPECT_FALSE(m.is_complete(1000 + anim::kTotalMorphMs - 1));
    EXPECT_TRUE(m.is_complete(1000 + anim::kTotalMorphMs));
}

TEST(MorphController, CrossfadeReachesOneAtCrossfadeDuration) {
    anim::MorphController m;
    m.start(500);
    EXPECT_FLOAT_EQ(m.crossfade(500), 0.0f);
    EXPECT_FLOAT_EQ(m.crossfade(500 + anim::kCrossfadeMs), 1.0f);
}

TEST(MorphController, DebouncesRestartWhileActive) {
    anim::MorphController m;
    m.start(1000);
    m.start(1100);  // ignored — does not reset the start timestamp
    EXPECT_NEAR(m.progress(1100), 100.0f / static_cast<float>(anim::kTotalMorphMs), 1e-3f);
}

TEST(AdvanceMorph, SetsScreenStateOnCompletion) {
    bb::reset_screen_state_for_testing();
    anim::MorphController m;

    EXPECT_EQ(anim::advance_morph(m, 0), anim::MorphTick::Idle);

    m.start(0);
    EXPECT_EQ(anim::advance_morph(m, 100), anim::MorphTick::InProgress);
    EXPECT_EQ(bb::read_screen_state().current, bb::ScreenId::Root);

    EXPECT_EQ(anim::advance_morph(m, anim::kTotalMorphMs), anim::MorphTick::JustCompleted);
    EXPECT_EQ(bb::read_screen_state().current, bb::ScreenId::ModeSelection);
    EXPECT_FALSE(m.active());

    // After completion the controller is idle again.
    EXPECT_EQ(anim::advance_morph(m, anim::kTotalMorphMs + 1), anim::MorphTick::Idle);

    bb::reset_screen_state_for_testing();
}
