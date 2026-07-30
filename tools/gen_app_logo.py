#!/usr/bin/env python3
"""Generate the app logo monogram traced from the author's drawing.

The source drawing (an arched P/t monogram with the dealer-button chip set into its
dome) is a 78x190 px mark on a 1920x1080 canvas -- far too small to ship as a raster
at any size the Root screen wants. This redraws it as vector so it stays crisp, and
routes it through the same assets/svg -> tools/rasterize_assets.py pipeline as every
other generated asset.

Proportions are traced from the drawing; the chip's blue and the M's green are the
author's own values, which already match the dealer button.

The mark colour is a parameter because the drawing uses a near-black (#1E1E1E) that
would be invisible against the app's near-black background (bg_primary #1A1210).
"""

from __future__ import annotations

import argparse
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

# Traced from the source drawing (ink 78x190, aspect 0.41).
W, H = 420, 1024
STROKE = 54          # monogram stroke weight
SIDE_L, SIDE_R = 27 + STROKE / 2, 393 - STROKE / 2
DOME_CY = 250        # centre of the arch's semicircular top
BOTTOM = 997
CROSSBAR_Y = 620     # the t's horizontal
MULLION_X = W / 2    # the t's stem
MULLION_TOP = 415
CHIP_CX, CHIP_CY, CHIP_R = W / 2, 232, 150

# The author's own colours, unchanged: these already match dealer_button.svg.
CHIP_BLUE = "#2E6E9E"
CHIP_BLUE_RIM = "#4A86B4"
CHIP_GREEN = "#799A65"

# Brass reads against the dim cardroom background; the drawing's near-black does not.
MARK_BRASS = "#EFB42E"
MARK_DARK = "#1E1E1E"


def svg(mark: str) -> str:
    dome_r = (SIDE_R - SIDE_L) / 2
    return f"""<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">
  <!-- App logo: an arched monogram (P over a lowercase t) with the dealer-button chip
       set into the dome. Traced from the author's drawing by tools/gen_app_logo.py --
       regenerate rather than hand-editing.

       Canvas aspect is {W}/{H} = {W / H:.3f} and MUST stay in step with kLogoAspect in
       src/animations/button_morph.cpp: draw_image_slot stretches the art to fill its
       rect without preserving aspect, so any disagreement distorts the chip into an
       ellipse. -->
  <g fill="none" stroke="{mark}" stroke-width="{STROKE}"
     stroke-linecap="square" stroke-linejoin="round">
    <!-- Arch: straight sides rising into a semicircular dome, closed by a flat base. -->
    <path d="M {SIDE_L} {BOTTOM}
             L {SIDE_L} {DOME_CY}
             A {dome_r} {dome_r} 0 0 1 {SIDE_R} {DOME_CY}
             L {SIDE_R} {BOTTOM}
             Z"/>
    <!-- The t: a full-width crossbar and a stem down to the base. -->
    <path d="M {SIDE_L} {CROSSBAR_Y} L {SIDE_R} {CROSSBAR_Y}"/>
    <path d="M {MULLION_X} {MULLION_TOP} L {MULLION_X} {BOTTOM}"/>
  </g>

  <!-- Chip, drawn last so it sits over the arch stroke exactly as in the drawing.
       Simplified from dealer_button.svg (no dice inserts or dashed ring): at logo
       scale those read as noise, and the author's drawing drops them too. -->
  <circle cx="{CHIP_CX}" cy="{CHIP_CY}" r="{CHIP_R}" fill="{CHIP_BLUE}"/>
  <circle cx="{CHIP_CX}" cy="{CHIP_CY}" r="{CHIP_R - 9}" fill="none"
          stroke="{CHIP_BLUE_RIM}" stroke-width="10"/>
  <!-- M monogram, as paths so no font is needed. -->
  <path d="M {CHIP_CX - 74} {CHIP_CY + 62}
           L {CHIP_CX - 74} {CHIP_CY - 62}
           L {CHIP_CX} {CHIP_CY + 16}
           L {CHIP_CX + 74} {CHIP_CY - 62}
           L {CHIP_CX + 74} {CHIP_CY + 62}"
        fill="none" stroke="{CHIP_GREEN}" stroke-width="30"
        stroke-linecap="square" stroke-linejoin="miter"/>
</svg>
"""


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dark", action="store_true",
                    help="use the drawing's near-black mark instead of brass")
    ap.add_argument("-o", "--out", type=Path,
                    default=REPO / "assets" / "svg" / "tier1" / "app_logo.svg")
    args = ap.parse_args()
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(svg(MARK_DARK if args.dark else MARK_BRASS))
    print(f"wrote {args.out}  {W}x{H}  aspect {W / H:.3f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
