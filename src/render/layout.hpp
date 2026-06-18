#pragma once

#include "engine/scenario.hpp"

#include <array>
#include <cmath>
#include <cstdint>

// Zone 08 — Game-screen layout geometry. Pure functions of the canvas (browser
// viewport) dimensions only; no absolute pixels, no ImGui, no theme. The render
// TUs convert these to draw-list calls. Kept header-only and ImGui-free so the
// math stays inspectable and could be exercised without a live frame.
//
// The screen is composed FIRST-PERSON, from the hero's seat (the small-blind spot
// looking across the table). The felt is drawn in FORESHORTENED PERSPECTIVE: a
// wide near rim closest to the camera at the BOTTOM that narrows as it recedes to
// a small far rim at the TOP, so the table reads as a surface seen from a chair,
// not a map seen from above. The hero's hole cards sit dead-CENTER at the bottom,
// nearest the camera and largest; the five opponents are chip stacks (no avatars)
// along the hero's LEFT, across the far rim, and up to the upper-RIGHT — each
// drawn SMALLER the farther it recedes; the Butler dealer is seated on the RIGHT
// (front-right of the hero); the community + pot sit mid-table; Z09's math inputs
// stay on the LEFT. The perspective is a STATIC 2D composition (horizontal width
// and seat size scale with a per-point depth term) — NOT a camera or projection
// matrix — so it stays within the no-3D rule and swaps cleanly for authored art.
//
// table_center is the felt's visual center; table_ry is the vertical half-extent;
// table_rx is the NEAR (bottom) horizontal half-width and table_far_rx the FAR
// (top) one (table_far_rx < table_rx is what produces the foreshortening).

namespace poker_trainer::render {

struct Pt {
    float x{0.0f};
    float y{0.0f};
};

// A seat's on-screen position plus its perspective size scale (1.0 nearest the
// camera, down toward kFarSeatScale at the far rim). Render TUs scale the seat's
// chips / labels by it so far seats read as receding.
struct SeatSpot {
    Pt pos{};
    float scale{1.0f};
};

// Smallest perspective scale, applied at the far (top) rim; the near rim is 1.0.
inline constexpr float kFarSeatScale = 0.5f;
inline constexpr float kLayoutPi = 3.14159265358979323846f;

// The felt is a backwards-D: a STRAIGHT chord on the RIGHT (the dealer's edge) and
// the curve everywhere else, where the players sit. The procedural felt samples the
// rim from kFeltFlatHalfAngleDeg around to (360 - kFeltFlatHalfAngleDeg); the wedge
// it skips, centered on the right (angle 0), is closed as the flat dealer chord. No
// seat is ever placed inside that wedge — opponents sit only on the curve.
inline constexpr float kFeltFlatHalfAngleDeg = 42.0f;

struct GameLayout {
    float w{0.0f};
    float h{0.0f};

    // Foreshortened-perspective felt about table_center: table_ry is the vertical
    // half-extent, table_rx the NEAR (bottom) horizontal half-width, table_far_rx
    // the FAR (top) one. table_far_rx < table_rx makes the table recede.
    Pt table_center{};
    float table_rx{0.0f};
    float table_far_rx{0.0f};
    float table_ry{0.0f};

    // Pot cluster anchor (mid-table; community sits above it, the hero's cards below).
    Pt pot{};

    // Top-left reference column for the legend / pot / blinds stack.
    Pt info_anchor{};

    // Dealer box (seated on the right), top-left + size.
    Pt dealer_tl{};
    float dealer_w{0.0f};
    float dealer_h{0.0f};

