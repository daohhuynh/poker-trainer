#!/usr/bin/env python3
"""Generate the chip / dealer-button / all-in-marker source SVGs.

All ten discs come out of one parametric disc builder so the family reads as a
single set of physical objects on the felt:

    assets/svg/tier2/chips/chip_{white,red,green,black,purple,yellow,brown,gold}.svg
    assets/svg/tier1/dealer_button.svg
    assets/svg/tier2/side_pot_all_in_marker.svg

Chip body colours and denominations are FIXED (see DESIGN_SPEC / ARCHITECTURE):
re-skinning them would break the chip-counting drill's skill transfer. The
dealer button's ocean blue and sage green sit deliberately outside the play-chip
palette so the button never reads as a wagerable chip.

Denomination lettering is pre-baked to path data (see GLYPHS below) so the
committed SVGs contain no <text> and render identically on a machine without the
authoring font.

Usage:  python3 tools/gen_chips.py [repo_root]
"""

from __future__ import annotations

import math
import os
import sys

# --------------------------------------------------------------------------
# Baked lettering
# --------------------------------------------------------------------------
# Outlines of the eight denomination strings, baked from "Avenir Next Condensed
# Demi Bold" at font-size 200 with:
#   inkscape src.svg --export-type=svg --export-text-to-path --export-plain-svg
# Each entry carries the path data plus the exact bounding box Inkscape reported
# for it (--query-all), which is all the generator needs to scale and centre the
# lettering. Regenerating these requires the font; the emitted SVGs do not.

