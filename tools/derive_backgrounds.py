#!/usr/bin/env python3
"""Derive the three room-background PNGs from one source photograph.

ARCHITECTURE.md Module 2 specifies a single source render of the high-limit room,
blurred three ways at asset-preparation time so the app never pays for a runtime
blur:

    background_root.png   heavily blurred   ~12-20 px radius equivalent
    background_mode.png   medium blur       ~6-10 px radius equivalent
    background_game.png   light blur        ~2-4 px radius equivalent

The screens crossfade between these variants to fake a camera focus-pull, so all
three must be the same crop of the same source at the same size -- any framing
drift between variants shows up as a jump during the transition.

Output is PNG because the decoder is compiled STBI_ONLY_PNG (see
src/assets/loader.cpp); a JPG here would silently fail to load.

The same module budgets ~1.3 MB for all three combined. A full-depth 2560x1440
truecolour PNG blows through that on its own, so each variant is colour-quantized
to fit. Blur destroys high-frequency detail, which is exactly what makes heavy
quantization invisible here -- and the heavier the blur, the fewer colours the
variant needs. Dithering suppresses banding across the smooth tonal ramps that
survive the blur.
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
# Sigma is the midpoint of each band the architecture specifies, applied at full
# 2560px width so the blur radius means what the spec says it means.
#
# Stored resolution is then matched to the blur. A Gaussian of sigma s erases
# essentially all spatial detail finer than about 2s pixels, so storing a
# heavily-blurred variant at full width is paying for information the blur
# already destroyed. background_root (sigma 16) carries no detail below ~32px and
# is visually identical stored at 1/4 width and bilinear-upscaled by the GPU;
# background_game (sigma 3) is only lightly blurred and keeps most of its width.
# This is what brings the set inside the ~1.3 MB budget in Module 2 -- quantizing
# alone could not, and it matters most for background_root, which is Tier 1 and
# therefore on the critical path to first render.
#
# Palette size follows the same logic: the heavier the blur, the fewer colours
# needed before quantization becomes visible.
# The Game screen is deliberately NOT in this list -- see POOL_OF_LIGHT below.
#
# Root and Mode Selection use IDENTICAL parameters on purpose. Mode used to sit at
# sigma 8 so the room "resolved" a step as you moved inward, but both screens are
# just menus, the crossfade between them was not worth the switch, and the heavier
# blur is what makes their buttons dominate. Matching Root also drops Mode from
# 442 KB to the same 139 KB.
#
# The step that carries the whole idea is now the last one -- room to pool of light
# when you sit down at the table.
VARIANTS = [
    ("tier1/background_root.png", 16, 720, 96),
    ("tier2/background_mode.png", 16, 720, 96),
]

# The Game screen does not show the room at all. It shows a pool of light.
#
# The room was sharpest here, which put the most visual detail on the one screen
# where the user has the least attention to spare: the Game screen is mental
# arithmetic under a timer, with the table, dealer, chips, cards and math column all
# competing already. It also had it backwards photographically -- a camera focused on
# the felt throws the room OUT of focus, so a crisp room behind the table is the
# unrealistic state, not the realistic one.
#
# So this variant is generated rather than derived: a warm elliptical falloff over
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

    for rel, sigma, store_w, colours in VARIANTS:
        out = REPO / "assets" / "images" / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        store_h = store_w * HEIGHT // WIDTH
        run([
            "magick", str(args.source),
            # Centre-crop to exactly 16:9 at full width first. Cropping before the
            # blur keeps every variant on identical framing, which the crossfade
            # between them depends on.
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
