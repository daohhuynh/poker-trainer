#pragma once

// Zone 05 — Tier 1 (Ambient) motion layer.
//
// The always-on, barely-perceptible ambient tier of the Motion Layer (ARCHITECTURE
// Module 3 — Motion Layer): sine "breathing" on the persistent/root buttons and the
// dealer asset, plus an alpha-blended particle drift behind the UI. Driven by the
// single shared animation clock (backbone) and gated by the live Reduce Motion /
// Particle drift settings via the two gate seams wired once in boot.
//
// The motion math is expressed as pure functions of an absolute clock time so it can
// be unit-tested without a clock, an ImGui context, or the settings seam — mirroring
// the pure-function-of-t shape of animations/button_morph. The gated service wrappers
// (ambient_breath_scale, render_ambient_particles) read the clock + gates and are the
// entry points the render sites call; they self-suppress when Reduce Motion is on.
//
// This header stays ImGui-free (only a forward declaration of ImDrawList) so it — and
// the pure bridge library — compile under the ImGui-free native test compiler; the
// particle draw's ImGui body lives behind __EMSCRIPTEN__ in the .cpp.

#include <cstdint>
#include <functional>

struct ImDrawList;

namespace poker_trainer::bridge {

// ----- Tuning constants (Tier 1 Ambient) -----
inline constexpr float kBreathAmplitude = 0.02f;  // ±2% scale
inline constexpr float kBreathHz = 0.5f;          // one full grow/shrink cycle / 2 s

// Sparse by design: a low count of slow motes reads as atmosphere, not a particle
// effect. "If in doubt, fewer and slower."
inline constexpr int kAmbientParticleCount = 24;

// ----- Pure math (no ImGui, no settings — unit-tested) -----

// The breathing scale at absolute clock time `ms`: 1 ± 2% on a 0.5 Hz sine, so one
// full grow→shrink cycle every 2000 ms. Returns exactly 1.0 at ms = 0.
[[nodiscard]] float ambient_breath_scale_at(std::uint64_t ms) noexcept;

// One ambient particle's live state at time `ms` on a `w`×`h` canvas. Position, size
// and faintness are pure functions of the particle index and time (no stored state,
// no per-frame allocation): each particle has fixed, hashed parameters so the field
// drifts organically (varied speeds/directions/sizes/alphas) rather than as a uniform
// scroll or a grid.
struct AmbientParticle {
    float x{0.0f};       // pixels, in [0, w)
    float y{0.0f};       // pixels, in [0, h)
    float radius{0.0f};  // pixels
    float alpha{0.0f};   // 0..1, the particle's own faintness (before the theme alpha)
};
[[nodiscard]] AmbientParticle ambient_particle_at(int index, std::uint64_t ms, float w,
                                                  float h) noexcept;

// A top-left/bottom-right box in pixels. ImGui-free so the breathing transform stays
// in the pure bridge library; call sites convert to/from their own rect type.
struct BreathBox {
    float x0{0.0f};
    float y0{0.0f};
    float x1{0.0f};
    float y1{0.0f};
};

// Grow/shrink `box` about its center by `scale` (1.0 = unchanged). Used to apply the
// breath to a button / dealer draw rect (visual only — callers keep the original rect
// for hit-testing).
[[nodiscard]] BreathBox breathe_box(BreathBox box, float scale) noexcept;

// ----- Gate seams (wired once in boot to the live settings) -----
//
// reduce_motion() true  => Reduce Motion ON  => all of Tier 1 is suppressed.
// particle_drift() true => the Particle drift toggle is ON (default on).
// Both default to "disabled" when unset (native tests never drive the render paths).
void set_ambient_gates(std::function<bool()> reduce_motion,
                       std::function<bool()> particle_drift);

// ----- Gated service (render sites call these) -----

// The current breath scale read off the shared clock, or exactly 1.0 when Reduce
// Motion suppresses Tier 1. Multiply a rect's half-extents by this to breathe it.
[[nodiscard]] float ambient_breath_scale();

// Draw the ambient particle drift onto `dl`, sized to the `w`×`h` canvas. Intended as
// a background pass (call it right after a screen's background, before its UI, so the
// motes sit behind every interactive element). A no-op when Reduce Motion is on OR the
// Particle drift toggle is off (and a no-op in the native build, which has no surface).
void render_ambient_particles(ImDrawList* dl, float w, float h);

}  // namespace poker_trainer::bridge
