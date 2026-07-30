#!/usr/bin/env python3
"""Rasterize the SVG source art under assets/svg/ into the shipped PNGs.

Every generated PNG in assets/images/ comes from exactly one SVG in assets/svg/
at the mirrored path. This script is the single place that conversion happens, so
all 84 generated assets go through one verified, alpha-safe pipeline:

    inkscape in.svg --export-type=png --export-width=W --export-height=H

Plain Inkscape export preserves the transparent canvas and antialiased partial
alpha. Notably absent: any ImageMagick -alpha associate/disassociate wrapper.
That idiom is the usual premultiply fix for resize halos, but in ImageMagick 7 it
turns every transparent pixel opaque black -- measured, not assumed. Inkscape
renders straight to the target size here, so no resize step is needed at all.

The three background_*.png files are NOT produced here: they derive from a
photograph via tools/derive_backgrounds.py.

Usage:
    python3 tools/rasterize_assets.py            # rasterize everything
    python3 tools/rasterize_assets.py --check    # verify PNGs without rewriting
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SVG_ROOT = REPO / "assets" / "svg"
PNG_ROOT = REPO / "assets" / "images"

# Target pixel size per asset, keyed by the path relative to assets/svg/ with the
# .svg suffix dropped. Directory-level defaults cover the repetitive families
# (52 cards, 8 chips, 13 icons) so adding a card never means editing this table.
EXACT_SIZES: dict[str, tuple[int, int]] = {
    # Portrait monogram, not the old wide wordmark. Aspect must match
    # kLogoAspect in src/animations/button_morph.cpp.
    "tier1/app_logo": (420, 1024),
    "tier1/butler_neutral": (1024, 1536),
    "tier1/butler_raised": (1024, 1536),
    "tier1/dealer_button": (512, 512),
    "tier2/butler_profile": (1024, 1800),
    # Matches the aspect tools/gen_table_felt.py authors (the rim's bounding box in
    # the foreshortened racetrack projection), not the 16:9 canvas. The renderer
    # stretches this into the runtime rim box, so a mismatched aspect here would
    # only waste resolution -- but it would waste a lot of it.
    "tier2/table_felt": (2048, 1577),
    "tier2/side_pot_all_in_marker": (512, 512),
    "tier4/frog_base": (1024, 1024),
    "tier4/frog_expression_pass": (1024, 1024),
    "tier4/frog_expression_fail": (1024, 1024),
}

# Longest matching prefix wins, so a specific entry in EXACT_SIZES always beats
# the family default below.
DIR_SIZES: list[tuple[str, tuple[int, int]]] = [
    ("tier2/cards/", (400, 560)),
    ("tier2/chips/", (512, 512)),
    ("tier2/icons/", (256, 256)),
    ("tier1/icons/", (256, 256)),
]


def target_size(rel_key: str) -> tuple[int, int] | None:
    if rel_key in EXACT_SIZES:
        return EXACT_SIZES[rel_key]
    for prefix, size in DIR_SIZES:
        if rel_key.startswith(prefix):
            return size
    return None


def rasterize(svg: Path, png: Path, width: int, height: int) -> None:
    png.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [
            "inkscape",
            str(svg),
            "--export-type=png",
            f"--export-filename={png}",
            f"--export-width={width}",
            f"--export-height={height}",
        ],
        check=True,
        capture_output=True,
    )


def probe(png: Path) -> tuple[str, str, float, float]:
    """Return (geometry, channels, alpha_min, alpha_max) for a rasterized PNG."""
    geom = subprocess.run(
        ["magick", "identify", "-format", "%wx%h|%[channels]", str(png)],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    stats = subprocess.run(
        [
            "magick", str(png), "-alpha", "extract",
            "-format", "%[fx:minima*255]|%[fx:maxima*255]", "info:",
        ],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    geometry, channels = geom.split("|")
    amin, amax = (float(v) for v in stats.split("|"))
    return geometry, channels, amin, amax


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true",
                        help="verify existing PNGs instead of rewriting them")
    args = parser.parse_args()

    if not SVG_ROOT.is_dir():
        print(f"error: no SVG source tree at {SVG_ROOT}", file=sys.stderr)
        return 1

    svgs = sorted(SVG_ROOT.rglob("*.svg"))
    if not svgs:
        print(f"error: no SVGs found under {SVG_ROOT}", file=sys.stderr)
        return 1

    unsized: list[str] = []
    opaque: list[str] = []
    written = 0

    for svg in svgs:
        rel_key = svg.relative_to(SVG_ROOT).with_suffix("").as_posix()
        size = target_size(rel_key)
        if size is None:
            unsized.append(rel_key)
            continue

        png = PNG_ROOT / svg.relative_to(SVG_ROOT).with_suffix(".png")
        width, height = size
        if not args.check:
            rasterize(svg, png, width, height)
            written += 1
        if not png.exists():
            print(f"MISSING  {png.relative_to(REPO)}")
            continue

        geometry, channels, amin, _amax = probe(png)
        if "a" not in channels:
            opaque.append(rel_key)
        # Card faces and the felt are opaque artwork by design; everything else
        # is cut-out art and must carry at least one fully transparent pixel.
        cutout = not (rel_key.startswith("tier2/cards/") or rel_key == "tier2/table_felt")
        if cutout and amin > 0:
            opaque.append(f"{rel_key} (no transparent pixel, alpha_min={amin:.0f})")
        print(f"{geometry:>11}  {channels:<12}  {png.relative_to(REPO)}")

    print()
    print(f"rasterized {written} PNG(s) from {len(svgs)} SVG(s)")
    if unsized:
        print(f"\nERROR: no target size for {len(unsized)} SVG(s) "
              f"-- add them to EXACT_SIZES or DIR_SIZES:", file=sys.stderr)
        for u in unsized:
            print(f"  {u}", file=sys.stderr)
        return 1
    if opaque:
        print(f"\nERROR: {len(opaque)} asset(s) lost transparency:", file=sys.stderr)
        for o in opaque:
            print(f"  {o}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