GLYPHS: dict[str, dict[str, object]] = {
    "$1": {
        "x": 49.0, "y": 142.8, "w": 160.6, "h": 173.4,
        "d": "M 94,316.2 H 83.8 v -16.4 q -9,-0.4 -18.6,-4.2 -9.4,-3.8 -16.2,-9.8 l 11.6,-16.6 q 5.2,5 11.4,8 6.4,3 11.8,3.2 v -42.6 q -6.4,-2.4 -12.4,-5.6 -5.8,-3.2 -10.4,-7.8 -4.4,-4.6 -7.2,-11 -2.6,-6.4 -2.6,-15.4 0,-9 2.6,-16 2.6,-7 7.2,-12 4.6,-5 10.6,-7.8 6,-2.8 13,-3.4 v -16 h 10.2 v 15.8 q 8.6,0.2 17,3.2 8.6,3 15.2,9.4 l -10.8,16.2 q -4,-4.2 -9.8,-7 -5.8,-2.8 -11.6,-2.8 v 41 q 6.6,2.8 12.8,6 6.2,3.2 10.8,8 4.8,4.6 7.6,11.2 3,6.6 3,16.2 0,9 -2.6,16.2 -2.6,7.2 -7.4,12.4 -4.6,5 -11,8 -6.4,3 -14,3.2 z M 84.2,177.8 q -4.8,0.6 -8.8,5.4 -4,4.6 -4,13 0,4.4 1,7.4 1,3 2.6,5.2 1.8,2.2 4,3.8 2.4,1.4 5.2,2.6 z m 24,83.2 q 0,-9 -4.2,-13.2 -4,-4.4 -9.6,-7 v 39.8 q 5.8,-0.6 9.8,-5.6 4,-5 4,-14 z m 78,39 V 186.2 l -23.6,17.8 -10.8,-18.6 36.4,-27 h 21.4 V 300 Z",
    },
    "$5": {
        "x": 49.0, "y": 442.8, "w": 180.2, "h": 173.4,
        "d": "M 94,616.2 H 83.8 v -16.4 q -9,-0.4 -18.6,-4.2 -9.4,-3.8 -16.2,-9.8 l 11.6,-16.6 q 5.2,5 11.4,8 6.4,3 11.8,3.2 v -42.6 q -6.4,-2.4 -12.4,-5.6 -5.8,-3.2 -10.4,-7.8 -4.4,-4.6 -7.2,-11 -2.6,-6.4 -2.6,-15.4 0,-9 2.6,-16 2.6,-7 7.2,-12 4.6,-5 10.6,-7.8 6,-2.8 13,-3.4 v -16 h 10.2 v 15.8 q 8.6,0.2 17,3.2 8.6,3 15.2,9.4 l -10.8,16.2 q -4,-4.2 -9.8,-7 -5.8,-2.8 -11.6,-2.8 v 41 q 6.6,2.8 12.8,6 6.2,3.2 10.8,8 4.8,4.6 7.6,11.2 3,6.6 3,16.2 0,9 -2.6,16.2 -2.6,7.2 -7.4,12.4 -4.6,5 -11,8 -6.4,3 -14,3.2 z M 84.2,477.8 q -4.8,0.6 -8.8,5.4 -4,4.6 -4,13 0,4.4 1,7.4 1,3 2.6,5.2 1.8,2.2 4,3.8 2.4,1.4 5.2,2.6 z m 24,83.2 q 0,-9 -4.2,-13.2 -4,-4.4 -9.6,-7 v 39.8 q 5.8,-0.6 9.8,-5.6 4,-5 4,-14 z m 121,-6.8 q 0,12 -3.2,21 -3.2,9 -8.8,15.2 -5.4,6.2 -13,9.4 -7.6,3 -16.4,3 -15,0 -25.6,-8 -10.4,-8 -14.4,-23.4 l 20.6,-6.8 q 2.4,7.6 7,12.4 4.8,4.6 11.8,4.6 8.8,0 13.6,-7.2 5,-7.4 5,-19.4 0,-13 -6.2,-20 -6.2,-7 -16.6,-7 -6,0 -13.6,1.8 -7.4,1.8 -12.6,4.6 l 2.4,-76 h 65 v 21 h -44.6 l -1.4,33 q 2.8,-1.4 6.4,-2 3.8,-0.8 6.6,-0.8 9.2,0 16.2,3.2 7.2,3.2 12,9.2 4.8,5.8 7.2,14 2.6,8.2 2.6,18.2 z",
    },
    "$25": {
        "x": 49.0, "y": 742.8, "w": 280.2, "h": 173.4,
        "d": "M 94,916.2 H 83.8 v -16.4 q -9,-0.4 -18.6,-4.2 -9.4,-3.8 -16.2,-9.8 l 11.6,-16.6 q 5.2,5 11.4,8 6.4,3 11.8,3.2 v -42.6 q -6.4,-2.4 -12.4,-5.6 -5.8,-3.2 -10.4,-7.8 -4.4,-4.6 -7.2,-11 -2.6,-6.4 -2.6,-15.4 0,-9 2.6,-16 2.6,-7 7.2,-12 4.6,-5 10.6,-7.8 6,-2.8 13,-3.4 v -16 h 10.2 v 15.8 q 8.6,0.2 17,3.2 8.6,3 15.2,9.4 l -10.8,16.2 q -4,-4.2 -9.8,-7 -5.8,-2.8 -11.6,-2.8 v 41 q 6.6,2.8 12.8,6 6.2,3.2 10.8,8 4.8,4.6 7.6,11.2 3,6.6 3,16.2 0,9 -2.6,16.2 -2.6,7.2 -7.4,12.4 -4.6,5 -11,8 -6.4,3 -14,3.2 z M 84.2,777.8 q -4.8,0.6 -8.8,5.4 -4,4.6 -4,13 0,4.4 1,7.4 1,3 2.6,5.2 1.8,2.2 4,3.8 2.4,1.4 5.2,2.6 z m 24,83.2 q 0,-9 -4.2,-13.2 -4,-4.4 -9.6,-7 v 39.8 q 5.8,-0.6 9.8,-5.6 4,-5 4,-14 z M 228,795.8 q 0,6.8 -1.8,12.8 -1.6,5.8 -4.6,11.4 -3,5.4 -7,10.8 -3.8,5.2 -8,10.8 L 177.4,879 h 48.8 v 21 h -76.4 v -21.8 l 41,-51.8 q 6,-8 9.6,-15.2 3.6,-7.4 3.6,-14.6 0,-8.4 -3.8,-14.2 -3.8,-5.8 -11.6,-5.8 -7,0 -11.6,5.2 -4.6,5.2 -6,15 l -22.6,-2.2 q 2.6,-19.4 13.6,-29.2 11,-9.8 27.6,-9.8 9.2,0 16.2,3 7.2,3 12,8.4 5,5.4 7.6,12.8 2.6,7.4 2.6,16 z m 101.2,58.4 q 0,12 -3.2,21 -3.2,9 -8.8,15.2 -5.4,6.2 -13,9.4 -7.6,3 -16.4,3 -15,0 -25.6,-8 -10.4,-8 -14.4,-23.4 l 20.6,-6.8 q 2.4,7.6 7,12.4 4.8,4.6 11.8,4.6 8.8,0 13.6,-7.2 5,-7.4 5,-19.4 0,-13 -6.2,-20 -6.2,-7 -16.6,-7 -6,0 -13.6,1.8 -7.4,1.8 -12.6,4.6 l 2.4,-76 h 65 v 21 h -44.6 l -1.4,33 q 2.8,-1.4 6.4,-2 3.8,-0.8 6.6,-0.8 9.2,0 16.2,3.2 7.2,3.2 12,9.2 4.8,5.8 7.2,14 2.6,8.2 2.6,18.2 z",
    },
    "$100": {
        "x": 49.0, "y": 1042.8, "w": 383.8, "h": 173.4,
        "d": "M 94,1216.2 H 83.8 v -16.4 q -9,-0.4 -18.6,-4.2 -9.4,-3.8 -16.2,-9.8 l 11.6,-16.6 q 5.2,5 11.4,8 6.4,3 11.8,3.2 v -42.6 q -6.4,-2.4 -12.4,-5.6 -5.8,-3.2 -10.4,-7.8 -4.4,-4.6 -7.2,-11 -2.6,-6.4 -2.6,-15.4 0,-9 2.6,-16 2.6,-7 7.2,-12 4.6,-5 10.6,-7.8 6,-2.8 13,-3.4 v -16 h 10.2 v 15.8 q 8.6,0.2 17,3.2 8.6,3 15.2,9.4 l -10.8,16.2 q -4,-4.2 -9.8,-7 -5.8,-2.8 -11.6,-2.8 v 41 q 6.6,2.8 12.8,6 6.2,3.2 10.8,8 4.8,4.6 7.6,11.2 3,6.6 3,16.2 0,9 -2.6,16.2 -2.6,7.2 -7.4,12.4 -4.6,5 -11,8 -6.4,3 -14,3.2 z m -9.8,-138.4 q -4.8,0.6 -8.8,5.4 -4,4.6 -4,13 0,4.4 1,7.4 1,3 2.6,5.2 1.8,2.2 4,3.8 2.4,1.4 5.2,2.6 z m 24,83.2 q 0,-9 -4.2,-13.2 -4,-4.4 -9.6,-7 v 39.8 q 5.8,-0.6 9.8,-5.6 4,-5 4,-14 z m 78,39 v -113.8 l -23.6,17.8 -10.8,-18.6 36.4,-27 h 21.4 V 1200 Z m 146.6,-70.8 q 0,12.2 -1.8,25.2 -1.6,13 -6.4,23.8 -4.6,10.8 -13,17.8 -8.4,6.8 -21.6,6.8 -13.2,0 -21.6,-6.8 -8.2,-7 -13,-17.8 -4.8,-10.8 -6.6,-23.8 -1.6,-13 -1.6,-25.2 0,-12.2 1.6,-25.2 1.8,-13 6.6,-23.8 4.8,-10.8 13,-17.6 8.4,-7 21.6,-7 13.2,0 21.6,7 8.4,6.8 13,17.6 4.8,10.8 6.4,23.8 1.8,13 1.8,25.2 z m -24.2,0 q 0,-7.2 -0.6,-16.2 -0.6,-9.2 -2.6,-17 -1.8,-8 -5.6,-13.4 -3.6,-5.6 -9.8,-5.6 -6.2,0 -10,5.6 -3.6,5.4 -5.6,13.4 -1.8,7.8 -2.4,17 -0.6,9 -0.6,16.2 0,7.2 0.6,16.4 0.6,9 2.4,17 2,8 5.6,13.4 3.8,5.4 10,5.4 6.2,0 9.8,-5.4 3.8,-5.4 5.6,-13.4 2,-8 2.6,-17 0.6,-9.2 0.6,-16.4 z m 124.2,0 q 0,12.2 -1.8,25.2 -1.6,13 -6.4,23.8 -4.6,10.8 -13,17.8 -8.4,6.8 -21.6,6.8 -13.2,0 -21.6,-6.8 -8.2,-7 -13,-17.8 -4.8,-10.8 -6.6,-23.8 -1.6,-13 -1.6,-25.2 0,-12.2 1.6,-25.2 1.8,-13 6.6,-23.8 4.8,-10.8 13,-17.6 8.4,-7 21.6,-7 13.2,0 21.6,7 8.4,6.8 13,17.6 4.8,10.8 6.4,23.8 1.8,13 1.8,25.2 z m -24.2,0 q 0,-7.2 -0.6,-16.2 -0.6,-9.2 -2.6,-17 -1.8,-8 -5.6,-13.4 -3.6,-5.6 -9.8,-5.6 -6.2,0 -10,5.6 -3.6,5.4 -5.6,13.4 -1.8,7.8 -2.4,17 -0.6,9 -0.6,16.2 0,7.2 0.6,16.4 0.6,9 2.4,17 2,8 5.6,13.4 3.8,5.4 10,5.4 6.2,0 9.8,-5.4 3.8,-5.4 5.6,-13.4 2,-8 2.6,-17 0.6,-9.2 0.6,-16.4 z",
    },
    "$500": {
        "x": 49.0, "y": 1342.8, "w": 383.8, "h": 173.4,
        "d": "M 94,1516.2 H 83.8 v -16.4 q -9,-0.4 -18.6,-4.2 -9.4,-3.8 -16.2,-9.8 l 11.6,-16.6 q 5.2,5 11.4,8 6.4,3 11.8,3.2 v -42.6 q -6.4,-2.4 -12.4,-5.6 -5.8,-3.2 -10.4,-7.8 -4.4,-4.6 -7.2,-11 -2.6,-6.4 -2.6,-15.4 0,-9 2.6,-16 2.6,-7 7.2,-12 4.6,-5 10.6,-7.8 6,-2.8 13,-3.4 v -16 h 10.2 v 15.8 q 8.6,0.2 17,3.2 8.6,3 15.2,9.4 l -10.8,16.2 q -4,-4.2 -9.8,-7 -5.8,-2.8 -11.6,-2.8 v 41 q 6.6,2.8 12.8,6 6.2,3.2 10.8,8 4.8,4.6 7.6,11.2 3,6.6 3,16.2 0,9 -2.6,16.2 -2.6,7.2 -7.4,12.4 -4.6,5 -11,8 -6.4,3 -14,3.2 z m -9.8,-138.4 q -4.8,0.6 -8.8,5.4 -4,4.6 -4,13 0,4.4 1,7.4 1,3 2.6,5.2 1.8,2.2 4,3.8 2.4,1.4 5.2,2.6 z m 24,83.2 q 0,-9 -4.2,-13.2 -4,-4.4 -9.6,-7 v 39.8 q 5.8,-0.6 9.8,-5.6 4,-5 4,-14 z m 121,-6.8 q 0,12 -3.2,21 -3.2,9 -8.8,15.2 -5.4,6.2 -13,9.4 -7.6,3 -16.4,3 -15,0 -25.6,-8 -10.4,-8 -14.4,-23.4 l 20.6,-6.8 q 2.4,7.6 7,12.4 4.8,4.6 11.8,4.6 8.8,0 13.6,-7.2 5,-7.4 5,-19.4 0,-13 -6.2,-20 -6.2,-7 -16.6,-7 -6,0 -13.6,1.8 -7.4,1.8 -12.6,4.6 l 2.4,-76 h 65 v 21 h -44.6 l -1.4,33 q 2.8,-1.4 6.4,-2 3.8,-0.8 6.6,-0.8 9.2,0 16.2,3.2 7.2,3.2 12,9.2 4.8,5.8 7.2,14 2.6,8.2 2.6,18.2 z m 103.6,-25 q 0,12.2 -1.8,25.2 -1.6,13 -6.4,23.8 -4.6,10.8 -13,17.8 -8.4,6.8 -21.6,6.8 -13.2,0 -21.6,-6.8 -8.2,-7 -13,-17.8 -4.8,-10.8 -6.6,-23.8 -1.6,-13 -1.6,-25.2 0,-12.2 1.6,-25.2 1.8,-13 6.6,-23.8 4.8,-10.8 13,-17.6 8.4,-7 21.6,-7 13.2,0 21.6,7 8.4,6.8 13,17.6 4.8,10.8 6.4,23.8 1.8,13 1.8,25.2 z m -24.2,0 q 0,-7.2 -0.6,-16.2 -0.6,-9.2 -2.6,-17 -1.8,-8 -5.6,-13.4 -3.6,-5.6 -9.8,-5.6 -6.2,0 -10,5.6 -3.6,5.4 -5.6,13.4 -1.8,7.8 -2.4,17 -0.6,9 -0.6,16.2 0,7.2 0.6,16.4 0.6,9 2.4,17 2,8 5.6,13.4 3.8,5.4 10,5.4 6.2,0 9.8,-5.4 3.8,-5.4 5.6,-13.4 2,-8 2.6,-17 0.6,-9.2 0.6,-16.4 z m 124.2,0 q 0,12.2 -1.8,25.2 -1.6,13 -6.4,23.8 -4.6,10.8 -13,17.8 -8.4,6.8 -21.6,6.8 -13.2,0 -21.6,-6.8 -8.2,-7 -13,-17.8 -4.8,-10.8 -6.6,-23.8 -1.6,-13 -1.6,-25.2 0,-12.2 1.6,-25.2 1.8,-13 6.6,-23.8 4.8,-10.8 13,-17.6 8.4,-7 21.6,-7 13.2,0 21.6,7 8.4,6.8 13,17.6 4.8,10.8 6.4,23.8 1.8,13 1.8,25.2 z m -24.2,0 q 0,-7.2 -0.6,-16.2 -0.6,-9.2 -2.6,-17 -1.8,-8 -5.6,-13.4 -3.6,-5.6 -9.8,-5.6 -6.2,0 -10,5.6 -3.6,5.4 -5.6,13.4 -1.8,7.8 -2.4,17 -0.6,9 -0.6,16.2 0,7.2 0.6,16.4 0.6,9 2.4,17 2,8 5.6,13.4 3.8,5.4 10,5.4 6.2,0 9.8,-5.4 3.8,-5.4 5.6,-13.4 2,-8 2.6,-17 0.6,-9.2 0.6,-16.4 z",
    },
    "$1000": {
        "x": 49.0, "y": 1642.8, "w": 483.8, "h": 173.4,
        "d": "M 94,1816.2 H 83.8 v -16.4 q -9,-0.4 -18.6,-4.2 -9.4,-3.8 -16.2,-9.8 l 11.6,-16.6 q 5.2,5 11.4,8 6.4,3 11.8,3.2 v -42.6 q -6.4,-2.4 -12.4,-5.6 -5.8,-3.2 -10.4,-7.8 -4.4,-4.6 -7.2,-11 -2.6,-6.4 -2.6,-15.4 0,-9 2.6,-16 2.6,-7 7.2,-12 4.6,-5 10.6,-7.8 6,-2.8 13,-3.4 v -16 h 10.2 v 15.8 q 8.6,0.2 17,3.2 8.6,3 15.2,9.4 l -10.8,16.2 q -4,-4.2 -9.8,-7 -5.8,-2.8 -11.6,-2.8 v 41 q 6.6,2.8 12.8,6 6.2,3.2 10.8,8 4.8,4.6 7.6,11.2 3,6.6 3,16.2 0,9 -2.6,16.2 -2.6,7.2 -7.4,12.4 -4.6,5 -11,8 -6.4,3 -14,3.2 z m -9.8,-138.4 q -4.8,0.6 -8.8,5.4 -4,4.6 -4,13 0,4.4 1,7.4 1,3 2.6,5.2 1.8,2.2 4,3.8 2.4,1.4 5.2,2.6 z m 24,83.2 q 0,-9 -4.2,-13.2 -4,-4.4 -9.6,-7 v 39.8 q 5.8,-0.6 9.8,-5.6 4,-5 4,-14 z m 78,39 v -113.8 l -23.6,17.8 -10.8,-18.6 36.4,-27 h 21.4 V 1800 Z m 146.6,-70.8 q 0,12.2 -1.8,25.2 -1.6,13 -6.4,23.8 -4.6,10.8 -13,17.8 -8.4,6.8 -21.6,6.8 -13.2,0 -21.6,-6.8 -8.2,-7 -13,-17.8 -4.8,-10.8 -6.6,-23.8 -1.6,-13 -1.6,-25.2 0,-12.2 1.6,-25.2 1.8,-13 6.6,-23.8 4.8,-10.8 13,-17.6 8.4,-7 21.6,-7 13.2,0 21.6,7 8.4,6.8 13,17.6 4.8,10.8 6.4,23.8 1.8,13 1.8,25.2 z m -24.2,0 q 0,-7.2 -0.6,-16.2 -0.6,-9.2 -2.6,-17 -1.8,-8 -5.6,-13.4 -3.6,-5.6 -9.8,-5.6 -6.2,0 -10,5.6 -3.6,5.4 -5.6,13.4 -1.8,7.8 -2.4,17 -0.6,9 -0.6,16.2 0,7.2 0.6,16.4 0.6,9 2.4,17 2,8 5.6,13.4 3.8,5.4 10,5.4 6.2,0 9.8,-5.4 3.8,-5.4 5.6,-13.4 2,-8 2.6,-17 0.6,-9.2 0.6,-16.4 z m 124.2,0 q 0,12.2 -1.8,25.2 -1.6,13 -6.4,23.8 -4.6,10.8 -13,17.8 -8.4,6.8 -21.6,6.8 -13.2,0 -21.6,-6.8 -8.2,-7 -13,-17.8 -4.8,-10.8 -6.6,-23.8 -1.6,-13 -1.6,-25.2 0,-12.2 1.6,-25.2 1.8,-13 6.6,-23.8 4.8,-10.8 13,-17.6 8.4,-7 21.6,-7 13.2,0 21.6,7 8.4,6.8 13,17.6 4.8,10.8 6.4,23.8 1.8,13 1.8,25.2 z m -24.2,0 q 0,-7.2 -0.6,-16.2 -0.6,-9.2 -2.6,-17 -1.8,-8 -5.6,-13.4 -3.6,-5.6 -9.8,-5.6 -6.2,0 -10,5.6 -3.6,5.4 -5.6,13.4 -1.8,7.8 -2.4,17 -0.6,9 -0.6,16.2 0,7.2 0.6,16.4 0.6,9 2.4,17 2,8 5.6,13.4 3.8,5.4 10,5.4 6.2,0 9.8,-5.4 3.8,-5.4 5.6,-13.4 2,-8 2.6,-17 0.6,-9.2 0.6,-16.4 z m 124.2,0 q 0,12.2 -1.8,25.2 -1.6,13 -6.4,23.8 -4.6,10.8 -13,17.8 -8.4,6.8 -21.6,6.8 -13.2,0 -21.6,-6.8 -8.2,-7 -13,-17.8 -4.8,-10.8 -6.6,-23.8 -1.6,-13 -1.6,-25.2 0,-12.2 1.6,-25.2 1.8,-13 6.6,-23.8 4.8,-10.8 13,-17.6 8.4,-7 21.6,-7 13.2,0 21.6,7 8.4,6.8 13,17.6 4.8,10.8 6.4,23.8 1.8,13 1.8,25.2 z m -24.2,0 q 0,-7.2 -0.6,-16.2 -0.6,-9.2 -2.6,-17 -1.8,-8 -5.6,-13.4 -3.6,-5.6 -9.8,-5.6 -6.2,0 -10,5.6 -3.6,5.4 -5.6,13.4 -1.8,7.8 -2.4,17 -0.6,9 -0.6,16.2 0,7.2 0.6,16.4 0.6,9 2.4,17 2,8 5.6,13.4 3.8,5.4 10,5.4 6.2,0 9.8,-5.4 3.8,-5.4 5.6,-13.4 2,-8 2.6,-17 0.6,-9.2 0.6,-16.4 z",
    },
    "$5000": {
        "x": 49.0, "y": 1942.8, "w": 483.8, "h": 173.4,
        "d": "M 94,2116.2 H 83.8 v -16.4 q -9,-0.4 -18.6,-4.2 -9.4,-3.8 -16.2,-9.8 l 11.6,-16.6 q 5.2,5 11.4,8 6.4,3 11.8,3.2 v -42.6 q -6.4,-2.4 -12.4,-5.6 -5.8,-3.2 -10.4,-7.8 -4.4,-4.6 -7.2,-11 -2.6,-6.4 -2.6,-15.4 0,-9 2.6,-16 2.6,-7 7.2,-12 4.6,-5 10.6,-7.8 6,-2.8 13,-3.4 v -16 h 10.2 v 15.8 q 8.6,0.2 17,3.2 8.6,3 15.2,9.4 l -10.8,16.2 q -4,-4.2 -9.8,-7 -5.8,-2.8 -11.6,-2.8 v 41 q 6.6,2.8 12.8,6 6.2,3.2 10.8,8 4.8,4.6 7.6,11.2 3,6.6 3,16.2 0,9 -2.6,16.2 -2.6,7.2 -7.4,12.4 -4.6,5 -11,8 -6.4,3 -14,3.2 z m -9.8,-138.4 q -4.8,0.6 -8.8,5.4 -4,4.6 -4,13 0,4.4 1,7.4 1,3 2.6,5.2 1.8,2.2 4,3.8 2.4,1.4 5.2,2.6 z m 24,83.2 q 0,-9 -4.2,-13.2 -4,-4.4 -9.6,-7 v 39.8 q 5.8,-0.6 9.8,-5.6 4,-5 4,-14 z m 121,-6.8 q 0,12 -3.2,21 -3.2,9 -8.8,15.2 -5.4,6.2 -13,9.4 -7.6,3 -16.4,3 -15,0 -25.6,-8 -10.4,-8 -14.4,-23.4 l 20.6,-6.8 q 2.4,7.6 7,12.4 4.8,4.6 11.8,4.6 8.8,0 13.6,-7.2 5,-7.4 5,-19.4 0,-13 -6.2,-20 -6.2,-7 -16.6,-7 -6,0 -13.6,1.8 -7.4,1.8 -12.6,4.6 l 2.4,-76 h 65 v 21 h -44.6 l -1.4,33 q 2.8,-1.4 6.4,-2 3.8,-0.8 6.6,-0.8 9.2,0 16.2,3.2 7.2,3.2 12,9.2 4.8,5.8 7.2,14 2.6,8.2 2.6,18.2 z m 103.6,-25 q 0,12.2 -1.8,25.2 -1.6,13 -6.4,23.8 -4.6,10.8 -13,17.8 -8.4,6.8 -21.6,6.8 -13.2,0 -21.6,-6.8 -8.2,-7 -13,-17.8 -4.8,-10.8 -6.6,-23.8 -1.6,-13 -1.6,-25.2 0,-12.2 1.6,-25.2 1.8,-13 6.6,-23.8 4.8,-10.8 13,-17.6 8.4,-7 21.6,-7 13.2,0 21.6,7 8.4,6.8 13,17.6 4.8,10.8 6.4,23.8 1.8,13 1.8,25.2 z m -24.2,0 q 0,-7.2 -0.6,-16.2 -0.6,-9.2 -2.6,-17 -1.8,-8 -5.6,-13.4 -3.6,-5.6 -9.8,-5.6 -6.2,0 -10,5.6 -3.6,5.4 -5.6,13.4 -1.8,7.8 -2.4,17 -0.6,9 -0.6,16.2 0,7.2 0.6,16.4 0.6,9 2.4,17 2,8 5.6,13.4 3.8,5.4 10,5.4 6.2,0 9.8,-5.4 3.8,-5.4 5.6,-13.4 2,-8 2.6,-17 0.6,-9.2 0.6,-16.4 z m 124.2,0 q 0,12.2 -1.8,25.2 -1.6,13 -6.4,23.8 -4.6,10.8 -13,17.8 -8.4,6.8 -21.6,6.8 -13.2,0 -21.6,-6.8 -8.2,-7 -13,-17.8 -4.8,-10.8 -6.6,-23.8 -1.6,-13 -1.6,-25.2 0,-12.2 1.6,-25.2 1.8,-13 6.6,-23.8 4.8,-10.8 13,-17.6 8.4,-7 21.6,-7 13.2,0 21.6,7 8.4,6.8 13,17.6 4.8,10.8 6.4,23.8 1.8,13 1.8,25.2 z m -24.2,0 q 0,-7.2 -0.6,-16.2 -0.6,-9.2 -2.6,-17 -1.8,-8 -5.6,-13.4 -3.6,-5.6 -9.8,-5.6 -6.2,0 -10,5.6 -3.6,5.4 -5.6,13.4 -1.8,7.8 -2.4,17 -0.6,9 -0.6,16.2 0,7.2 0.6,16.4 0.6,9 2.4,17 2,8 5.6,13.4 3.8,5.4 10,5.4 6.2,0 9.8,-5.4 3.8,-5.4 5.6,-13.4 2,-8 2.6,-17 0.6,-9.2 0.6,-16.4 z m 124.2,0 q 0,12.2 -1.8,25.2 -1.6,13 -6.4,23.8 -4.6,10.8 -13,17.8 -8.4,6.8 -21.6,6.8 -13.2,0 -21.6,-6.8 -8.2,-7 -13,-17.8 -4.8,-10.8 -6.6,-23.8 -1.6,-13 -1.6,-25.2 0,-12.2 1.6,-25.2 1.8,-13 6.6,-23.8 4.8,-10.8 13,-17.6 8.4,-7 21.6,-7 13.2,0 21.6,7 8.4,6.8 13,17.6 4.8,10.8 6.4,23.8 1.8,13 1.8,25.2 z m -24.2,0 q 0,-7.2 -0.6,-16.2 -0.6,-9.2 -2.6,-17 -1.8,-8 -5.6,-13.4 -3.6,-5.6 -9.8,-5.6 -6.2,0 -10,5.6 -3.6,5.4 -5.6,13.4 -1.8,7.8 -2.4,17 -0.6,9 -0.6,16.2 0,7.2 0.6,16.4 0.6,9 2.4,17 2,8 5.6,13.4 3.8,5.4 10,5.4 6.2,0 9.8,-5.4 3.8,-5.4 5.6,-13.4 2,-8 2.6,-17 0.6,-9.2 0.6,-16.4 z",
    },
    "$25000": {
        "x": 49.0, "y": 2242.8, "w": 583.8, "h": 173.4,
        "d": "M 94,2416.2 H 83.8 v -16.4 q -9,-0.4 -18.6,-4.2 -9.4,-3.8 -16.2,-9.8 l 11.6,-16.6 q 5.2,5 11.4,8 6.4,3 11.8,3.2 v -42.6 q -6.4,-2.4 -12.4,-5.6 -5.8,-3.2 -10.4,-7.8 -4.4,-4.6 -7.2,-11 -2.6,-6.4 -2.6,-15.4 0,-9 2.6,-16 2.6,-7 7.2,-12 4.6,-5 10.6,-7.8 6,-2.8 13,-3.4 v -16 h 10.2 v 15.8 q 8.6,0.2 17,3.2 8.6,3 15.2,9.4 l -10.8,16.2 q -4,-4.2 -9.8,-7 -5.8,-2.8 -11.6,-2.8 v 41 q 6.6,2.8 12.8,6 6.2,3.2 10.8,8 4.8,4.6 7.6,11.2 3,6.6 3,16.2 0,9 -2.6,16.2 -2.6,7.2 -7.4,12.4 -4.6,5 -11,8 -6.4,3 -14,3.2 z m -9.8,-138.4 q -4.8,0.6 -8.8,5.4 -4,4.6 -4,13 0,4.4 1,7.4 1,3 2.6,5.2 1.8,2.2 4,3.8 2.4,1.4 5.2,2.6 z m 24,83.2 q 0,-9 -4.2,-13.2 -4,-4.4 -9.6,-7 v 39.8 q 5.8,-0.6 9.8,-5.6 4,-5 4,-14 z M 228,2295.8 q 0,6.8 -1.8,12.8 -1.6,5.8 -4.6,11.4 -3,5.4 -7,10.8 -3.8,5.2 -8,10.8 l -29.2,37.4 h 48.8 v 21 h -76.4 v -21.8 l 41,-51.8 q 6,-8 9.6,-15.2 3.6,-7.4 3.6,-14.6 0,-8.4 -3.8,-14.2 -3.8,-5.8 -11.6,-5.8 -7,0 -11.6,5.2 -4.6,5.2 -6,15 l -22.6,-2.2 q 2.6,-19.4 13.6,-29.2 11,-9.8 27.6,-9.8 9.2,0 16.2,3 7.2,3 12,8.4 5,5.4 7.6,12.8 2.6,7.4 2.6,16 z m 101.2,58.4 q 0,12 -3.2,21 -3.2,9 -8.8,15.2 -5.4,6.2 -13,9.4 -7.6,3 -16.4,3 -15,0 -25.6,-8 -10.4,-8 -14.4,-23.4 l 20.6,-6.8 q 2.4,7.6 7,12.4 4.8,4.6 11.8,4.6 8.8,0 13.6,-7.2 5,-7.4 5,-19.4 0,-13 -6.2,-20 -6.2,-7 -16.6,-7 -6,0 -13.6,1.8 -7.4,1.8 -12.6,4.6 l 2.4,-76 h 65 v 21 h -44.6 l -1.4,33 q 2.8,-1.4 6.4,-2 3.8,-0.8 6.6,-0.8 9.2,0 16.2,3.2 7.2,3.2 12,9.2 4.8,5.8 7.2,14 2.6,8.2 2.6,18.2 z m 103.6,-25 q 0,12.2 -1.8,25.2 -1.6,13 -6.4,23.8 -4.6,10.8 -13,17.8 -8.4,6.8 -21.6,6.8 -13.2,0 -21.6,-6.8 -8.2,-7 -13,-17.8 -4.8,-10.8 -6.6,-23.8 -1.6,-13 -1.6,-25.2 0,-12.2 1.6,-25.2 1.8,-13 6.6,-23.8 4.8,-10.8 13,-17.6 8.4,-7 21.6,-7 13.2,0 21.6,7 8.4,6.8 13,17.6 4.8,10.8 6.4,23.8 1.8,13 1.8,25.2 z m -24.2,0 q 0,-7.2 -0.6,-16.2 -0.6,-9.2 -2.6,-17 -1.8,-8 -5.6,-13.4 -3.6,-5.6 -9.8,-5.6 -6.2,0 -10,5.6 -3.6,5.4 -5.6,13.4 -1.8,7.8 -2.4,17 -0.6,9 -0.6,16.2 0,7.2 0.6,16.4 0.6,9 2.4,17 2,8 5.6,13.4 3.8,5.4 10,5.4 6.2,0 9.8,-5.4 3.8,-5.4 5.6,-13.4 2,-8 2.6,-17 0.6,-9.2 0.6,-16.4 z m 124.2,0 q 0,12.2 -1.8,25.2 -1.6,13 -6.4,23.8 -4.6,10.8 -13,17.8 -8.4,6.8 -21.6,6.8 -13.2,0 -21.6,-6.8 -8.2,-7 -13,-17.8 -4.8,-10.8 -6.6,-23.8 -1.6,-13 -1.6,-25.2 0,-12.2 1.6,-25.2 1.8,-13 6.6,-23.8 4.8,-10.8 13,-17.6 8.4,-7 21.6,-7 13.2,0 21.6,7 8.4,6.8 13,17.6 4.8,10.8 6.4,23.8 1.8,13 1.8,25.2 z m -24.2,0 q 0,-7.2 -0.6,-16.2 -0.6,-9.2 -2.6,-17 -1.8,-8 -5.6,-13.4 -3.6,-5.6 -9.8,-5.6 -6.2,0 -10,5.6 -3.6,5.4 -5.6,13.4 -1.8,7.8 -2.4,17 -0.6,9 -0.6,16.2 0,7.2 0.6,16.4 0.6,9 2.4,17 2,8 5.6,13.4 3.8,5.4 10,5.4 6.2,0 9.8,-5.4 3.8,-5.4 5.6,-13.4 2,-8 2.6,-17 0.6,-9.2 0.6,-16.4 z m 124.2,0 q 0,12.2 -1.8,25.2 -1.6,13 -6.4,23.8 -4.6,10.8 -13,17.8 -8.4,6.8 -21.6,6.8 -13.2,0 -21.6,-6.8 -8.2,-7 -13,-17.8 -4.8,-10.8 -6.6,-23.8 -1.6,-13 -1.6,-25.2 0,-12.2 1.6,-25.2 1.8,-13 6.6,-23.8 4.8,-10.8 13,-17.6 8.4,-7 21.6,-7 13.2,0 21.6,7 8.4,6.8 13,17.6 4.8,10.8 6.4,23.8 1.8,13 1.8,25.2 z m -24.2,0 q 0,-7.2 -0.6,-16.2 -0.6,-9.2 -2.6,-17 -1.8,-8 -5.6,-13.4 -3.6,-5.6 -9.8,-5.6 -6.2,0 -10,5.6 -3.6,5.4 -5.6,13.4 -1.8,7.8 -2.4,17 -0.6,9 -0.6,16.2 0,7.2 0.6,16.4 0.6,9 2.4,17 2,8 5.6,13.4 3.8,5.4 10,5.4 6.2,0 9.8,-5.4 3.8,-5.4 5.6,-13.4 2,-8 2.6,-17 0.6,-9.2 0.6,-16.4 z",
    },
}

