#!/usr/bin/env python3
"""Generate the 52 playing-card faces plus the card back as SVG source art.

Output goes to assets/svg/tier2/cards/<suit>_<rank>.svg (400x560 each), which
tools/rasterize_assets.py then turns into the shipped PNGs. This script is the
source of truth for the card artwork: tweak a constant here and re-run to
regenerate all 53 files consistently.

    python3 tools/gen_cards.py                  # write all 53 SVGs
    python3 tools/gen_cards.py --out-dir DIR    # write somewhere else (dry runs)
    python3 tools/gen_cards.py --dump-glyphs    # re-derive the RANK_GLYPHS table

Design notes
------------
* Everything is a vector path. There is no <text> element in the output, so the
  files render identically on a machine without Charter installed.
* The four suit pips are hand-authored paths, each drawn inside a nominal
  0..100 box so one placement helper positions pips and rank glyphs alike.
* The rank glyphs were baked once from Charter Black (a Carter transitional
  serif with lining figures, drawn for low-resolution output -- it stays legible
  when a 400x560 card is drawn 90px wide on the table). See RANK_GLYPHS and
  --dump-glyphs for how to regenerate them.
* Card corners are transparent: the artwork is the rounded rectangle itself,
  never an artboard-filling background rect.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import NamedTuple
from xml.etree import ElementTree

REPO = Path(__file__).resolve().parent.parent
DEFAULT_OUT_DIR = REPO / "assets" / "svg" / "tier2" / "cards"

# --------------------------------------------------------------------------
# Canvas and palette
# --------------------------------------------------------------------------

CARD_W = 400.0
CARD_H = 560.0
CARD_CX = CARD_W / 2.0
CARD_CY = CARD_H / 2.0

# The face plate is inset a hair so the antialiased rounded corners are not
# clipped by the artboard edge.
FACE_INSET = 3.0
FACE_RADIUS = 28.0

FACE_CREAM = "#F7F4EC"  # warm white; pure #FFFFFF reads cold against the felt
FACE_EDGE = "#C8B392"   # thin warm border
FACE_KEYLINE = "#E2D7C0"  # barely-there inner rule, a quality detail up close
FACE_RULE = "#C9B189"  # warm tan used for the court-card cartouche

SUIT_RED = "#C0392B"
SUIT_BLACK = "#1C1C1C"

# Card back: brass on mahogany, private-club rather than souvenir shop.
BACK_FIELD = "#3B241A"       # deep mahogany
BACK_EDGE = "#241610"        # warm shadow, defines the silhouette
BACK_LATTICE_DARK = "#6E4E2C"  # brass dark
BACK_LATTICE_LIGHT = "#A87B4A"  # brass mid
BACK_BORDER = "#A87B4A"
BACK_BORDER_HAIRLINE = "#D9A441"  # brass light

# --------------------------------------------------------------------------
# Shape primitives
# --------------------------------------------------------------------------


class Shape(NamedTuple):
    """A path plus the bounding box of its ink, used for optical placement."""

    d: str
    x: float
    y: float
    w: float
    h: float


def fmt(value: float) -> str:
    """Format a coordinate compactly: 4 decimals, no trailing zeros."""
    text = f"{value:.4f}".rstrip("0").rstrip(".")
    return text if text not in ("", "-") else "0"


def place(
    shape: Shape,
    cx: float,
    cy: float,
    height: float,
    max_width: float | None = None,
    upside_down: bool = False,
) -> str:
    """Emit `shape` centred on (cx, cy), scaled to `height` (or narrower).

    `max_width` exists for the "10" index, which is far wider than the other
    ranks and would otherwise crowd the card edge.
    """
    scale = height / shape.h
    if max_width is not None and shape.w * scale > max_width:
        scale = max_width / shape.w

    transform = [f"translate({fmt(cx)},{fmt(cy)})"]
    if upside_down:
        transform.append("rotate(180)")
    transform.append(f"scale({fmt(scale)})")
    transform.append(
        f"translate({fmt(-(shape.x + shape.w / 2.0))},{fmt(-(shape.y + shape.h / 2.0))})"
    )
    return f'<path transform="{" ".join(transform)}" d="{shape.d}"/>'


# --------------------------------------------------------------------------
# Suit pips -- hand-authored, each drawn to fill a nominal 0..100 box
# --------------------------------------------------------------------------

PIP_BOX = (0.0, 0.0, 100.0, 100.0)

SPADE_PIP = Shape(
    "M 50 0"
    " C 40 18 24 32 14 44"
    " C 2 58 5 76 22 78"
    " C 33 79 41 74 46 67"
    " C 46 81 40 92 28 100"
    " L 72 100"
    " C 60 92 54 81 54 67"
    " C 59 74 67 79 78 78"
    " C 95 76 98 58 86 44"
    " C 76 32 60 18 50 0 Z",
    *PIP_BOX,
)

HEART_PIP = Shape(
    "M 50 100"
    " C 28 79 2 61 2 36"
    " C 2 15 17 0 32 0"
    " C 42 0 48 7 50 15"
    " C 52 7 58 0 68 0"
    " C 83 0 98 15 98 36"
    " C 98 61 72 79 50 100 Z",
    *PIP_BOX,
)

# Straight-sided rhombus, narrower than tall the way real card diamonds are.
DIAMOND_PIP = Shape("M 50 0 L 82 50 L 50 100 L 18 50 Z", *PIP_BOX)

# Three overlapping lobes plus a flared stem, written as one path. All four
# subpaths wind the same direction so the default nonzero fill rule unions them.
CLUB_PIP = Shape(
    "M 26 24 a 24 24 0 1 1 48 0 a 24 24 0 1 1 -48 0 Z"
    " M 1 60 a 24 24 0 1 1 48 0 a 24 24 0 1 1 -48 0 Z"
    " M 51 60 a 24 24 0 1 1 48 0 a 24 24 0 1 1 -48 0 Z"
    " M 57 44 C 58 64 61 86 68 100 L 32 100 C 39 86 42 64 43 44 Z",
    *PIP_BOX,
)


class Suit(NamedTuple):
    key: str
    color: str
    pip: Shape
    # Optical correction: pips of equal height do not read as equal mass.
    pip_scale: float


SUITS: tuple[Suit, ...] = (
    Suit("spade", SUIT_BLACK, SPADE_PIP, 1.00),
    Suit("heart", SUIT_RED, HEART_PIP, 0.97),
    Suit("diamond", SUIT_RED, DIAMOND_PIP, 1.04),
    Suit("club", SUIT_BLACK, CLUB_PIP, 1.00),
)

# --------------------------------------------------------------------------
# Rank glyphs
# --------------------------------------------------------------------------

# Filename key -> label drawn on the card. Note "t" is the filename, "10" the
# glyph, matching both asset_paths.hpp and every real deck.
RANKS: tuple[tuple[str, str], ...] = (
    ("a", "A"),
    ("2", "2"),
    ("3", "3"),
    ("4", "4"),
    ("5", "5"),
    ("6", "6"),
    ("7", "7"),
    ("8", "8"),
    ("9", "9"),
    ("t", "10"),
    ("j", "J"),
    ("q", "Q"),
    ("k", "K"),
)

GLYPH_FONT_FAMILY = "Charter"
GLYPH_FONT_WEIGHT = "900"
GLYPH_FONT_SIZE = 100.0
# "10" is two glyphs wide; tighten it so the index block stays a sane width.
GLYPH_TRACKING: dict[str, float] = {"t": -6.0}

# Frozen output of `python3 tools/gen_cards.py --dump-glyphs`. Regenerate only
# if the typeface or tracking above changes; the values are Charter Black
# outlines at GLYPH_FONT_SIZE, with the bounding box Inkscape measured for each.
RANK_GLYPHS: dict[str, Shape] = {
    # A
    "a": Shape(
        "m 180.41992,98.681641 -7.91015,22.119139 h 15.96679 z M 177.7832,82.421875 h 14.4043 l 23.19336,60.986325 6.10351,0.78125 V 150 h -32.37304 v -5.81055 l 6.78711,-0.48828 0.78125,-1.31836 -5.27344,-14.89258 h -21.28906 l -5.0293,14.79493 1.02539,1.2207 6.39649,0.68359 V 150 h -24.1211 v -5.81055 l 6.49414,-0.39062 z",
        148.389, 82.4219, 73.0957, 67.5781,
    ),
    # 2
    "2": Shape(
        "m 470.89844,137.5 h 27.49023 l 1.80664,-8.98438 h 7.91016 V 150 h -52.49024 v -9.61914 q 17.38282,-12.35352 24.65821,-21.09375 7.32422,-8.78906 7.32422,-16.99219 0,-6.20117 -3.41797,-9.570311 -3.36914,-3.417968 -9.57032,-3.417968 -0.78125,0 -1.2207,0.04883 -0.43945,0 -0.78125,0.04883 v 15.087893 q -2.4414,1.07422 -4.49219,1.61133 -2.05078,0.48828 -3.80859,0.48828 -4.15039,0 -6.64062,-2.2461 -2.49024,-2.29492 -2.49024,-6.054684 0,-7.373047 6.98242,-12.011719 7.03125,-4.6875 18.35938,-4.6875 11.8164,0 18.65234,5.419922 6.83594,5.371093 6.83594,14.599611 0,9.22851 -8.54492,17.96875 -8.4961,8.74023 -26.5625,17.91992 z",
        455.176, 81.5918, 52.9297, 68.4082,
    ),
    # 3
    "3": Shape(
        "m 786.37695,114.59961 q 10.20508,0.58594 15.72266,4.98047 5.51758,4.3457 5.51758,11.8164 0,8.59375 -8.20313,14.25782 -8.20312,5.66406 -20.99609,5.66406 -11.47461,0 -18.21289,-3.71094 -6.68946,-3.71094 -6.68946,-9.91211 0,-3.80859 2.44141,-6.15234 2.49024,-2.34375 6.54297,-2.34375 1.36719,0 2.7832,0.24414 1.41602,0.24414 2.92969,0.73242 l 3.36914,13.13477 q 1.9043,0.39062 3.27149,0.58593 1.41601,0.19532 2.4414,0.19532 5.3711,0 8.69141,-3.36914 3.32031,-3.41797 3.32031,-8.93555 0,-5.56641 -3.85742,-8.74023 -3.8086,-3.22266 -10.44922,-3.22266 -1.75781,0 -3.41797,0.19531 -1.66016,0.19531 -3.27148,0.58594 v -8.69141 q 1.07422,0.14649 2.05078,0.24414 0.97656,0.0488 1.85547,0.0488 6.83593,0 10.69336,-2.97851 3.90625,-3.02735 3.90625,-8.25196 0,-5.566404 -3.51563,-8.740232 -3.51562,-3.222656 -9.7168,-3.222656 -0.29296,0 -0.8789,0.04883 -0.58594,0.04883 -0.92774,0.04883 v 13.964842 q -1.70898,0.87891 -3.46679,1.31836 -1.70899,0.39063 -3.51563,0.39063 -4.24805,0 -6.78711,-2.2461 -2.49023,-2.24609 -2.49023,-5.957029 0,-6.49414 6.98242,-10.644531 7.03125,-4.150391 18.4082,-4.150391 10.98633,0 17.5293,4.638672 6.54297,4.589844 6.54297,12.158203 0,6.201176 -4.83399,10.400396 -4.83398,4.15039 -13.76953,5.61523 z",
        753.516, 81.7871, 54.1016, 69.5313,
    ),
    # 4
    "4": Shape(
        "m 1083.7891,98.583984 -20.6055,25.439456 h 20.6055 z m 2.0019,-16.259765 h 14.0137 v 41.699221 h 11.8164 v 9.375 h -11.8164 v 19.09179 h -16.0156 v -19.09179 h -31.2989 v -9.91211 z",
        1052.49, 82.3242, 59.1309, 70.166,
    ),
    # 5
    "5": Shape(
        "m 1359.4238,82.910156 h 44.3848 v 12.890625 h -34.4238 V 109.7168 q 4.0039,-1.41602 7.666,-2.09961 3.6621,-0.73242 7.0312,-0.73242 10.8887,0 17.5293,5.5664 6.6895,5.56641 6.6895,14.64844 0,11.03516 -8.4961,17.57812 -8.4961,6.54297 -22.9981,6.54297 -9.8144,0 -15.9179,-3.71093 -6.1035,-3.75977 -6.1035,-9.52149 0,-3.95508 2.6855,-6.54297 2.6856,-2.63672 6.8359,-2.63672 1.3184,0 2.8321,0.29297 1.5136,0.29297 3.3691,0.87891 l 2.9785,14.01367 q 0.1465,0 0.4883,0.0488 1.416,0.14648 2.1484,0.14648 6.3965,0 10.4981,-3.95508 4.1016,-3.95507 4.1016,-10.15625 0,-6.00585 -4.7364,-9.71679 -4.7363,-3.75977 -12.5,-3.75977 -3.2226,0 -6.7383,0.63477 -3.5156,0.58594 -7.3242,1.75781 z",
        1354.79, 82.9102, 53.5156, 68.3105,
    ),
    # 6
    "6": Shape(
        "m 1672.1191,114.69727 q -0.049,0.19531 -0.049,0.58593 -0.1465,3.75977 -0.1465,5.61524 0,11.52343 2.7832,17.48047 2.8321,5.9082 8.3008,5.9082 4.4922,0 7.1289,-3.95508 2.6856,-4.00391 2.6856,-10.74219 0,-8.49609 -3.2715,-12.10937 -3.2227,-3.66211 -10.7422,-3.66211 -1.8066,0 -3.4668,0.24414 -1.6602,0.19531 -3.2227,0.63477 z m 29.1016,-36.083989 V 87.5 l -6.2012,-0.878906 q -9.2285,1.02539 -14.8437,6.591797 -5.5664,5.517578 -7.0801,15.087889 3.4668,-2.09961 7.5195,-3.17383 4.1016,-1.12304 8.5938,-1.12304 9.082,0 15.2832,6.54297 6.2012,6.54296 6.2012,16.35742 0,11.08398 -7.6661,17.87109 -7.6171,6.73828 -20.2148,6.73828 -13.5254,0 -20.9473,-8.25195 -7.373,-8.25195 -7.373,-23.33984 0,-9.375 3.2715,-17.08985 3.2715,-7.714843 9.7168,-13.720702 5.7129,-5.371094 13.33,-7.910156 7.6172,-2.587891 17.9688,-2.587891 z",
        1654.49, 78.6133, 56.2012, 72.9004,
    ),
    # 7
    "7": Shape(
        "m 1958.3008,82.910156 h 51.9043 v 8.984375 l -33.1055,57.812499 v 3.27149 h -13.3789 l 33.2031,-55.468754 h -28.4179 l -1.6114,9.375004 h -8.5937 z",
        1958.3, 82.9102, 51.9043, 70.0684,
    ),
    # 8
    "8": Shape(
        "m 2273.1934,120.01953 q -1.6602,3.02735 -2.4903,5.56641 -0.7812,2.53906 -0.7812,4.73633 0,5.9082 3.7109,10.05859 3.7598,4.10156 9.082,4.10156 4.2969,0 7.2266,-2.58789 2.9785,-2.58789 2.9785,-6.29883 0,-3.90625 -4.0039,-7.17773 -3.9551,-3.32031 -15.7226,-8.39844 z m 20.8984,-6.20117 q 8.3008,3.125 11.8164,7.22656 3.5156,4.05274 3.5156,10.44922 0,9.375 -7.5683,14.69727 -7.5196,5.32226 -20.8496,5.32226 -12.2559,0 -19.6289,-5.37109 -7.3731,-5.3711 -7.3731,-14.16016 0,-5.76172 3.5156,-9.42383 3.5157,-3.71093 10.4004,-5.17578 -5.7129,-2.63672 -8.4961,-6.54297 -2.7343,-3.90625 -2.7343,-9.22851 0,-9.326174 7.2265,-14.697268 7.2266,-5.419921 19.7754,-5.419921 10.5957,0 16.8945,4.93164 6.2989,4.882813 6.2989,13.085938 0,4.687501 -3.2715,8.349611 -3.2227,3.61328 -9.5215,5.95703 z m -5.3711,-2.73438 q 1.4648,-2.39257 2.1484,-4.6875 0.7325,-2.29492 0.7325,-4.6875 0,-5.957027 -2.7344,-9.52148 -2.7344,-3.564453 -7.2754,-3.564453 -3.418,0 -5.6152,2.392578 -2.1973,2.34375 -2.1973,6.103516 0,4.052739 3.418,7.275389 3.4179,3.22266 11.5234,6.68945 z",
        2254.0, 81.4941, 55.4199, 70.0195,
    ),
    # 9
    "9": Shape(
        "m 2563.7207,155.51758 -3.3203,-8.59375 h 5.6152 q 9.7168,-3.75977 15.625,-9.17969 5.9082,-5.46875 8.5449,-13.23242 -3.7109,1.95312 -7.2753,2.92969 -3.5157,0.97656 -7.0313,0.97656 -9.5215,0 -15.7226,-6.15234 -6.1524,-6.15235 -6.1524,-15.67383 0,-11.230472 7.8125,-18.212894 7.8613,-6.982422 20.6055,-6.982422 12.5976,0 20.166,7.470703 7.6172,7.470704 7.6172,19.824223 0,16.65039 -12.2559,29.05273 -12.2558,12.35352 -34.2285,17.77344 z m 28.0762,-37.8418 q 0.3418,-2.24609 0.4883,-4.49219 0.1953,-2.24609 0.1953,-4.39453 0,-9.863279 -2.7344,-15.136716 -2.6856,-5.273438 -7.8613,-5.273438 -4.3946,0 -7.1778,4.150391 -2.7832,4.101562 -2.7832,10.791013 0,7.76367 3.711,11.76758 3.7597,4.00391 11.084,4.00391 0.9765,0 2.246,-0.3418 1.2696,-0.3418 2.8321,-1.07422 z",
        2554.0, 81.3965, 56.2012, 74.1211,
    ),
    # 10
    "t": Shape(
        "m 2883.5938,82.177734 h 10.3027 v 60.107426 l 1.123,1.51367 10.9864,0.48828 V 150 h -42.3829 v -5.81055 l 11.5723,-0.58593 1.2207,-1.70899 V 95.3125 l -1.3183,-1.123047 -14.3067,1.904297 v -6.005859 z m 56.8906,6.00586 q -5.0293,0 -7.4707,6.933593 -2.3926,6.884763 -2.3926,21.191403 0,14.79493 2.3437,21.53321 2.3926,6.73828 7.5196,6.73828 5.2246,0 7.6172,-6.73828 2.3925,-6.73828 2.3925,-21.53321 0,-14.59961 -2.4414,-21.337887 -2.3925,-6.787109 -7.5683,-6.787109 z m -0.098,-6.494141 q 13.1836,0 20.8008,9.277344 7.6172,9.277343 7.6172,25.439453 0,16.16211 -7.6172,25.48828 -7.6172,9.32617 -20.8008,9.32617 -12.9883,0 -20.5078,-9.27734 -7.4707,-9.32617 -7.4707,-25.53711 0,-16.16211 7.4707,-25.439453 7.4707,-9.277344 20.5078,-9.277344 z",
        2860.79, 81.6895, 108.013, 69.5312,
    ),
    # J
    "j": Shape(
        "m 3175.1953,90.576172 -1.709,-1.464844 -7.4707,-0.488281 v -5.712891 h 34.7657 v 5.712891 l -6.2012,0.488281 -1.6602,1.464844 V 123.0957 q 0,14.84375 -6.2011,21.48438 -6.2012,6.64062 -20.0196,6.64062 -3.7109,0 -7.5683,-0.63476 -3.8575,-0.58594 -7.9102,-1.80664 v -19.18946 h 9.1797 l 2.2949,13.62305 1.3184,1.26953 q 6.2011,-0.24414 8.6914,-4.73633 2.4902,-4.49218 2.4902,-16.74804 z",
        3151.22, 82.9102, 49.5606, 68.3105,
    ),
    # Q
    "q": Shape(
        "m 3473.7793,116.79688 q 0,12.20703 4.834,19.77539 4.834,7.51953 12.4023,7.51953 7.4707,0 12.0117,-7.66602 4.5899,-7.66601 4.5899,-20.50781 0,-12.40234 -4.5899,-19.628908 -4.541,-7.27539 -12.1093,-7.27539 -7.8614,0 -12.5,7.519531 -4.6387,7.470707 -4.6387,20.263677 z m 27.3437,32.61718 q 0.1465,5.66406 3.2715,8.39844 3.125,2.7832 9.3262,2.7832 2.9785,0 5.5176,-0.29297 2.539,-0.24414 4.8828,-0.78125 v 5.95704 q -5.3223,1.31835 -10.2051,1.95312 -4.8828,0.68359 -9.4238,0.68359 -11.7676,0 -17.1387,-4.19921 -5.3711,-4.1504 -5.3711,-13.23243 v -0.48828 q -13.1836,-2.14844 -20.5566,-11.23047 -7.3242,-9.13086 -7.3242,-23.14453 0,-15.47851 10.0586,-24.902341 10.0586,-9.423828 26.6601,-9.423828 6.8848,0 12.8418,1.855468 6.0059,1.806641 10.4492,5.273438 6.543,5.029297 10.0098,12.207033 3.4668,7.1289 3.4668,15.57617 0,12.79297 -7.2754,21.875 -7.2754,9.0332 -19.1895,11.13281 z",
        3454.1, 81.4941, 73.4863, 86.6211,
    ),
    # K
    "k": Shape(
        "m 3752.4902,150 v -5.81055 l 6.6895,-0.39062 1.6113,-1.9043 V 90.576172 l -1.6113,-1.464844 -6.6895,-0.390625 v -5.810547 h 33.5938 v 5.810547 l -5.9082,0.488281 -1.5625,1.367188 v 25.146488 h 2.1972 l 22.168,-25.341801 -0.4883,-1.171875 -6.0058,-0.488281 v -5.810547 h 25 v 5.810547 l -7.5684,0.488281 -17.9199,20.410156 20.8984,33.98438 7.4219,0.58593 V 150 h -25.7324 v -3.90625 l -15.2832,-25 h -4.6875 v 20.80078 l 1.5625,1.9043 5.9082,0.39062 V 150 Z",
        3752.49, 82.9102, 71.8262, 67.0898,
    ),
}

# --------------------------------------------------------------------------
# Corner index block
# --------------------------------------------------------------------------

INDEX_X = 48.0
INDEX_RANK_CY = 64.0
INDEX_RANK_H = 54.0
INDEX_RANK_MAX_W = 62.0
INDEX_PIP_CY = 120.0
INDEX_PIP_H = 40.0

# --------------------------------------------------------------------------
# Centre pip layouts
# --------------------------------------------------------------------------

PIP_COLUMN_X: dict[str, float] = {"L": 128.0, "C": 200.0, "R": 272.0}
PIP_FIELD_TOP = 168.0
PIP_FIELD_BOTTOM = 392.0

_THIRD = 1.0 / 3.0
_SIXTH = 1.0 / 6.0

# rank key -> (pip height, ((column, position down the field 0..1), ...)).
# These are the traditional arrangements: pips below the midline are rotated
# 180 degrees, exactly as a real deck prints them.
NUMBER_LAYOUTS: dict[str, tuple[float, tuple[tuple[str, float], ...]]] = {
    "2": (78.0, (("C", 0.0), ("C", 1.0))),
    "3": (78.0, (("C", 0.0), ("C", 0.5), ("C", 1.0))),
    "4": (72.0, (("L", 0.0), ("R", 0.0), ("L", 1.0), ("R", 1.0))),
    "5": (72.0, (("L", 0.0), ("R", 0.0), ("C", 0.5), ("L", 1.0), ("R", 1.0))),
    "6": (70.0, (("L", 0.0), ("R", 0.0), ("L", 0.5), ("R", 0.5), ("L", 1.0), ("R", 1.0))),
    "7": (
        66.0,
        (
            ("L", 0.0), ("R", 0.0),
            ("C", 0.25),
            ("L", 0.5), ("R", 0.5),
            ("L", 1.0), ("R", 1.0),
        ),
    ),
    "8": (
        66.0,
        (
            ("L", 0.0), ("R", 0.0),
            ("C", 0.25),
            ("L", 0.5), ("R", 0.5),
            ("C", 0.75),
            ("L", 1.0), ("R", 1.0),
        ),
    ),
    "9": (
        54.0,
        (
            ("L", 0.0), ("R", 0.0),
            ("L", _THIRD), ("R", _THIRD),
            ("C", 0.5),
            ("L", 1.0 - _THIRD), ("R", 1.0 - _THIRD),
            ("L", 1.0), ("R", 1.0),
        ),
    ),
    "t": (
        54.0,
        (
            ("L", 0.0), ("R", 0.0),
            ("C", _SIXTH),
            ("L", _THIRD), ("R", _THIRD),
            ("L", 1.0 - _THIRD), ("R", 1.0 - _THIRD),
            ("C", 1.0 - _SIXTH),
            ("L", 1.0), ("R", 1.0),
        ),
    ),
}

ACE_PIP_H = 192.0

# --------------------------------------------------------------------------
# Court cards (J / Q / K)
# --------------------------------------------------------------------------
#
# No portraiture: at the size these render, a drawn court figure turns to mud.
# A framed cartouche holding the rank letter between two pips stays readable and
# keeps the restrained cardroom register.

COURT_PANEL_X = 104.0
COURT_PANEL_Y = 136.0
COURT_PANEL_W = 192.0
COURT_PANEL_H = 288.0
COURT_PANEL_RADIUS = 18.0
COURT_PANEL_INSET = 9.0
COURT_PIP_H = 46.0
COURT_PIP_OFFSET = 52.0  # from the panel edge to the pip centre
COURT_RANK_H = 96.0
COURT_RANK_MAX_W = 130.0

COURT_RANKS = ("j", "q", "k")

# --------------------------------------------------------------------------
# Card back
# --------------------------------------------------------------------------

BACK_BORDER_INSET = 19.0
BACK_HAIRLINE_INSET = 27.0
BACK_LATTICE_INSET = 34.0
BACK_LATTICE_SPACING = 36.0
# Half the card diagonal, rounded up: how far the rotated lattice must reach to
# cover every corner once it is turned 45 degrees.
BACK_LATTICE_REACH = 360.0

# --------------------------------------------------------------------------
# SVG assembly
# --------------------------------------------------------------------------

SVG_OPEN = (
    '<svg xmlns="http://www.w3.org/2000/svg" '
    f'width="{fmt(CARD_W)}" height="{fmt(CARD_H)}" '
    f'viewBox="0 0 {fmt(CARD_W)} {fmt(CARD_H)}">'
)


def rounded_rect(inset: float, radius: float, extra: str) -> str:
    return (
        f'<rect x="{fmt(inset)}" y="{fmt(inset)}" '
        f'width="{fmt(CARD_W - 2 * inset)}" height="{fmt(CARD_H - 2 * inset)}" '
        f'rx="{fmt(radius)}" {extra}/>'
    )


def index_block(suit: Suit, rank_key: str) -> str:
    """The rank-over-pip block that sits in the top-left corner."""
    glyph = RANK_GLYPHS[rank_key]
    return (
        "<g>"
        + place(glyph, INDEX_X, INDEX_RANK_CY, INDEX_RANK_H, INDEX_RANK_MAX_W)
        + place(suit.pip, INDEX_X, INDEX_PIP_CY, INDEX_PIP_H * suit.pip_scale)
        + "</g>"
    )


def number_centre(suit: Suit, rank_key: str) -> list[str]:
    height, positions = NUMBER_LAYOUTS[rank_key]
    span = PIP_FIELD_BOTTOM - PIP_FIELD_TOP
    out: list[str] = []
    for column, position in positions:
        cy = PIP_FIELD_TOP + span * position
        out.append(
            place(
                suit.pip,
                PIP_COLUMN_X[column],
                cy,
                height * suit.pip_scale,
                upside_down=position > 0.5,
            )
        )
    return out


def court_centre(suit: Suit, rank_key: str) -> list[str]:
    # The cartouche is ruled in warm tan rather than the suit colour: a tinted
    # panel behind a red rank reads as pink stationery, and a grey one behind a
    # black rank reads as a UI box. Tan reads as printed card stock.
    inner = COURT_PANEL_INSET
    panel = (
        f'<rect x="{fmt(COURT_PANEL_X)}" y="{fmt(COURT_PANEL_Y)}" '
        f'width="{fmt(COURT_PANEL_W)}" height="{fmt(COURT_PANEL_H)}" '
        f'rx="{fmt(COURT_PANEL_RADIUS)}" fill="none" '
        f'stroke="{FACE_RULE}" stroke-width="2.5"/>'
    )
    panel_inner = (
        f'<rect x="{fmt(COURT_PANEL_X + inner)}" y="{fmt(COURT_PANEL_Y + inner)}" '
        f'width="{fmt(COURT_PANEL_W - 2 * inner)}" height="{fmt(COURT_PANEL_H - 2 * inner)}" '
        f'rx="{fmt(COURT_PANEL_RADIUS - inner + 2)}" fill="none" '
        f'stroke="{FACE_RULE}" stroke-opacity="0.6" stroke-width="1.2"/>'
    )
    top_pip_cy = COURT_PANEL_Y + COURT_PIP_OFFSET
    bottom_pip_cy = COURT_PANEL_Y + COURT_PANEL_H - COURT_PIP_OFFSET
    return [
        panel,
        panel_inner,
        place(suit.pip, CARD_CX, top_pip_cy, COURT_PIP_H * suit.pip_scale),
        place(RANK_GLYPHS[rank_key], CARD_CX, CARD_CY, COURT_RANK_H, COURT_RANK_MAX_W),
        place(
            suit.pip,
            CARD_CX,
            bottom_pip_cy,
            COURT_PIP_H * suit.pip_scale,
            upside_down=True,
        ),
    ]


def centre_art(suit: Suit, rank_key: str) -> list[str]:
    if rank_key == "a":
        return [place(suit.pip, CARD_CX, CARD_CY, ACE_PIP_H * suit.pip_scale)]
    if rank_key in COURT_RANKS:
        return court_centre(suit, rank_key)
    return number_centre(suit, rank_key)


def build_face(suit: Suit, rank_key: str) -> str:
    index = index_block(suit, rank_key)
    body = [
        SVG_OPEN,
        "  " + rounded_rect(
            FACE_INSET,
            FACE_RADIUS,
            f'fill="{FACE_CREAM}" stroke="{FACE_EDGE}" stroke-width="2"',
        ),
        "  " + rounded_rect(
            12.0,
            FACE_RADIUS - 9.0,
            f'fill="none" stroke="{FACE_KEYLINE}" stroke-width="1.5"',
        ),
        f'  <g fill="{suit.color}">',
    ]
    body += ["    " + line for line in centre_art(suit, rank_key)]
    body.append("    " + index)
    # The opposite corner is the same block turned through the card centre, so
    # the two indices can never drift apart.
    body.append(
        f'    <g transform="rotate(180 {fmt(CARD_CX)} {fmt(CARD_CY)})">{index}</g>'
    )
    body.append("  </g>")
    body.append("</svg>")
    return "\n".join(body) + "\n"


def build_back() -> str:
    lattice: list[str] = []
    steps = int(BACK_LATTICE_REACH / BACK_LATTICE_SPACING)
    offsets = [k * BACK_LATTICE_SPACING for k in range(-steps, steps + 1)]

    for offset in offsets:
        x = CARD_CX + offset
        lattice.append(
            f'<line x1="{fmt(x)}" y1="{fmt(CARD_CY - BACK_LATTICE_REACH)}" '
            f'x2="{fmt(x)}" y2="{fmt(CARD_CY + BACK_LATTICE_REACH)}"/>'
        )
    for offset in offsets:
        y = CARD_CY + offset
        lattice.append(
            f'<line x1="{fmt(CARD_CX - BACK_LATTICE_REACH)}" y1="{fmt(y)}" '
            f'x2="{fmt(CARD_CX + BACK_LATTICE_REACH)}" y2="{fmt(y)}"/>'
        )

    # Half-offset secondary grid: the finer brass thread that makes the pattern
    # read as engine-turning rather than graph paper.
    fine: list[str] = []
    half = BACK_LATTICE_SPACING / 2.0
    for offset in offsets:
        x = CARD_CX + offset + half
        fine.append(
            f'<line x1="{fmt(x)}" y1="{fmt(CARD_CY - BACK_LATTICE_REACH)}" '
            f'x2="{fmt(x)}" y2="{fmt(CARD_CY + BACK_LATTICE_REACH)}"/>'
        )
        y = CARD_CY + offset + half
        fine.append(
            f'<line x1="{fmt(CARD_CX - BACK_LATTICE_REACH)}" y1="{fmt(y)}" '
            f'x2="{fmt(CARD_CX + BACK_LATTICE_REACH)}" y2="{fmt(y)}"/>'
        )

    # A brass point in the middle of every other cell. Inside the rotated group
    # an axis-aligned square draws as a lozenge.
    pips: list[str] = []
    side = 5.0
    visible = BACK_LATTICE_REACH * 0.82  # beyond this the clip hides everything
    for i, ox in enumerate(offsets):
        for j, oy in enumerate(offsets):
            if (i + j) % 2:
                continue
            cx = CARD_CX + ox + half
            cy = CARD_CY + oy + half
            if abs(cx - CARD_CX) > visible or abs(cy - CARD_CY) > visible:
                continue
            pips.append(
                f'<rect x="{fmt(cx - side / 2)}" y="{fmt(cy - side / 2)}" '
                f'width="{fmt(side)}" height="{fmt(side)}"/>'
            )

    field_w = CARD_W - 2 * BACK_LATTICE_INSET
    field_h = CARD_H - 2 * BACK_LATTICE_INSET

    parts = [
        SVG_OPEN,
        "  <defs>",
        '    <clipPath id="back_lattice">',
        f'      <rect x="{fmt(BACK_LATTICE_INSET)}" y="{fmt(BACK_LATTICE_INSET)}" '
        f'width="{fmt(field_w)}" height="{fmt(field_h)}" rx="12"/>',
        "    </clipPath>",
        "  </defs>",
        "  " + rounded_rect(FACE_INSET, FACE_RADIUS, f'fill="{BACK_FIELD}"'),
        '  <g clip-path="url(#back_lattice)">',
        f'    <g transform="rotate(45 {fmt(CARD_CX)} {fmt(CARD_CY)})">',
        f'      <g stroke="{BACK_LATTICE_DARK}" stroke-width="2.2">',
        "        " + "".join(lattice),
        "      </g>",
        f'      <g stroke="{BACK_LATTICE_LIGHT}" stroke-width="0.9" stroke-opacity="0.45">',
        "        " + "".join(fine),
        "      </g>",
        f'      <g fill="{BACK_LATTICE_LIGHT}" fill-opacity="0.85">',
        "        " + "".join(pips),
        "      </g>",
        "    </g>",
        "  </g>",
        "  " + rounded_rect(
            BACK_BORDER_INSET,
            18.0,
            f'fill="none" stroke="{BACK_BORDER}" stroke-width="2.5"',
        ),
        "  " + rounded_rect(
            BACK_HAIRLINE_INSET,
            13.0,
            f'fill="none" stroke="{BACK_BORDER_HAIRLINE}" '
            'stroke-width="1" stroke-opacity="0.4"',
        ),
        "  " + rounded_rect(
            FACE_INSET,
            FACE_RADIUS,
            f'fill="none" stroke="{BACK_EDGE}" stroke-width="2"',
        ),
        "</svg>",
    ]
    return "\n".join(parts) + "\n"


# --------------------------------------------------------------------------
# Rank glyph extraction (maintenance path, not needed for a normal run)
# --------------------------------------------------------------------------

SVG_NS = "{http://www.w3.org/2000/svg}"


def dump_glyphs() -> str:
    """Re-derive RANK_GLYPHS from the installed typeface via Inkscape.

    Writes an atlas of <text> elements, has Inkscape convert them to paths and
    measure each bounding box, and prints the Python literal to paste back into
    RANK_GLYPHS above. Requires Inkscape and the Charter family.
    """
    step = GLYPH_FONT_SIZE * 3.0
    baseline = GLYPH_FONT_SIZE * 1.5
    width = step * (len(RANKS) + 1)

    entries = []
    for i, (key, label) in enumerate(RANKS):
        tracking = GLYPH_TRACKING.get(key)
        spacing = f' letter-spacing="{fmt(tracking)}"' if tracking else ""
        entries.append(
            f'<text id="rank_{key}" x="{fmt(step * (i + 0.5))}" y="{fmt(baseline)}" '
            f'font-family="{GLYPH_FONT_FAMILY}" font-weight="{GLYPH_FONT_WEIGHT}" '
            f'font-size="{fmt(GLYPH_FONT_SIZE)}"{spacing}>{label}</text>'
        )

    atlas = (
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{fmt(width)}" '
        f'height="{fmt(baseline * 2)}" viewBox="0 0 {fmt(width)} {fmt(baseline * 2)}">'
        + "".join(entries)
        + "</svg>"
    )

    with tempfile.TemporaryDirectory() as tmp:
        src = Path(tmp) / "atlas.svg"
        baked = Path(tmp) / "atlas_baked.svg"
        src.write_text(atlas, encoding="utf-8")
        subprocess.run(
            [
                "inkscape", str(src),
                "--export-type=svg",
                "--export-text-to-path",
                f"--export-filename={baked}",
            ],
            check=True,
            capture_output=True,
        )

        paths = {
            el.get("id"): el.get("d")
            for el in ElementTree.parse(baked).getroot().iter(f"{SVG_NS}path")
        }
        query = subprocess.run(
            ["inkscape", str(baked), "--query-all"],
            check=True,
            capture_output=True,
            text=True,
        )

    boxes: dict[str, tuple[float, float, float, float]] = {}
    for line in query.stdout.splitlines():
        fields = line.split(",")
        if len(fields) == 5 and fields[0].startswith("rank_"):
            boxes[fields[0]] = tuple(float(v) for v in fields[1:])  # type: ignore[assignment]

    lines = ["RANK_GLYPHS: dict[str, Shape] = {"]
    for key, label in RANKS:
        element_id = f"rank_{key}"
        if element_id not in paths or element_id not in boxes:
            raise RuntimeError(f"Inkscape did not produce a baked glyph for {label!r}")
        x, y, w, h = boxes[element_id]
        lines.append(f'    # {label}')
        lines.append(f'    "{key}": Shape(')
        lines.append(f'        "{paths[element_id]}",')
        lines.append(f"        {x}, {y}, {w}, {h},")
        lines.append("    ),")
    lines.append("}")
    return "\n".join(lines)


# --------------------------------------------------------------------------
# Entry point
# --------------------------------------------------------------------------


def write_all(out_dir: Path) -> list[Path]:
    out_dir.mkdir(parents=True, exist_ok=True)
    written: list[Path] = []
    for suit in SUITS:
        for rank_key, _label in RANKS:
            path = out_dir / f"{suit.key}_{rank_key}.svg"
            path.write_text(build_face(suit, rank_key), encoding="utf-8")
            written.append(path)
    back = out_dir / "back.svg"
    back.write_text(build_back(), encoding="utf-8")
    written.append(back)
    return written


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=DEFAULT_OUT_DIR,
        help="directory to write the SVGs into (default: assets/svg/tier2/cards)",
    )
    parser.add_argument(
        "--dump-glyphs",
        action="store_true",
        help="print a regenerated RANK_GLYPHS table and exit (needs Inkscape)",
    )
    args = parser.parse_args(argv)

    if args.dump_glyphs:
        print(dump_glyphs())
        return 0

    if not RANK_GLYPHS:
        print(
            "RANK_GLYPHS is empty; run --dump-glyphs and paste the result in.",
            file=sys.stderr,
        )
        return 1

    written = write_all(args.out_dir)
    print(f"wrote {len(written)} SVGs to {args.out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
