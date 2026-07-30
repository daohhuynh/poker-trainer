#!/usr/bin/env python3
"""Build a visual contact sheet of every shipped asset.

Renders all 87 PNGs plus a waveform strip for each of the 21 audio files into a
single reviewable image. Tiles sit on the app's own near-black warm background
(#1A1210) rather than white, because most of this art is cut-out with a
transparent canvas and much of it is warm off-white -- on a white sheet the icon
set would be invisible, which is exactly the failure a contact sheet exists to
catch.

Usage:
    python3 tools/contact_sheet.py [-o out.png]
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
IMAGES = REPO / "assets" / "images"
AUDIO = REPO / "assets" / "audio"

BG = "#1A1210"
INK = "#EDE3D3"
ACCENT = "#EFB42E"

# This ImageMagick build ships no configured font ("magick -list font" is empty),
# so every -annotate/-label needs an explicit font file or it fails with
# "unable to read font ''". Resolve one up front rather than per call.
FONT_CANDIDATES = [
    "/System/Library/Fonts/Helvetica.ttc",
    "/System/Library/Fonts/Supplemental/Arial.ttf",
    "/System/Library/Fonts/Geneva.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
]


def find_font() -> list[str]:
    for cand in FONT_CANDIDATES:
        if Path(cand).exists():
            return ["-font", cand]
    print("warning: no usable font found; tiles will render without labels",
          file=sys.stderr)
    return []


FONT: list[str] = []

# (section title, glob, tile size, columns)
SECTIONS = [
    ("BACKGROUNDS", ["tier1/background_root.png", "tier2/background_mode.png",
                     "tier2/background_game.png"], 380, 3),
    ("DEALER  -  butler x3, frog base + overlays", [
        "tier1/butler_neutral.png", "tier1/butler_raised.png",
        "tier2/butler_profile.png", "tier4/frog_base.png",
        "tier4/frog_expression_pass.png", "tier4/frog_expression_fail.png"], 230, 6),
    ("TABLE  -  felt, dealer button, all-in marker, logo", [
        "tier2/table_felt.png", "tier1/dealer_button.png",
        "tier2/side_pot_all_in_marker.png", "tier1/app_logo.png"], 280, 4),
    ("CHIPS", ["tier2/chips/*.png"], 150, 8),
    ("ICONS", ["tier1/icons/*.png", "tier2/icons/*.png"], 110, 13),
    ("CARDS", ["tier2/cards/*.png"], 96, 14),
]


def run(cmd: list[str]) -> None:
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError(
            f"command failed (exit {proc.returncode}):\n"
            f"  {' '.join(cmd[:6])} ... {cmd[-1]}\n"
            f"  stderr: {proc.stderr.strip()[:600]}"
        )


def expand(patterns: list[str]) -> list[Path]:
    out: list[Path] = []
    for pat in patterns:
        if "*" in pat:
            out.extend(sorted(IMAGES.glob(pat)))
        else:
            p = IMAGES / pat
            if p.exists():
                out.append(p)
    return out


def band(title: str, width: int, tmp: Path, idx: int) -> Path:
    """A section header strip."""
    out = tmp / f"hdr_{idx}.png"
    run(["magick", "-size", f"{width}x52", f"xc:{BG}", *FONT,
         "-fill", ACCENT, "-pointsize", "26", "-gravity", "west",
         "-annotate", "+18+0", title, str(out)])
    return out


def make_section(title: str, files: list[Path], tile: int, cols: int,
                 tmp: Path, idx: int) -> Path | None:
    if not files:
        return None
    grid = tmp / f"sec_{idx}.png"
    # ImageMagick settings apply to images read AFTER them, so -label/-font must
    # precede the filenames or the labels silently never attach.
    run([
        "magick", "montage",
        "-background", BG, *FONT,
        "-fill", INK, "-pointsize", "13",
        "-label", "%f",
        "-tile", f"{cols}x",
        "-geometry", f"{tile}x{tile}+7+7",
        *[str(f) for f in files],
        str(grid),
    ])
    width = int(subprocess.run(["magick", "identify", "-format", "%w", str(grid)],
                               check=True, capture_output=True, text=True).stdout)
    header = band(f"{title}   [{len(files)}]", width, tmp, idx)
    out = tmp / f"blk_{idx}.png"
    run(["magick", str(header), str(grid), "-background", BG, "-append", str(out)])
    return out


def audio_section(tmp: Path, idx: int) -> Path | None:
    files = sorted(AUDIO.rglob("*.ogg")) + sorted(AUDIO.rglob("*.mp3"))
    if not files:
        return None
    strips: list[Path] = []
    for i, f in enumerate(files):
        wave = tmp / f"wav_{i}.png"
        try:
            run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-i", str(f),
                 "-filter_complex",
                 f"showwavespic=s=460x84:colors={ACCENT}", "-frames:v", "1",
                 str(wave)])
        except subprocess.CalledProcessError:
            continue
        dur = subprocess.run(
            ["ffprobe", "-v", "error", "-show_entries", "format=duration",
             "-of", "default=nw=1:nk=1", str(f)],
            check=True, capture_output=True, text=True).stdout.strip()
        kb = f.stat().st_size / 1024
        rel = f.relative_to(AUDIO)
        labelled = tmp / f"wavl_{i}.png"
        run(["magick", str(wave), "-background", BG, "-flatten", *FONT,
             "-fill", INK, "-pointsize", "15", "-gravity", "northwest",
             "-annotate", "+8+4", f"{rel}",
             "-fill", "#A89A88", "-pointsize", "13", "-gravity", "southwest",
             "-annotate", "+8+4", f"{float(dur):.2f}s   {kb:.0f} KB",
             "-bordercolor", "#3A2C25", "-border", "1", str(labelled)])
        strips.append(labelled)
    if not strips:
        return None
    grid = tmp / f"sec_{idx}.png"
    run(["magick", "montage", "-background", BG, *FONT, "-label", "",
         "-tile", "3x", "-geometry", "+7+7",
         *[str(s) for s in strips], str(grid)])
    width = int(subprocess.run(["magick", "identify", "-format", "%w", str(grid)],
                               check=True, capture_output=True, text=True).stdout)
    header = band(f"AUDIO  -  waveforms   [{len(strips)}]", width, tmp, idx)
    out = tmp / f"blk_{idx}.png"
    run(["magick", str(header), str(grid), "-background", BG, "-append", str(out)])
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("-o", "--out", type=Path,
                    default=REPO / "docs" / "asset_contact_sheet.png")
    args = ap.parse_args()

    global FONT
    FONT = find_font()

    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        blocks: list[Path] = []
        for i, (title, pats, tile, cols) in enumerate(SECTIONS):
            files = expand(pats)
            print(f"{title}: {len(files)} file(s)")
            blk = make_section(title, files, tile, cols, tmp, i)
            if blk:
                blocks.append(blk)
        aud = audio_section(tmp, len(SECTIONS))
        if aud:
            blocks.append(aud)
        if not blocks:
            print("error: nothing to render", file=sys.stderr)
            return 1

        args.out.parent.mkdir(parents=True, exist_ok=True)
        # Left-align the stacked sections; widths differ per section.
        run(["magick", *[str(b) for b in blocks],
             "-background", BG, "-gravity", "west", "-append",
             "-bordercolor", BG, "-border", "24", str(args.out)])

    geom = subprocess.run(["magick", "identify", "-format", "%wx%h", str(args.out)],
                          check=True, capture_output=True, text=True).stdout
    try:
        shown = args.out.relative_to(REPO)
    except ValueError:
        shown = args.out
    print(f"\nwrote {shown}  {geom}  "
          f"{args.out.stat().st_size / 1024 / 1024:.1f} MB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