# --------------------------------------------------------------------------
# Palette
# --------------------------------------------------------------------------

CREAM = "#F4ECDC"          # warm highlight tone
SHADOW = "#120C0A"         # warm shadow tone
INK_DARK = "#1A1210"       # lettering on light chip bodies
INK_LIGHT = "#F7F2E8"      # lettering on dark chip bodies

OCEAN_BLUE = "#2E6E9E"     # dealer button body  (fixed)
SAGE_GREEN = "#7A9A65"     # dealer button M     (fixed)

BRASS_LIGHT = "#D9A441"
BRASS_MID = "#A87B4A"
BRASS_DARK = "#6E4E2C"
PLAQUE = "#2A1F1B"
MARKER_BAR = "#C0584A"
MARKER_BAR_DARK = "#8E3A2E"

# chip name -> (body colour, denomination string). Both columns are locked.
CHIPS: list[tuple[str, str, str]] = [
    ("white", "#F5F5F5", "$1"),
    ("red", "#C42A2A", "$5"),
    ("green", "#228B45", "$25"),
    ("black", "#1C1C1C", "$100"),
    ("purple", "#7E349E", "$500"),
    ("yellow", "#E0C42A", "$1000"),
    ("brown", "#784A26", "$5000"),
    ("gold", "#D4AF37", "$25000"),
]

