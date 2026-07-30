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
// The screen is composed FIRST-PERSON, from the hero's seat looking across the
// table. The felt is a LEFT-RIGHT SYMMETRIC racetrack oval in MILD perspective:
// the far (top) rim is modestly narrower than the near (bottom) one, the way a
// real table looks from a chair at one end. It is deliberately NOT a wide "D"
// with a flat dealer chord — that one-sided flattening is what made the table
// read as a lopsided egg — and it is sized to sit inside the canvas with margin
// on both sides rather than running off the edges.
//
// The hero's hole cards sit dead-CENTER at the bottom, nearest the camera and
// largest; the five opponents are chip stacks (no avatars) around the rest of the
// ring — up the near-left rail, across the left end, along the far rail and up to
// the far-RIGHT — each drawn slightly smaller the farther it recedes; the Butler
// dealer stands at the RIGHT-hand end of the table; the community + pot sit
// mid-table; Z09's math inputs stay on the LEFT. The perspective is a STATIC 2D
// composition (horizontal width and seat size scale with a per-point depth term)
// — NOT a camera or projection matrix — so it stays within the no-3D rule and
// swaps cleanly for authored art.
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
// Close to the felt's own far/near width ratio, so a far seat shrinks by about as
// much as the table does under it — and its label stays legible.
inline constexpr float kFarSeatScale = 0.78f;
inline constexpr float kLayoutPi = 3.14159265358979323846f;

// Silhouette exponent of the felt outline, a superellipse |x|^n + |y|^n = 1 in
// normalized half-extents. n == 2 is a plain ellipse; raising it straightens the
// long top and bottom rails while keeping the two ends round, which is the
// racetrack outline of a real poker table. The rim is closed and symmetric about
// the vertical axis — there is no flat dealer chord.
inline constexpr float kFeltEdgeExponent = 2.9f;

struct GameLayout {
    float w{0.0f};
    float h{0.0f};

    // Foreshortened-perspective felt about table_center: table_ry is the vertical
    // half-extent, table_rx the NEAR (bottom) horizontal half-width, table_far_rx
    // the FAR (top) one. table_far_rx < table_rx makes the table recede.
    //
    // Both are half-widths at a DEPTH, not the silhouette's bounding box: the rim
    // is widest at mid-depth, where the half-width is the mean of the two. Sizing
    // the table against the canvas therefore works off (table_rx + table_far_rx)/2.
    Pt table_center{};
    float table_rx{0.0f};
    float table_far_rx{0.0f};
    float table_ry{0.0f};

    // Multiplier applied to every chip dimension. The chip metrics in
    // render_constants.hpp (kChipRadius, kChipStackStep, kChipColumnPitch) are
    // ABSOLUTE pixels, so on their own they neither shrink when the felt does nor
    // grow on a high-DPI canvas: a full-height stack that read as a neat pile on the
    // old, larger table rendered as a tower overhanging the current one, and at 4K
    // the same chips would be specks. Deriving the multiplier from table_ry — which
    // is itself canvas-relative — keeps a stack proportionate to the felt it sits on
    // at every window size.
    float chip_scale{1.0f};

    // Pot cluster anchor (mid-table; community sits above it, the hero's cards below).
    Pt pot{};

    // Top-left reference column for the legend / pot / blinds stack.
    Pt info_anchor{};

    // Dealer box (standing at the right end), top-left + size.
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
    // The table sits in the LOWER-CENTER of the frame — far (top) rim at 0.405h,
    // near (bottom) rim at 0.785h — so the room fills the space above it and the
    // hero's cards have foreground below it. The rim is widest at mid-depth, where
    // the half-width is the mean of the two below, so the silhouette spans
    // 0.253w..0.747w. That quarter-canvas margin on each side is what keeps the
    // felt off both screen edges and clear of Z09's left-hand math column, which is
    // fixed-width in pixels and reaches about 0.02w + 280px.
    L.table_center = Pt{w * 0.50f, h * 0.595f};
    L.table_rx = w * 0.265f;      // near (bottom) half-width
    L.table_far_rx = w * 0.220f;  // far (top) half-width — 0.83x the near one
    L.table_ry = h * 0.19f;
    // The felt half-height the absolute chip constants in render_constants.hpp were
    // originally tuned against. Chips scale by how far the real felt departs from it.
    constexpr float kChipReferenceFeltRy = 220.0f;
    L.chip_scale = L.table_ry / kChipReferenceFeltRy;
    // Pot near the felt's visual middle (community just above, hero's cards below).
    L.pot = Pt{L.table_center.x, L.table_center.y - L.table_ry * 0.05f};
    L.info_anchor = Pt{w * 0.02f, h * 0.03f};
    // Dealer standing at the felt's right end, front-right of the hero. The box is
    // DELIBERATELY taller than the frame: the art is a full-length figure (1024 x
    // 1800, shoes at the canvas bottom) and the box bottom lands at 1.145h, so the
    // frame cuts him mid-shin instead of leaving him to stop in mid-air above the
    // rail. 0.278/0.87 is the art's 1024:1800 aspect on a 16:9 canvas, so he is not
    // squashed; the x origin puts his gloved hand on the felt's right rail.
    L.dealer_w = w * 0.278f;
    L.dealer_h = h * 0.87f;
    L.dealer_tl = Pt{w * 0.700f, h * 0.275f};
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
// closest to the camera, 270 = the far/top point, 0 = the right end), with its
// depth scale. Raising |cos| and |sin| to 2/n walks the superellipse rather than an
// ellipse; the horizontal half-width and the size scale then both lerp from the far
// rim to the near rim by depth, which is what foreshortens the shape.
[[nodiscard]] inline SeatSpot rim_spot(const GameLayout& L, float angle_deg) noexcept {
    const float a = angle_deg * (kLayoutPi / 180.0f);
    const float e = 2.0f / kFeltEdgeExponent;
    const float c = std::cos(a);
    const float s = std::sin(a);
    const float ux = std::copysign(std::pow(std::fabs(c), e), c);
    const float uy = std::copysign(std::pow(std::fabs(s), e), s);
    const float depth = (uy + 1.0f) * 0.5f;  // 1 at the near/bottom rim, 0 at the far/top
    const float half_w = L.table_far_rx + depth * (L.table_rx - L.table_far_rx);
    const float scale = kFarSeatScale + depth * (1.0f - kFarSeatScale);
    return SeatSpot{Pt{L.table_center.x + ux * half_w, L.table_center.y + uy * L.table_ry}, scale};
}

// First-person seat spot. Slot 0 is the hero, dead-CENTER at the bottom nearest
// the camera (largest; the hero's hole cards + label render here). Slots 1..5 are
// the five opponents as chip stacks around the rest of the ring — from the hero's
// near-LEFT, across the left end, along the far rail and up to the far-RIGHT —
// each scaled down as it recedes. Slot 4 is the far top-center seat
// (active_opponent_slot), so a Caller's pushed chips read straight down toward the
// pot. The near-right quarter is left empty: that is where the dealer stands, and
// a seat there would sit under his figure.
[[nodiscard]] inline SeatSpot seat_spot(const GameLayout& L, int slot) noexcept {
    if (slot <= 0) {
        return SeatSpot{Pt{L.table_center.x, L.table_center.y + L.table_ry * 1.04f}, 1.0f};
    }
    static constexpr std::array<float, 5> kSeatAngleDeg{140.0f, 188.0f, 230.0f, 270.0f, 312.0f};
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
