# ASSETS.md

License ledger for every third-party asset shipped in the Poker Trainer, plus
the provenance of the generated art. Every fetched file has a row here.

Regenerate the art with `tools/rasterize_assets.py` and
`tools/derive_backgrounds.py`; neither touches this file, so keep it current by
hand when assets change.

## Summary

| Class | Count | License | Attribution |
|---|---|---|---|
| Sound effects | 9 | CC0 1.0 | not required |
| Music tracks | 12 | 10× CC BY 4.0, 2× public domain / CC0 | **REQUIRED** for 10 |
| Source photograph | 1 | Unsplash License | not required |
| Generated vector art | 84 | original work, project-owned | n/a |

---

## Sound effects — 9 files, all CC0 1.0

Sourced from [Freesound](https://freesound.org) via the CC0 search filter, with
each sound's license re-checked against the `/apiv2/sounds/<id>/` detail
endpoint rather than trusted from the filter alone. CC0 is a public-domain
dedication, so **no attribution is legally required** — the authors are credited
here anyway because they earned it.

Delivered as Ogg Vorbis 44.1 kHz, hard-trimmed to remove leading silence so each
effect fires on the frame it is triggered.

| Asset | Size | Original | Author | Source | License |
|---|---|---|---|---|---|
| `assets/audio/sfx/button_click_confirmation.ogg` | 6 KB | CLICK_177.wav | Jaszunio15 | [link](https://freesound.org/s/421294/) | CC0 1.0 |
| `assets/audio/sfx/card_deal.ogg` | 29 KB | deal_five_cards.mp3 | eggdeng | [link](https://freesound.org/s/502661/) | CC0 1.0 |
| `assets/audio/sfx/chip_push.ogg` | 10 KB | AllInPushChips.wav | Joma86 | [link](https://freesound.org/s/532861/) | CC0 1.0 |
| `assets/audio/sfx/frog_toggle.ogg` | 7 KB | Frog.ogg | egomassive | [link](https://freesound.org/s/536759/) | CC0 1.0 |
| `assets/audio/sfx/modal_swoosh_close.ogg` | 7 KB | Whoosh #1 (time-reversed for the close pair) | Kinoton | [link](https://freesound.org/s/427823/) | CC0 1.0 |
| `assets/audio/sfx/modal_swoosh_open.ogg` | 8 KB | Whoosh #1 | Kinoton | [link](https://freesound.org/s/427823/) | CC0 1.0 |
| `assets/audio/sfx/side_pot_split.ogg` | 31 KB | GAMEBoard_chips drop hit constant fast chain ceramic straight | sabrinaaiuijijiji | [link](https://freesound.org/s/828399/) | CC0 1.0 |
| `assets/audio/sfx/slide_in.ogg` | 9 KB | Whoosh stereo light (transition) | xkeril | [link](https://freesound.org/s/701104/) | CC0 1.0 |
| `assets/audio/sfx/slide_out.ogg` | 9 KB | Whoosh stereo light (transition) (time-reversed per spec) | xkeril | [link](https://freesound.org/s/701104/) | CC0 1.0 |

`slide_out.ogg` is `slide_in.ogg` time-reversed (`sox slide_in.ogg slide_out.ogg
reverse`) so the pair reads as one matched transition, per the spec.
`modal_swoosh_close.ogg` is derived the same way from the open swoosh.

---

## Music — 12 tracks — 10 REQUIRE ATTRIBUTION

**10 of 12** are by **Kevin MacLeod**
([incompetech.com](https://incompetech.com)), licensed **Creative Commons
Attribution 4.0** (<http://creativecommons.org/licenses/by/4.0/>). Those carry a
hard attribution obligation.

The remaining **2** are genuinely attribution-free — public-domain or
CC0 performances of public-domain compositions:

- `assets/audio/music/classical/adagio.mp3` — *Piano Sonata No. 8 in C minor 'Pathetique', Op. 13 - II. Adagio cantabile*, Daniel Veesey (composition: Ludwig van Beethoven, 1798) (Public Domain)
- `assets/audio/music/classical/counterpoint.mp3` — *Goldberg Variations, BWV 988 - Variatio 21 Canone alla Settima*, Kimiko Ishizaka (composition: J. S. Bach, 1741) (CC0 1.0)

CC0 was preferred and searched for first throughout. It was achievable for the
classical slots, where public-domain compositions have freely-licensed
recordings. It was not achievable for lounge jazz, bossa nova, or ambient —
Freesound's results there were either CC-BY-NC (unusable) or 20–48 second loops
(too short to serve as a track), so CC BY was accepted for those.

Transcoded to MP3 112 kbps / 44.1 kHz stereo. The filename is the app's slot
name and deliberately differs from the track's real title.

| Asset (slot) | Size | Real title | Artist | License |
|---|---|---|---|---|
| `assets/audio/music/ambient/distant_lights.mp3` | 2531 KB | *Silver Blue Light* | Kevin MacLeod | CC BY 4.0 **(credit required)** |
| `assets/audio/music/ambient/slow_tide.mp3` | 2462 KB | *Light Awash* | Kevin MacLeod | CC BY 4.0 **(credit required)** |
| `assets/audio/music/ambient/velvet_room.mp3` | 2462 KB | *Wisps of Whorls* | Kevin MacLeod | CC BY 4.0 **(credit required)** |
| `assets/audio/music/bossa_nova/copacabana.mp3` | 3228 KB | *Casa Bossa Nova* | Kevin MacLeod | CC BY 4.0 **(credit required)** |
| `assets/audio/music/bossa_nova/ipanema_night.mp3` | 2872 KB | *Bossa Antigua* | Kevin MacLeod | CC BY 4.0 **(credit required)** |
| `assets/audio/music/bossa_nova/sao_paulo.mp3` | 3228 KB | *Modern Jazz Samba* | Kevin MacLeod | CC BY 4.0 **(credit required)** |
| `assets/audio/music/classical/adagio.mp3` | 2736 KB | *Piano Sonata No. 8 in C minor 'Pathetique', Op. 13 - II. Adagio cantabile* | Daniel Veesey (composition: Ludwig van Beethoven, 1798) | Public Domain |
| `assets/audio/music/classical/counterpoint.mp3` | 3207 KB | *Goldberg Variations, BWV 988 - Variatio 21 Canone alla Settima* | Kimiko Ishizaka (composition: J. S. Bach, 1741) | CC0 1.0 |
| `assets/audio/music/classical/nocturne.mp3` | 2559 KB | *Gymnopedie No. 1* | Kevin MacLeod (composition: Erik Satie, 1888) | CC BY 4.0 **(credit required)** |
| `assets/audio/music/lounge_jazz/after_hours.mp3` | 3120 KB | *Just As Soon* | Kevin MacLeod | CC BY 4.0 **(credit required)** |
| `assets/audio/music/lounge_jazz/penthouse_suite.mp3` | 3009 KB | *Backbay Lounge* | Kevin MacLeod | CC BY 4.0 **(credit required)** |
| `assets/audio/music/lounge_jazz/smoke_and_mirrors.mp3` | 2284 KB | *I Knew a Guy* | Kevin MacLeod | CC BY 4.0 **(credit required)** |

### Required credit strings

Incompetech's own generator format, for the 10 CC BY tracks. These
must appear in the About / Credits surface before release. The public-domain /
CC0 tracks above are exempt.

```
"Silver Blue Light" by Kevin MacLeod (incompetech.com)
"Light Awash" by Kevin MacLeod (incompetech.com)
"Wisps of Whorls" by Kevin MacLeod (incompetech.com)
"Casa Bossa Nova" by Kevin MacLeod (incompetech.com)
"Bossa Antigua" by Kevin MacLeod (incompetech.com)
"Modern Jazz Samba" by Kevin MacLeod (incompetech.com)
"Gymnopedie No. 1" by Kevin MacLeod (incompetech.com)
"Just As Soon" by Kevin MacLeod (incompetech.com)
"Backbay Lounge" by Kevin MacLeod (incompetech.com)
"I Knew a Guy" by Kevin MacLeod (incompetech.com)

Licensed under Creative Commons: By Attribution 4.0
http://creativecommons.org/licenses/by/4.0/
```

---

## Source photograph — 1 file, Unsplash License

| Field | Value |
|---|---|
| Archived source | `assets/source/room_source.jpg` |
| Resolution | 4272×2848 |
| Title | lighted chandelier inside bar |
| Photographer | Sarah Götze (Unsplash @sarah_lu, https://unsplash.com/@sarah_lu) |
| Source | <https://unsplash.com/photos/lighted-chandelier-inside-bar-ODua_Pc7VQY> |
| License | Unsplash License |
| Attribution | not required (credit given here regardless) |

**This is not CC0.** The Unsplash License permits free commercial and
non-commercial use without attribution, but it is a distinct licence: it
forbids compiling photos to build a competing imagery service. That restriction
does not affect this project. No genuine CC0 photograph of this subject at
usable quality and aspect ratio was found.

`assets/source/room_source.jpg` is an **archived input, not a shipped asset** —
the CMake deploy step excludes `assets/source/` from the bundle. It is committed
so the room background can be re-derived at a different blur radius without
re-sourcing the photo.

One background derives from it via `tools/derive_backgrounds.py`. The same script
emits the Game screen's backdrop, but that one is generated rather than derived —
a radial gradient, no photographic content, so no licence obligation attaches:

| Asset | Origin | Blur σ | Stored | Size |
|---|---|---|---|---|
| `assets/images/tier1/background_room.png` | photo | 16 | 720×405 | 139 KB |
| `assets/images/tier2/background_game.png` | generated | n/a | 480×270 | 155 KB |

The room is one asset drawn by four screens (Root, Mode Selection, Post-Round,
Tutorial Complete). It used to ship twice — Mode Selection carried a byte-identical
copy under its own name once it took Root's blur parameters.

---

## Generated vector art — 84 files, original work

Authored for this project as SVG, no third-party source material, no licence
obligation. Editable sources live at `assets/svg/<mirrored path>.svg` and are
committed alongside the PNGs so art can be edited rather than redrawn.
`assets/svg/` is excluded from the deployed bundle.

| Group | Count | Source |
|---|---|---|
| Butler dealer (neutral, raised, profile) | 3 | `assets/svg/tier1/butler_*.svg` |
| Frog dealer (base + 2 expression overlays) | 3 | `assets/svg/tier4/*.svg` |
| Card faces + back | 53 | `assets/svg/tier2/cards/*.svg` |
| Chip denominations | 8 | `assets/svg/tier2/chips/*.svg` |
| UI icons | 13 | `assets/svg/tier1/icons/*.svg` |
| Table felt | 1 | `assets/svg/tier2/table_felt.svg` |
| Dealer button | 1 | `assets/svg/tier1/dealer_button.svg` |
| Side-pot all-in marker | 1 | `assets/svg/tier2/side_pot_all_in_marker.svg` |
| App logo | 1 | `assets/svg/tier1/app_logo.svg` |

Rebuild every PNG from source with:

```
python3 tools/rasterize_assets.py       # 84 vector assets
python3 tools/derive_backgrounds.py     # the room + the Game pool of light
```

---

## Outstanding obligation

`src/settings/settings_modal.cpp:1486` still reads:

```
ImGui::TextWrapped("Music: CC-BY tracks. Credits pending from the audio pipeline.");
```

ARCHITECTURE.md requires CC-BY tracks be credited in the About / Credits modal.
The 12 tracks above are CC BY 4.0, so **shipping with that placeholder in place
is a licence violation**. Replace it with the credit strings above, or extend
the `kCredits` array at `src/settings/settings_modal.cpp:331`.