# --------------------------------------------------------------------------
# Disc geometry (shared by all ten assets)
# --------------------------------------------------------------------------

SIZE = 512
C = SIZE / 2.0

R_OUTER = 248.0            # silhouette
R_BODY = 226.0             # inner edge of the darker/lighter rim band
R_INSERT_OUT = 246.0       # edge inserts cut across the rim band
R_INSERT_IN = 190.0
R_RING = 176.0             # inset ring inboard of the inserts
R_HAIRLINE = 162.0
R_INLAY = 154.0            # printed centre field

VALUE_MAX_W = 258.0
VALUE_MAX_H = 108.0

# --------------------------------------------------------------------------
# Small helpers
# --------------------------------------------------------------------------


def num(v: float) -> str:
    """Compact fixed-precision number for SVG attributes."""
    s = f"{v:.2f}".rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


def _rgb(hex_color: str) -> tuple[int, int, int]:
    h = hex_color.lstrip("#")
    return int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)


def mix(a: str, b: str, t: float) -> str:
    """Blend colour a toward colour b by t in [0, 1]."""
    ar, ag, ab = _rgb(a)
    br, bg, bb = _rgb(b)
    return "#{:02X}{:02X}{:02X}".format(
        round(ar + (br - ar) * t),
        round(ag + (bg - ag) * t),
        round(ab + (bb - ab) * t),
    )