    // Top-right persistent-cluster anchor.
    Pt cluster_anchor{};
};

[[nodiscard]] inline GameLayout compute_layout(float w, float h) noexcept {
    GameLayout L{};
    L.w = w;
    L.h = h;
    // The D-table sits in the LOWER-CENTER of the frame — far (top) rim around 36%,
    // near (bottom) rim around 80% — so the room/background fills above it, NOT
    // spanning the whole height. The near rim is wide and the far rim ~half as wide
    // (foreshortening); the right side is flattened into the dealer's chord.
    L.table_center = Pt{w * 0.50f, h * 0.58f};
    L.table_rx = w * 0.40f;       // near (bottom) half-width
    L.table_far_rx = w * 0.23f;   // far (top) half-width
    L.table_ry = h * 0.22f;
    // Pot near the felt's visual middle (community just above, hero's cards below).
    L.pot = Pt{L.table_center.x, L.table_center.y - L.table_ry * 0.05f};
    L.info_anchor = Pt{w * 0.02f, h * 0.03f};
    // Dealer seated at the felt's flat right edge, front-right of the hero.
    L.dealer_w = w * 0.18f;
    L.dealer_h = h * 0.44f;
    L.dealer_tl = Pt{w * 0.72f, h * 0.34f};
    L.cluster_anchor = Pt{w * 0.86f, h * 0.04f};
    return L;
}

// Map a position to its seat slot relative to the hero (slot 0 = hero, bottom
// center). Slots advance around the table from the hero's seat.
[[nodiscard]] inline int seat_slot(engine::Position seat, engine::Position hero) noexcept {
    return (static_cast<int>(seat) - static_cast<int>(hero) + static_cast<int>(engine::kPositionCount)) %
           static_cast<int>(engine::kPositionCount);
}

// A point on the felt's perspective rim at `angle_deg` (90 = the near/bottom point
// closest to the camera, 270 = the far/top point), with its depth scale. The
// horizontal half-width and the size scale both lerp from the far rim (top) to the
// near rim (bottom) by depth, which is what foreshortens the oval.
[[nodiscard]] inline SeatSpot rim_spot(const GameLayout& L, float angle_deg) noexcept {
    const float a = angle_deg * (kLayoutPi / 180.0f);
    const float ex = std::cos(a);
    const float ey = std::sin(a);
    const float depth = (ey + 1.0f) * 0.5f;  // 1 at the near/bottom rim, 0 at the far/top
    const float half_w = L.table_far_rx + depth * (L.table_rx - L.table_far_rx);
    const float scale = kFarSeatScale + depth * (1.0f - kFarSeatScale);
    return SeatSpot{Pt{L.table_center.x + ex * half_w, L.table_center.y + ey * L.table_ry}, scale};
}

// First-person seat spot. Slot 0 is the hero, dead-CENTER at the bottom nearest
// the camera (largest; the hero's hole cards + label render here). Slots 1..5 are
// the five opponents as chip stacks on the D's CURVE — from the hero's lower-LEFT,
// up the left, across the far (top) rim, to the upper-RIGHT — each scaled down as
// it recedes. None sit in the right wedge (the dealer's flat chord); slot 4 is the
// far top-center seat (active_opponent_slot), so a Caller's pushed chips read
// straight down toward the pot.
[[nodiscard]] inline SeatSpot seat_spot(const GameLayout& L, int slot) noexcept {
    if (slot <= 0) {
        return SeatSpot{Pt{L.table_center.x, L.table_center.y + L.table_ry * 1.04f}, 1.0f};
    }
    static constexpr std::array<float, 5> kSeatAngleDeg{135.0f, 175.0f, 218.0f, 268.0f, 312.0f};
    static constexpr float kSeatInset = 0.80f;  // pull seats off the rail onto the felt
    const SeatSpot r = rim_spot(L, kSeatAngleDeg[static_cast<std::size_t>(slot - 1)]);
    return SeatSpot{Pt{L.table_center.x + (r.pos.x - L.table_center.x) * kSeatInset,
                       L.table_center.y + (r.pos.y - L.table_center.y) * kSeatInset},
                    r.scale};
}

// Convenience: a seat's center point only (callers that don't need the scale).
[[nodiscard]] inline Pt seat_center(const GameLayout& L, int slot) noexcept {
    return seat_spot(L, slot).pos;
}

}  // namespace poker_trainer::render
