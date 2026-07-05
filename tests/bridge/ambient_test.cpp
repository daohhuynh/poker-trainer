// Tier 1 (Ambient) motion math — the pure, clock-driven functions behind the
// breathing scale and the particle drift. The gated service wrappers
// (ambient_breath_scale / render_ambient_particles) read the live settings + an
// ImGui draw list and are verified in-browser, not here (per CLAUDE.md §9).

#include "bridge/ambient.hpp"

#include <cmath>
#include <cstdint>

#include <gtest/gtest.h>

namespace pt = poker_trainer::bridge;

namespace {

// One full breathing cycle at 0.5 Hz is 2000 ms.
constexpr std::uint64_t kCycleMs = 2000;

}  // namespace

TEST(AmbientBreath, NeutralAtCycleBoundaries) {
    // sin is 0 at 0, half, and full cycle -> scale is exactly 1.0 (no scaling).
    EXPECT_NEAR(pt::ambient_breath_scale_at(0), 1.0f, 1e-6f);
    EXPECT_NEAR(pt::ambient_breath_scale_at(kCycleMs / 2), 1.0f, 1e-4f);
    EXPECT_NEAR(pt::ambient_breath_scale_at(kCycleMs), 1.0f, 1e-4f);
}

TEST(AmbientBreath, PeakAndTroughAreExactlyPlusMinusTwoPercent) {
    // Quarter cycle (500 ms) is the +2% peak; three-quarter (1500 ms) the -2% trough.
    EXPECT_NEAR(pt::ambient_breath_scale_at(kCycleMs / 4), 1.0f + pt::kBreathAmplitude, 1e-4f);
    EXPECT_NEAR(pt::ambient_breath_scale_at(3 * kCycleMs / 4), 1.0f - pt::kBreathAmplitude, 1e-4f);
}

TEST(AmbientBreath, StaysWithinTwoPercentEnvelopeAcrossManyCycles) {
    for (std::uint64_t ms = 0; ms <= 10 * kCycleMs; ms += 37) {
        const float s = pt::ambient_breath_scale_at(ms);
        EXPECT_LE(std::fabs(s - 1.0f), pt::kBreathAmplitude + 1e-4f) << "ms=" << ms;
    }
}

TEST(AmbientBreath, PeriodIsTwoSeconds) {
    // Same phase one full cycle apart -> equal scale.
    for (std::uint64_t ms = 0; ms < kCycleMs; ms += 50) {
        EXPECT_NEAR(pt::ambient_breath_scale_at(ms), pt::ambient_breath_scale_at(ms + kCycleMs),
                    1e-4f)
            << "ms=" << ms;
    }
}

TEST(AmbientParticles, StayWithinCanvasBounds) {
    constexpr float w = 1280.0f;
    constexpr float h = 720.0f;
    for (std::uint64_t ms = 0; ms <= 60000; ms += 250) {
        for (int i = 0; i < pt::kAmbientParticleCount; ++i) {
            const pt::AmbientParticle p = pt::ambient_particle_at(i, ms, w, h);
            EXPECT_GE(p.x, 0.0f);
            EXPECT_LT(p.x, w);
            EXPECT_GE(p.y, 0.0f);
            EXPECT_LT(p.y, h);
        }
    }
}

TEST(AmbientParticles, AreSubtleAndSoft) {
    // Small radii and faint alphas — the "atmosphere you feel more than see" bar.
    for (int i = 0; i < pt::kAmbientParticleCount; ++i) {
        const pt::AmbientParticle p = pt::ambient_particle_at(i, 1234, 1000.0f, 1000.0f);
        EXPECT_GT(p.radius, 0.0f);
        EXPECT_LE(p.radius, 4.0f);
        EXPECT_GT(p.alpha, 0.0f);
        EXPECT_LE(p.alpha, 0.1f);
    }
}

TEST(HoverTilt, EasesToHeldPeakWhileActive) {
    // Hovered / focused: progress rises toward 1 and HOLDS there — a held lean, not a
    // wobble that reverts.
    float p = 0.0f;
    for (int i = 0; i < 200; ++i) {  // ~3.2 s at 16 ms/frame
        p = pt::tilt_ease_step(p, 1.0f, 16.0f);
    }
    EXPECT_GT(p, 0.98f);
    EXPECT_LE(p, 1.0f + 1e-4f);
}

TEST(HoverTilt, SettlesBackToRestOnLeave) {
    // On leave the target is 0 and progress eases home (the return keeps animating).
    float p = 1.0f;
    for (int i = 0; i < 200; ++i) {
        p = pt::tilt_ease_step(p, 0.0f, 16.0f);
    }
    EXPECT_LT(p, 0.02f);
    EXPECT_GE(p, -1e-4f);
}

TEST(HoverTilt, EaseIsMonotonicRisingAndBounded) {
    float p = 0.0f;
    for (int i = 0; i < 60; ++i) {
        const float next = pt::tilt_ease_step(p, 1.0f, 16.0f);
        EXPECT_GE(next, p - 1e-6f);  // never reverses while the target is the peak
        EXPECT_LE(next, 1.0f + 1e-4f);
        p = next;
    }
}

TEST(HoverTilt, HeldPeakIsMoreThanAFewDegrees) {
    // "More aggressive": the held lean is a clearly-visible angle (> ~5°).
    EXPECT_GT(pt::kTiltPeakRad, 0.087f);  // 0.087 rad ≈ 5°
}

TEST(AmbientParticles, DriftIsVariedNotUniform) {
    constexpr float w = 800.0f;
    constexpr float h = 600.0f;
    // Neighboring particles occupy different positions (not a stacked/grid field).
    const pt::AmbientParticle a = pt::ambient_particle_at(0, 5000, w, h);
    const pt::AmbientParticle b = pt::ambient_particle_at(1, 5000, w, h);
    EXPECT_TRUE(std::fabs(a.x - b.x) > 1.0f || std::fabs(a.y - b.y) > 1.0f);

    // A particle moves over time (it drifts).
    const pt::AmbientParticle t0 = pt::ambient_particle_at(3, 0, w, h);
    const pt::AmbientParticle t1 = pt::ambient_particle_at(3, 4000, w, h);
    EXPECT_TRUE(std::fabs(t0.x - t1.x) > 0.5f || std::fabs(t0.y - t1.y) > 0.5f);
}