def luminance(hex_color: str) -> float:
    r, g, b = (c / 255.0 for c in _rgb(hex_color))
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def polar(r: float, deg: float) -> tuple[float, float]:
    """Point on the disc. 0deg is right, +deg runs clockwise on screen."""
    a = math.radians(deg)
    return C + r * math.cos(a), C + r * math.sin(a)


def circle(r: float, **attrs: object) -> str:
    parts = " ".join(f'{k.replace("_", "-")}="{v}"' for k, v in attrs.items())
    return f'<circle cx="{num(C)}" cy="{num(C)}" r="{num(r)}" {parts}/>'


def arc(r: float, a0: float, a1: float, **attrs: object) -> str:
    """Open arc stroke from a0 to a1 (clockwise on screen)."""
    x0, y0 = polar(r, a0)
    x1, y1 = polar(r, a1)
    large = 1 if (a1 - a0) % 360.0 > 180.0 else 0
    d = f"M {num(x0)} {num(y0)} A {num(r)} {num(r)} 0 {large} 1 {num(x1)} {num(y1)}"
    parts = " ".join(f'{k.replace("_", "-")}="{v}"' for k, v in attrs.items())
    return f'<path d="{d}" fill="none" {parts}/>'


def sector(r_in: float, r_out: float, a0: float, a1: float, **attrs: object) -> str:
    """Annular sector — the classic clay-chip edge insert."""
    ox0, oy0 = polar(r_out, a0)
    ox1, oy1 = polar(r_out, a1)
    ix1, iy1 = polar(r_in, a1)
    ix0, iy0 = polar(r_in, a0)
    large = 1 if (a1 - a0) % 360.0 > 180.0 else 0
    d = (
        f"M {num(ox0)} {num(oy0)} "
        f"A {num(r_out)} {num(r_out)} 0 {large} 1 {num(ox1)} {num(oy1)} "
        f"L {num(ix1)} {num(iy1)} "
        f"A {num(r_in)} {num(r_in)} 0 {large} 0 {num(ix0)} {num(iy0)} Z"
    )
    parts = " ".join(f'{k.replace("_", "-")}="{v}"' for k, v in attrs.items())
    return f'<path d="{d}" {parts}/>'


