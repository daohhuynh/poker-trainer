#!/usr/bin/env python3
"""Generate table_felt.svg in the renderer's own first-person D projection.

The Game screen does not draw a flat top-down oval. render/layout.hpp defines a
foreshortened D: the horizontal half-width lerps from a narrow far rim at the top
to a wide near rim at the bottom, and the right wedge is flattened into the
dealer's straight chord. Seats and chip stacks are positioned from that same
rim, so felt art in any other projection cannot line up with them.

This script reproduces rim_spot() exactly:

    depth  = (sin(a) + 1) / 2                       1 at the near rim, 0 at the far
    half_w = far_rx + depth * (rx - far_rx)
    x, y   = cos(a) * half_w,  sin(a) * ry

Because x scales only with the canvas width and y only with its height, the
shape is a pure axis-aligned scaling of one normalized form. Authoring in
normalized space and letting the renderer stretch the image into the rim's
bounding box therefore reproduces the shape exactly at every window aspect --
no distortion, and no need to re-export when the window resizes.

Keep the constants below in lock-step with render/layout.hpp.
"""

from __future__ import annotations

import math
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
OUT = REPO / "assets" / "svg" / "tier2" / "table_felt.svg"

# --- mirrored from src/render/layout.hpp -------------------------------------
RX = 0.40          # L.table_rx      / w   near (bottom) half-width
FAR_RX = 0.23      # L.table_far_rx  / w   far (top) half-width
RY = 0.22          # L.table_ry      / h   vertical half-extent
FLAT_HALF_ANGLE = 42.0   # kFeltFlatHalfAngleDeg
STEPS = 256        # the renderer traces 64; oversample so the art edge is smooth

# --- art proportions ---------------------------------------------------------
RAIL_FRAC = 0.085  # rail band width, as a fraction of the shape, drawn INWARD
TRIM_FRAC = 0.012  # brass trim line inboard of the rail
CANVAS_W = 1536    # export resolution; aspect is irrelevant (the image is stretched)

FELT_DARK = "#1E100B"
FELT_MID = "#2A160F"
FELT_LIGHT = "#3A1E14"
RAIL_DARK = "#2B1A12"
RAIL_MID = "#4A2E1D"
RAIL_LIGHT = "#5C4436"
BRASS = "#A87B4A"
BRASS_LIGHT = "#D9A441"
SHADOW = "#120C0A"


def rim(angle_deg: float, shrink: float = 1.0) -> tuple[float, float]:
    """A point on the perspective rim, in normalized (w, h) units.

    shrink scales the point toward the table centre, which is how the rail's
    inner edge and the trim ring are derived -- so every ring stays in the same
    projection as the outer rim instead of being a naive offset curve.
    """
    a = math.radians(angle_deg)
    ex, ey = math.cos(a), math.sin(a)
    depth = (ey + 1.0) * 0.5
    half_w = FAR_RX + depth * (RX - FAR_RX)
    return ex * half_w * shrink, ey * RY * shrink


def d_path(shrink: float = 1.0) -> list[tuple[float, float]]:
    """The closed D outline: rim from the wedge's lower edge round to its upper
    edge; closing the path turns the open right span into the dealer's chord."""
    lo, hi = FLAT_HALF_ANGLE, 360.0 - FLAT_HALF_ANGLE
    return [rim(lo + (hi - lo) * i / STEPS, shrink) for i in range(STEPS + 1)]


def main() -> int:
    outer = d_path(1.0)
    xs = [p[0] for p in outer]
    ys = [p[1] for p in outer]
    min_x, max_x = min(xs), max(xs)
    min_y, max_y = min(ys), max(ys)
    span_x, span_y = max_x - min_x, max_y - min_y

    # Map normalized coords into the export canvas. The renderer stretches this
    # image into the identical bounding box, so the mapping cancels out.
    canvas_h = round(CANVAS_W * span_y / span_x)

    def to_px(p: tuple[float, float]) -> tuple[float, float]:
        return ((p[0] - min_x) / span_x * CANVAS_W,
                (p[1] - min_y) / span_y * canvas_h)

    def poly(shrink: float) -> str:
        pts = [to_px(p) for p in d_path(shrink)]
        head = f"M {pts[0][0]:.2f} {pts[0][1]:.2f}"
        body = " ".join(f"L {x:.2f} {y:.2f}" for x, y in pts[1:])
        return f"{head} {body} Z"

    rail_outer = poly(1.0)
    rail_inner = poly(1.0 - RAIL_FRAC)
    trim_ring = poly(1.0 - RAIL_FRAC - TRIM_FRAC)
    felt_edge = poly(1.0 - RAIL_FRAC - TRIM_FRAC * 2.0)

    # A quiet centre is a hard requirement: the pot, community cards and every
    # chip stack render on top of it. Visual interest lives at the rail.
    glow_cx, glow_cy = to_px((0.0, -RY * 0.10))
    glow_r = CANVAS_W * 0.30

    svg = f"""<svg xmlns="http://www.w3.org/2000/svg" width="{CANVAS_W}" height="{canvas_h}" viewBox="0 0 {CANVAS_W} {canvas_h}">
  <!-- Poker table felt, authored in the renderer's foreshortened D projection
       (see tools/gen_table_felt.py). The outer edge is the exact rim that
       render/layout.hpp's rim_spot() traces, so seats and chip stacks sit where
       they should. Regenerate rather than hand-editing. -->
  <defs>
    <radialGradient id="feltGlow" cx="{glow_cx / CANVAS_W:.4f}" cy="{glow_cy / canvas_h:.4f}" r="{glow_r / CANVAS_W:.4f}">
      <stop offset="0%" stop-color="{FELT_LIGHT}" stop-opacity="0.85"/>
      <stop offset="55%" stop-color="{FELT_MID}" stop-opacity="0.45"/>
      <stop offset="100%" stop-color="{FELT_DARK}" stop-opacity="0"/>
    </radialGradient>
    <linearGradient id="railShade" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="{RAIL_DARK}"/>
      <stop offset="45%" stop-color="{RAIL_MID}"/>
      <stop offset="100%" stop-color="{RAIL_LIGHT}"/>
    </linearGradient>
    <linearGradient id="trimShade" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="{BRASS}"/>
      <stop offset="50%" stop-color="{BRASS_LIGHT}"/>
      <stop offset="100%" stop-color="{BRASS}"/>
    </linearGradient>
  </defs>

  <!-- padded leather / wood rail: the whole shape, with the felt punched over it -->
  <path d="{rail_outer}" fill="url(#railShade)"/>
  <path d="{rail_outer}" fill="none" stroke="{SHADOW}" stroke-width="{CANVAS_W * 0.004:.1f}" stroke-opacity="0.75"/>

  <!-- brass trim where rail meets felt -->
  <path d="{trim_ring}" fill="url(#trimShade)"/>

  <!-- felt bed -->
  <path d="{felt_edge}" fill="{FELT_DARK}"/>
  <path d="{felt_edge}" fill="url(#feltGlow)"/>
  <!-- inner shadow: the felt darkens where it tucks under the rail -->
  <path d="{rail_inner}" fill="none" stroke="{SHADOW}" stroke-width="{CANVAS_W * 0.012:.1f}" stroke-opacity="0.45"/>
</svg>
"""
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(svg)

    print(f"wrote {OUT.relative_to(REPO)}  {CANVAS_W}x{canvas_h}")
    print("normalized rim bounding box (renderer must match):")
    print(f"  x: {min_x:+.6f} .. {max_x:+.6f}  (units of canvas width)")
    print(f"  y: {min_y:+.6f} .. {max_y:+.6f}  (units of canvas height)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
