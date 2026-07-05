#include "bridge/ambient.hpp"

#include "backbone/animation_clock.hpp"

#include <array>
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
std::function<bool()> g_hover_tilt;      // true => Hover-tilt toggle ON

[[nodiscard]] bool reduce_motion_on() noexcept {
    return static_cast<bool>(g_reduce_motion) && g_reduce_motion();
}
// Only read from the particle draw (wasm-only), so it is unused in the native build.
[[nodiscard, maybe_unused]] bool particle_drift_on() noexcept {
    return static_cast<bool>(g_particle_drift) && g_particle_drift();
}
[[nodiscard]] bool hover_tilt_on() noexcept {
    return !reduce_motion_on() && static_cast<bool>(g_hover_tilt) && g_hover_tilt();
}

// Per-element tilt progress (0 = rest, 1 = held peak), eased toward the element's active
// state each frame and eased back when it goes inactive — so a button keeps animating its
// return AFTER the cursor / focus leaves, and many can be mid-transition at once. A small
// fixed pool keyed by element id (no per-frame allocation); transient Z05-owned render
// state, the same file-scope-seam shape as the gates above (CLAUDE.md §10: a Z05 render
// service, not cross-zone shared state).
struct TiltSlot {
    backbone::FocusableId id{};
    float progress{0.0f};
    float sign{1.0f};        // current lean direction (+1 / -1); re-rolled on each entry
    std::uint64_t last_ms{0};
    bool used{false};
    bool was_active{false};  // previous frame's active state, for the rest->active edge
};
constexpr std::size_t kTiltSlotCount = 24;
std::array<TiltSlot, kTiltSlotCount> g_tilt_slots{};

[[nodiscard]] TiltSlot& tilt_slot_for(backbone::FocusableId id, std::uint64_t now) noexcept {
    std::size_t free_slot = kTiltSlotCount;  // sentinel: no free slot seen yet
    for (std::size_t i = 0; i < kTiltSlotCount; ++i) {
        if (g_tilt_slots[i].used && g_tilt_slots[i].id == id) {
            return g_tilt_slots[i];
        }
        if (free_slot == kTiltSlotCount && !g_tilt_slots[i].used) {
            free_slot = i;
        }
    }
    // First sighting of this element: claim a free slot (the pool comfortably exceeds the
    // handful of eligible buttons, so a free slot is effectively always available).
    TiltSlot& s = g_tilt_slots[free_slot < kTiltSlotCount ? free_slot : 0];
    s = TiltSlot{};
    s.id = id;
    s.used = true;
    s.last_ms = now;
    return s;
}

// Cosmetic-only PRNG (xorshift32) used to pick a random lean direction on each entry, so
// hovering / tabbing the same button leans left or right unpredictably — it reads more
// organic than a fixed per-button side. NOT scenario state: never seeded from a Scenario
// ID, never persisted, and it has no bearing on scenario reproducibility.
std::uint32_t g_tilt_rng = 0x9e3779b9u;
[[nodiscard]] float random_lean_sign() noexcept {
    g_tilt_rng ^= g_tilt_rng << 13;
    g_tilt_rng ^= g_tilt_rng >> 17;
    g_tilt_rng ^= g_tilt_rng << 5;
    return (g_tilt_rng & 1u) != 0u ? 1.0f : -1.0f;
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

float tilt_ease_step(float progress, float target, float dt_ms) noexcept {
    // Exponential approach: quick rise to the peak, slightly slower settle back to rest.
    const float tau = (target > progress) ? kTiltRiseTauMs : kTiltFallTauMs;
    const float alpha = 1.0f - std::exp(-dt_ms / tau);
    return progress + (target - progress) * alpha;
}

void set_hover_tilt_gate(std::function<bool()> hover_tilt) {
    g_hover_tilt = std::move(hover_tilt);
}

float hover_tilt_angle(backbone::FocusableId id, bool hovered, bool focused) {
    const std::uint64_t now = backbone::total_ms_since_app_start();
    TiltSlot& slot = tilt_slot_for(id, now);
    float dt_ms = static_cast<float>(now - slot.last_ms);
    slot.last_ms = now;
    if (dt_ms > kTiltMaxStepMs) {
        dt_ms = kTiltMaxStepMs;  // a backgrounded/paused tab must not teleport the tilt
    }
    // Target the held peak while hovered or focused, but only while Tier 1 tilt is
    // enabled; a disabled gate (Reduce Motion / toggle off) drives the target to rest so
    // any in-flight tilt eases home rather than snapping.
    const bool active = hover_tilt_on() && (hovered || focused);
    // On a genuine entry (rest -> active) roll a fresh random lean direction; if the
    // element re-activates while still mid-lean, keep the current direction so the tilt
    // never snaps through center.
    if (active && !slot.was_active && slot.progress < 0.05f) {
        slot.sign = random_lean_sign();
    }
    slot.was_active = active;
    slot.progress = tilt_ease_step(slot.progress, active ? 1.0f : 0.0f, dt_ms);
    return slot.progress * slot.sign * kTiltPeakRad;
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