def value_paths(text: str, ink: str) -> str:
    """Baked denomination lettering, scaled to fit and optically centred."""
    g = GLYPHS[text]
    bx, by = float(g["x"]), float(g["y"])
    bw, bh = float(g["w"]), float(g["h"])
    scale = min(VALUE_MAX_W / bw, VALUE_MAX_H / bh)
    tx = C - scale * (bx + bw / 2.0)
    ty = C - scale * (by + bh / 2.0)
    return (
        f'<g transform="translate({num(tx)} {num(ty)}) scale({scale:.5f})" fill="{ink}">'
        f'<path d="{g["d"]}"/></g>'
    )


def svg_document(body: str, title_comment: str) -> str:
    return (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        f"<!-- {title_comment} -->\n"
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{SIZE}" height="{SIZE}" '
        f'viewBox="0 0 {SIZE} {SIZE}">\n'
        + "\n".join("  " + line for line in body.strip().splitlines())
        + "\n</svg>\n"
    )


# --------------------------------------------------------------------------
# The parametric disc
# --------------------------------------------------------------------------


def disc_shell(body: str, rim_band: str, contour: str) -> list[str]:
    """Silhouette + rim band + contour line. Shared by every disc in the set."""
    return [
        circle(R_OUTER, fill=rim_band),
        circle(R_BODY, fill=body),
        circle(R_OUTER - 1.25, fill="none", stroke=contour,
               stroke_width="2.5", stroke_opacity="0.55"),
    ]


