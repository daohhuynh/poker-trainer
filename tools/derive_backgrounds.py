#!/usr/bin/env python3
"""Produce the two backdrop PNGs the app draws behind its screens.

ARCHITECTURE.md Module 2 specifies a single source render of the high-limit room
blurred at asset-preparation time so the app never pays for a runtime blur. What
ships is two files, not one per screen:

    background_room.png   heavily blurred photo     Root, Mode Selection,
                                                    Post-Round, Tutorial Complete
    background_game.png   generated pool of light   Game screen

The room is derived from the photograph below. The pool of light is not derived
from anything -- see POOL_OF_LIGHT for why the Game screen stopped showing the
room at all.

The four menu-side screens share one asset rather than taking one each: they are
all menus, they all want the same heavy blur, and a crossfade between two
identical images is not a transition. The focus-pull the spec describes happens
once, at the step that carries the idea -- room to pool of light, when you sit
down at the table.

Output is PNG because the decoder is compiled STBI_ONLY_PNG (see
src/assets/loader.cpp); a JPG here would silently fail to load.

The same module budgets ~1.3 MB for the backdrop set. A full-depth 2560x1440
truecolour PNG blows through that on its own, so the room is colour-quantized to
fit. Blur destroys high-frequency detail, which is exactly what makes heavy
quantization invisible here. Dithering suppresses banding across the smooth tonal
ramps that survive the blur.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SOURCE = REPO / "assets" / "source" / "room_source.jpg"

WIDTH, HEIGHT = 2560, 1440

# (output path, gaussian sigma at 2560px, stored width, palette colours)
#
# Sigma sits at the midpoint of the heavy band the architecture specifies for the
# innermost menu backdrop, applied at full 2560px width so the blur radius means
# what the spec says it means.
#
# Stored resolution is then matched to the blur. A Gaussian of sigma s erases
# essentially all spatial detail finer than about 2s pixels, so storing a
# heavily-blurred image at full width is paying for information the blur already
# destroyed. At sigma 16 the room carries no detail below ~32px and is visually
# identical stored at 1/4 width and bilinear-upscaled by the GPU. This is what
# brings the backdrop set inside the ~1.3 MB budget in Module 2 -- quantizing
# alone could not -- and it matters here because the room is Tier 1 and therefore
# on the critical path to first render.
#
# Palette size follows the same logic: the heavier the blur, the fewer colours
# needed before quantization becomes visible.
ROOM = ("tier1/background_room.png", 16, 720, 96)

# The Game screen does not show the room at all. It shows a pool of light.
#
# The room was sharpest here, which put the most visual detail on the one screen
# where the user has the least attention to spare: the Game screen is mental
# arithmetic under a timer, with the table, dealer, chips, cards and math column all
# competing already. It also had it backwards photographically -- a camera focused on
# the felt throws the room OUT of focus, so a crisp room behind the table is the
# unrealistic state, not the realistic one.
#
# So this backdrop is generated rather than derived: a warm elliptical falloff over
# black, as if the only light in the room hangs above the table. That kills the
# luminance competition (blur alone would not -- a blurred chandelier is still a
# large bright blob) while keeping the table and the dealer standing in a lit space
# rather than floating in a void, which is what pure black would have done to a
# cut-out figure whose legs run off the bottom edge.
#
# The pool is centred and sized to cover BOTH the felt (table_center 0.50w/0.595h in
# render/layout.hpp) and the dealer standing to its right (dealer_tl.x 0.700w, running
# to ~0.96w). A dark-suited figure on pure black loses its silhouette entirely, so the
# light has to reach him.
#
# Centre / radii are fractions of the canvas, so the ellipse stretches with the window
# exactly as the table does (table_rx scales with width, table_ry with height) and the
# two stay in proportion at every aspect.
POOL_OF_LIGHT = {
    "path": "tier2/background_game.png",
    "size": (480, 270),      # a smooth gradient upscales perfectly; the GPU's
                         # bilinear filter also smooths the ramp, so a larger
                         # store measured no less banded and cost 3x the bytes
    "center": (0.52, 0.60),
    "radii": (0.58, 0.52),
    "inner": "#31241B",      # dim warm lamplight, not a bright casino wash
    "outer": "#000000",
}

# The app paints its own bg_primary tint over the room at runtime, so bake only a
# slight darken here for text legibility. Baking the full mood would double up
# with the theme tint and crush the photo to mud.
DARKEN_PERCENT = 12


def run(cmd: list[str]) -> None:
    subprocess.run(cmd, check=True, capture_output=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=SOURCE)
    args = parser.parse_args()

    if not args.source.exists():
        print(f"error: source photo not found at {args.source}", file=sys.stderr)
        return 1

    dims = subprocess.run(
        ["magick", "identify", "-format", "%wx%h", str(args.source)],
        check=True, capture_output=True, text=True,
    ).stdout
    src_w, src_h = (int(v) for v in dims.split("x"))
    print(f"source: {args.source.relative_to(REPO)}  {src_w}x{src_h}")
    if src_w < WIDTH or src_h < HEIGHT:
        print(f"error: source is smaller than {WIDTH}x{HEIGHT}; refusing to upscale",
              file=sys.stderr)
        return 1

    total = 0
    # --- the generated pool of light (Game screen) ---
    pool = POOL_OF_LIGHT
    pool_out = REPO / "assets" / "images" / pool["path"]
    pool_out.parent.mkdir(parents=True, exist_ok=True)
    pw, ph = pool["size"]
    cx, cy = pool["center"][0] * pw, pool["center"][1] * ph
    rx, ry = pool["radii"][0] * pw, pool["radii"][1] * ph
    run([
        "magick", "-size", f"{pw}x{ph}",
        "-define", f"gradient:center={cx:.0f},{cy:.0f}",
        "-define", f"gradient:radii={rx:.0f},{ry:.0f}",
        f"radial-gradient:{pool['inner']}-{pool['outer']}",
        # Smooth ramps band badly once quantized; dither instead of reducing colours.
        "-dither", "FloydSteinberg",
        "-strip", "-define", "png:compression-level=9",
        str(pool_out),
    ])
    pool_size = pool_out.stat().st_size
    total += pool_size
    print(f"  {pool['path']:<32} POOL OF LIGHT  {pw}x{ph}  {pool_size / 1024:.0f} KB")

    # --- the blurred room (Root, Mode Selection, Post-Round, Tutorial Complete) ---
    rel, sigma, store_w, colours = ROOM
    out = REPO / "assets" / "images" / rel
    out.parent.mkdir(parents=True, exist_ok=True)
    store_h = store_w * HEIGHT // WIDTH
    run([
        "magick", str(args.source),
        # Centre-crop to exactly 16:9 at full width before blurring, so the framing
        # is a property of the crop rather than of the blur pass -- re-deriving at a
        # different sigma must not shift the composition.
        "-resize", f"{WIDTH}x{HEIGHT}^",
        "-gravity", "center",
        "-extent", f"{WIDTH}x{HEIGHT}",
        # Blur at full width so the radius matches the spec's stated pixels...
        "-blur", f"0x{sigma}",
        # ...then downsample to the storage resolution the blur justifies.
        "-resize", f"{store_w}x{store_h}",
        "-brightness-contrast", f"-{DARKEN_PERCENT}x0",
        "-colors", str(colours),
        "-dither", "FloydSteinberg",
        "-strip",
        "-define", "png:compression-level=9",
        str(out),
    ])
    size = out.stat().st_size
    total += size
    geom = subprocess.run(
        ["magick", "identify", "-format", "%wx%h %[type]", str(out)],
        check=True, capture_output=True, text=True,
    ).stdout
    print(f"  {rel:<32} sigma={sigma:<3} colors={colours:<4} "
          f"{geom}  {size / 1024:.0f} KB")

    print(f"\ncombined: {total / 1024 / 1024:.2f} MB "
          f"(ARCHITECTURE Module 2 budget: ~1.3 MB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
