#include "bridge/ambient.hpp"

#include "backbone/animation_clock.hpp"

#include <cmath>
#include <cstdint>
#include <utility>

// The particle draw is the only ImGui/theme-touching part; it is compiled only in the
// wasm build (the native bridge library stays ImGui-free). Native tests exercise the
// pure math directly and never draw.
#ifdef __EMSCRIPTEN__
#include "theme/theme_tokens.hpp"

#include <imgui.h>
#endif

namespace poker_trainer::bridge {

namespace {

constexpr float kPi = 3.14159265358979323846f;

// Tier 1 gate seams. File-scope std::function seams wired once in boot, mirroring
// modal_base's g_tutorial_start_handler pattern (a Z05-owned seam, not shared
// cross-zone state). Unset in native unit tests, where the gated render/scale paths
// are never exercised — the pure math is tested directly.
std::function<bool()> g_reduce_motion;   // true => Reduce Motion ON  => Tier 1 off
std::function<bool()> g_particle_drift;  // true => Particle drift toggle ON

[[nodiscard]] bool reduce_motion_on() noexcept {
    return static_cast<bool>(g_reduce_motion) && g_reduce_motion();
}
// Only read from the particle draw (wasm-only), so it is unused in the native build.
[[nodiscard, maybe_unused]] bool particle_drift_on() noexcept {
    return static_cast<bool>(g_particle_drift) && g_particle_drift();
}

// Cheap deterministic hash -> [0, 1). `salt` selects an independent parameter stream
// per particle (base x/y, speed, sway, phase, depth) so no two particles share a
// trajectory and the field never reads as a grid.
[[nodiscard]] float hash01(int i, int salt) noexcept {
    std::uint32_t h = static_cast<std::uint32_t>(i) * 2654435761u ^
                      static_cast<std::uint32_t>(salt) * 2246822519u;
    h ^= h >> 15;
    h *= 2654435761u;
    h ^= h >> 13;
    return static_cast<float>(h & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

[[nodiscard]] float wrap01(float v) noexcept { return v - std::floor(v); }

}  // namespace

float ambient_breath_scale_at(std::uint64_t ms) noexcept {
    const float seconds = static_cast<float>(ms) / 1000.0f;
    return 1.0f + kBreathAmplitude * std::sin(2.0f * kPi * kBreathHz * seconds);
}

AmbientParticle ambient_particle_at(int index, std::uint64_t ms, float w, float h) noexcept {
    const float t = static_cast<float>(ms) / 1000.0f;  // seconds

    // Fixed per-particle parameters (all in normalized units), hashed off the index so
    // the drift is organic: slightly varied rise speeds, a small slow lateral sway, and
    // a depth term that co-varies size and alpha for a faint sense of distance.
    const float base_x = hash01(index, 1);
    const float base_y = hash01(index, 2);
    const float rise = 0.006f + 0.010f * hash01(index, 3);      // slow upward drift, frac/s
    const float sway_amp = 0.008f + 0.018f * hash01(index, 4);  // small lateral sway, frac
    const float sway_hz = 0.02f + 0.05f * hash01(index, 5);     // very slow sway
    const float phase = hash01(index, 6) * 2.0f * kPi;
    const float depth = hash01(index, 7);                       // 0..1: near←→far

    // Rise (wrapping so motes recycle) with a gentle sway; deeper motes are smaller and
    // fainter. Kept subtle: radius ~1.4–3.4 px, alpha ~0.035–0.085 before the theme alpha.
    const float ny = wrap01(base_y - rise * t);
    const float nx = wrap01(base_x + sway_amp * std::sin(2.0f * kPi * sway_hz * t + phase));

    AmbientParticle p{};
    p.x = nx * w;
    p.y = ny * h;
    p.radius = 1.4f + 2.0f * depth;
    p.alpha = 0.035f + 0.050f * depth;
    return p;
}

BreathBox breathe_box(BreathBox box, float scale) noexcept {
    const float cx = (box.x0 + box.x1) * 0.5f;
    const float cy = (box.y0 + box.y1) * 0.5f;
    const float half_w = (box.x1 - box.x0) * 0.5f * scale;
    const float half_h = (box.y1 - box.y0) * 0.5f * scale;
    return BreathBox{cx - half_w, cy - half_h, cx + half_w, cy + half_h};
}

void set_ambient_gates(std::function<bool()> reduce_motion,
                       std::function<bool()> particle_drift) {
    g_reduce_motion = std::move(reduce_motion);
    g_particle_drift = std::move(particle_drift);
}

float ambient_breath_scale() {
    if (reduce_motion_on()) {
        return 1.0f;  // Reduce Motion: static
    }
    return ambient_breath_scale_at(backbone::total_ms_since_app_start());
}

void render_ambient_particles(ImDrawList* dl, float w, float h) {
#ifdef __EMSCRIPTEN__
    if (dl == nullptr || w <= 0.0f || h <= 0.0f) {
        return;
    }
    // Gate: off under Reduce Motion, and independently off when the Particle drift
    // toggle is cleared.
    if (reduce_motion_on() || !particle_drift_on()) {
        return;
    }

    const std::uint64_t ms = backbone::total_ms_since_app_start();

    // Palette-consistent tint: the theme's muted warm secondary accent (bronze/amber in
    // the leather themes) — never pure white. Per-particle alpha keeps every mote faint.
    const ImVec4 tint = theme::get_color(theme::ColorToken::AccentSecondary);
    for (int i = 0; i < kAmbientParticleCount; ++i) {
        const AmbientParticle p = ambient_particle_at(i, ms, w, h);
        // ImDrawList has no blur, so softness is faked: a wide, extra-faint halo under a
        // slightly stronger small core reads as a soft mote rather than a hard dot.
        const ImU32 halo = ImGui::ColorConvertFloat4ToU32(
            ImVec4{tint.x, tint.y, tint.z, tint.w * p.alpha * 0.45f});
        const ImU32 core = ImGui::ColorConvertFloat4ToU32(
            ImVec4{tint.x, tint.y, tint.z, tint.w * p.alpha});
        dl->AddCircleFilled(ImVec2{p.x, p.y}, p.radius * 1.9f, halo, 12);
        dl->AddCircleFilled(ImVec2{p.x, p.y}, p.radius, core, 10);
    }
#else
    // Native build: no draw surface (rendering is verified in-browser, not in tests).
    (void)dl;
    (void)w;
    (void)h;
#endif
}

}  // namespace poker_trainer::bridge