def disc_relief() -> list[str]:
    """Restrained top-light / bottom-shadow along the rim. No filters."""
    return [
        arc(R_OUTER - 5.0, -158.0, -22.0, stroke="#FFFFFF", stroke_opacity="0.13",
            stroke_width="7", stroke_linecap="round"),
        arc(R_OUTER - 5.0, 22.0, 158.0, stroke=SHADOW, stroke_opacity="0.18",
            stroke_width="7", stroke_linecap="round"),
    ]


def build_chip(body: str, denom: str) -> str:
    light_body = luminance(body) > 0.5

    rim_band = mix(body, SHADOW, 0.16) if light_body else mix(body, CREAM, 0.13)
    contour = mix(body, SHADOW, 0.32) if light_body else mix(body, CREAM, 0.30)
    ring_tone = mix(body, PLAQUE, 0.42) if light_body else mix(body, CREAM, 0.62)
    inlay = mix(body, SHADOW, 0.07) if light_body else mix(body, CREAM, 0.10)
    insert_fill = mix(body, CREAM, 0.74)
    insert_edge = mix(body, SHADOW, 0.50)
    ink = INK_DARK if light_body else INK_LIGHT

    out: list[str] = []
    out.append("<!-- body -->")
    out += disc_shell(body, rim_band, contour)

    out.append("<!-- eight clay edge inserts -->")
    half = 13.5
    for k in range(8):
        centre = -90.0 + k * 45.0
        out.append(
            sector(R_INSERT_IN, R_INSERT_OUT, centre - half, centre + half,
                   fill=insert_fill, stroke=insert_edge, stroke_width="3",
                   stroke_opacity="0.55", stroke_linejoin="round")
        )

    out.append("<!-- inset rings -->")
    out.append(circle(R_RING, fill="none", stroke=ring_tone, stroke_width="7",
                      stroke_opacity="0.92"))
    out.append(circle(R_HAIRLINE, fill="none", stroke=ring_tone, stroke_width="2.5",
                      stroke_opacity="0.55"))

    out.append("<!-- printed centre field -->")
    out.append(circle(R_INLAY, fill=inlay))

    out.append("<!-- denomination -->")
    out.append(value_paths(denom, ink))

    out += ["<!-- relief -->"] + disc_relief()
    return "\n".join(out)


# --------------------------------------------------------------------------
# Dealer button
# --------------------------------------------------------------------------

# Pip layout inside one rectangular insert, in insert-local coordinates.
DICE_FACES: dict[int, list[tuple[float, float]]] = {
    1: [(0, 0)],
    2: [(-18, -16), (18, 16)],
    3: [(-18, -16), (0, 0), (18, 16)],
    4: [(-18, -16), (18, -16), (-18, 16), (18, 16)],
    5: [(-18, -16), (18, -16), (0, 0), (-18, 16), (18, 16)],
    6: [(-18, -16), (18, -16), (-18, 0), (18, 0), (-18, 16), (18, 16)],
}

INSERT_W = 68.0
INSERT_H = 62.0
INSERT_R = 206.0           # radial centre of each insert


def build_dealer_button() -> str:
    body = OCEAN_BLUE
    rim_band = mix(body, CREAM, 0.13)
    contour = mix(body, CREAM, 0.30)
    insert_fill = "#F4EFE4"
    insert_edge = mix(body, SHADOW, 0.35)
    pip = mix(body, SHADOW, 0.42)

    out: list[str] = []
    out.append("<!-- ocean blue body -->")
    out += disc_shell(body, rim_band, contour)

    out.append("<!-- six white rectangular inserts, one per dice face -->")
    # Scattered rather than sequential so the ring does not read as a counter.
    for k, face in enumerate((1, 4, 2, 6, 3, 5)):
        angle = k * 60.0
        cx, cy = C, C - INSERT_R
        pips = "".join(
            f'<circle cx="{num(cx + dx)}" cy="{num(cy + dy)}" r="6.5" fill="{pip}"/>'
            for dx, dy in DICE_FACES[face]
        )
        out.append(
            f'<g transform="rotate({num(angle)} {num(C)} {num(C)})">'
            f'<rect x="{num(cx - INSERT_W / 2)}" y="{num(cy - INSERT_H / 2)}" '
            f'width="{num(INSERT_W)}" height="{num(INSERT_H)}" rx="11" '
            f'fill="{insert_fill}" stroke="{insert_edge}" stroke-width="2.5" '
            f'stroke-opacity="0.45"/>{pips}</g>'
        )

    out.append("<!-- inset dashed ring between the dice edge and the monogram -->")
    r_dash = 168.0
    period = (2.0 * math.pi * r_dash) / 24.0
    out.append(circle(r_dash, fill="none", stroke="#EDE3D3", stroke_opacity="0.78",
                      stroke_width="7", stroke_linecap="butt",
                      stroke_dasharray=f"{num(period * 0.55)} {num(period * 0.45)}"))

    out.append("<!-- sage green venue monogram -->")
    out.append(
        '<path d="M 180 335 L 180 177 L 256 282 L 332 177 L 332 335" '
        f'fill="none" stroke="{SAGE_GREEN}" stroke-width="34" '
        'stroke-linecap="butt" stroke-linejoin="miter" stroke-miterlimit="6"/>'
    )

    out += ["<!-- relief -->"] + disc_relief()
    return "\n".join(out)


# --------------------------------------------------------------------------
# Side-pot all-in marker
# --------------------------------------------------------------------------

BAR_ANGLE = -32.0
BAR_LEN = 508.0
BAR_H = 94.0

R_MARKER_OUT = 230.0
R_MARKER_IN = 150.0


def build_all_in_marker() -> str:
    """Brass ring barred by a diagonal banner: reads as 'this seat is closed'.

    The ring is left open in the middle so the felt shows through — that
    figure/ground break is what keeps the marker legible at 32px.
    """
    r_mid = (R_MARKER_OUT + R_MARKER_IN) / 2.0
    band = R_MARKER_OUT - R_MARKER_IN
    brass_body = mix(BRASS_MID, BRASS_LIGHT, 0.30)

    x = C - BAR_LEN / 2.0
    y = C - BAR_H / 2.0

    out: list[str] = []
    out.append("<!-- brass ring -->")
    out.append(circle(r_mid, fill="none", stroke=brass_body, stroke_width=num(band)))
    out.append("<!-- turned edges -->")
    out.append(circle(R_MARKER_OUT - 3.0, fill="none", stroke=BRASS_DARK,
                      stroke_width="6"))
    out.append(circle(R_MARKER_IN + 3.0, fill="none", stroke=BRASS_DARK,
                      stroke_width="6"))
    out.append("<!-- light from the upper left, shadow lower right -->")
    out.append(arc(R_MARKER_OUT - 15.0, -172.0, -32.0, stroke=BRASS_LIGHT,
                   stroke_width="16", stroke_linecap="round"))
    out.append(arc(R_MARKER_IN + 15.0, -168.0, -40.0, stroke=BRASS_DARK,
                   stroke_opacity="0.45", stroke_width="14", stroke_linecap="round"))
    out.append(arc(R_MARKER_OUT - 15.0, 14.0, 152.0, stroke=BRASS_DARK,
                   stroke_opacity="0.55", stroke_width="16", stroke_linecap="round"))
    out.append(arc(R_MARKER_IN + 15.0, 26.0, 150.0, stroke=BRASS_LIGHT,
                   stroke_opacity="0.5", stroke_width="12", stroke_linecap="round"))

    out.append("<!-- bold diagonal banner across the ring -->")
    rails = "".join(
        f'<rect x="{num(x + 24)}" y="{num(C + off - 2)}" '
        f'width="{num(BAR_LEN - 48)}" height="4" rx="2" '
        f'fill="{BRASS_LIGHT}" fill-opacity="0.8"/>'
        for off in (-27.0, 27.0)
    )
    caps = "".join(
        f'<rect x="{num(cx)}" y="{num(y + 8)}" width="14" '
        f'height="{num(BAR_H - 16)}" rx="6" fill="{BRASS_LIGHT}" fill-opacity="0.9"/>'
        for cx in (x + 30.0, x + BAR_LEN - 44.0)
    )
    out.append(
        f'<g transform="rotate({num(BAR_ANGLE)} {num(C)} {num(C)})">'
        f'<rect x="{num(x)}" y="{num(y)}" width="{num(BAR_LEN)}" height="{num(BAR_H)}" '
        f'rx="20" fill="{mix(MARKER_BAR_DARK, SHADOW, 0.55)}"/>'
        f'<rect x="{num(x + 6)}" y="{num(y + 6)}" width="{num(BAR_LEN - 12)}" '
        f'height="{num(BAR_H - 12)}" rx="15" fill="{MARKER_BAR_DARK}"/>'
        f'<rect x="{num(x + 11)}" y="{num(y + 11)}" width="{num(BAR_LEN - 22)}" '
        f'height="{num(BAR_H - 22)}" rx="11" fill="{MARKER_BAR}"/>'
        f'<rect x="{num(x + 26)}" y="{num(y + 16)}" width="{num(BAR_LEN - 52)}" '
        f'height="9" rx="4.5" fill="#FFFFFF" fill-opacity="0.14"/>'
        f'<rect x="{num(x + 26)}" y="{num(y + BAR_H - 26)}" '
        f'width="{num(BAR_LEN - 52)}" height="8" rx="4" '
        f'fill="{SHADOW}" fill-opacity="0.22"/>'
        f"{rails}{caps}</g>"
    )
    return "\n".join(out)


# --------------------------------------------------------------------------
# Entry point
# --------------------------------------------------------------------------


def main() -> int:
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..")
    root = os.path.abspath(root)
    chips_dir = os.path.join(root, "assets", "svg", "tier2", "chips")
    tier1_dir = os.path.join(root, "assets", "svg", "tier1")
    tier2_dir = os.path.join(root, "assets", "svg", "tier2")
    for d in (chips_dir, tier1_dir, tier2_dir):
        os.makedirs(d, exist_ok=True)

    written: list[str] = []

    for name, body, denom in CHIPS:
        path = os.path.join(chips_dir, f"chip_{name}.svg")
        doc = svg_document(build_chip(body, denom),
                           f"chip_{name} {denom} - generated by tools/gen_chips.py")
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(doc)
        written.append(path)

    path = os.path.join(tier1_dir, "dealer_button.svg")
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(svg_document(build_dealer_button(),
                              "dealer_button - generated by tools/gen_chips.py"))
    written.append(path)

    path = os.path.join(tier2_dir, "side_pot_all_in_marker.svg")
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(svg_document(build_all_in_marker(),
                              "side_pot_all_in_marker - generated by tools/gen_chips.py"))
    written.append(path)

    for p in written:
        print(os.path.relpath(p, root))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
