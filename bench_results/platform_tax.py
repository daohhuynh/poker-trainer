#!/usr/bin/env python3
"""Q5 — the platform-tax bill. Per-zone (directory) classification of the 33,780
src LOC into DOMAIN (poker decision math), PLATFORM TAX (substrate the browser/DOM
gives free/near-free), and INHERENT (product logic you'd write in any target).

Rule (stated in PLATFORM_TAX.md): a file is TAX if its primary job is something the
browser/DOM/JS stdlib provides directly — input events/focus/keyboard nav, text
inputs, scrolling, text/image layout, animation timing, theming, drawing UI to a
canvas (what DOM composites free), audio playback plumbing, asset fetch/decode/cache,
local-persistence plumbing, HTTP/token/auth plumbing, and the wasm<->browser glue.
DOMAIN = the poker decision mathematics. INHERENT = other product logic (scenario
generation/determinism, tutorial script, game/economy/settings *semantics*).

Splits within mixed directories are LOC estimates from reading the files; the split
rationale is in the notes. Judgment calls flagged with '*'.
"""
import json, os

# dir -> (total, domain, tax, inherent, note)
Z = {
 "engine":      (1675, 665, 0, 1010, "domain=EV/fold/side-pot/hand-eval math (665); inherent=scenario gen + MT19937_64 determinism + deck (1010)"),
 "math":        (1604, 482, 1122, 0, "domain=submission+tier_flow+interrogator state (482); TAX=input_boxes render 541 + keybinds 427 + bet buttons 154 (browser <input> free)"),
 "temporal":    (419, 93, 326, 0, "domain=target_time calibration (93); TAX=delta_timer + countdown render/pause plumbing (326)"),
 "easter_egg":  (64, 0, 0, 64, "inherent product content"),
 "animations":  (401, 0, 401, 0, "TAX: button morph + chip slide = CSS transitions/animations free"),
 "render":      (2039, 0, 2039, 0, "*TAX (judgment): hand-drawing the poker table/cards/chips/HUD to canvas; a DOM app composites <img>+CSS near-free. Domain is the MATH, not the picture."),
 "theme":       (746, 0, 746, 0, "TAX: token palettes + ImGui restyle = CSS variables free"),
 "assets":      (1127, 0, 1127, 0, "TAX: PNG decode + tiered CDN load + retry = <img>/fetch free"),
 "audio":       (1214, 0, 1112, 102, "TAX=miniaudio/WebAudio plumbing, shuffle, sfx (1112); inherent=choreography sequencing (102)"),
 "persistence": (3605, 0, 3426, 179, "TAX=IDBFS async flush + serializer + Auth0/Supabase/token + sync queue/backoff (3426); inherent=economy/tomatoes rules (179)"),
 "settings":    (3996, 0, 3338, 658, "TAX=settings_modal 2021 + account_modal 790 + auth_form 246 + fuzzy search 281 UI chrome; inherent=settings semantics + denylist (658)"),
 "modal":       (3869, 0, 3869, 0, "TAX: modal framework, cluster, leaderboard table, shop, offline indicator, outage banner = <dialog>/<table>/native scroll/overlays free"),
 "screens":     (3682, 0, 3682, 0, "*TAX (judgment): root/mode/game/post-round/complete screen layout+render; DOM gives layout/reflow/focus/scroll free"),
 "tutorial":    (1970, 0, 1098, 872, "TAX=overlay lens/spotlight/callout render (1098, a DOM tour lib gives free); inherent=step sequencer + forced settings + content (872)"),
 "bridge":      (5889, 0, 5378, 511, "TAX=boot/main-loop/emscripten glue, gl_renderer, canvas, html_overlay, idbfs, cdn, texture, transitions, focus seam, settings-persist, sfx, ambient, tilt (5378); inherent=game_launch + shared_scenario (511)"),
 "backbone":    (1462, 0, 1071, 391, "TAX=event_router+focus_manager+screen_state+modal_state+animation_clock (1071, DOM events/focus/history/rAF free); inherent=scenario_events bus + game_mode (391)"),
}

SRC_TOTAL = 33780  # canonical (per-dir sums to 33,762; ~18 in hidden/misc files)

# "pure substrate" = TAX that is near-zero code in a DOM app (excludes UI-content
# chrome: settings/modal/screens, which a DOM app still writes, just far cheaper).
UI_CHROME_DIRS = {"settings", "modal", "screens"}

dom = tax = inh = 0
pure_sub = ui_chrome = 0
rows = []
for name, (tot, d, t, i, note) in Z.items():
    assert d + t + i == tot, f"{name}: {d}+{t}+{i} != {tot}"
    dom += d; tax += t; inh += i
    if name in UI_CHROME_DIRS: ui_chrome += t
    else: pure_sub += t
    rows.append({"zone": name, "total": tot, "domain": d, "tax": t, "inherent": i, "note": note})

classified = dom + tax + inh
print(f"classified {classified} LOC across {len(Z)} zones (src total {SRC_TOTAL})\n")
print(f"{'zone':12} {'total':>6} {'domain':>7} {'tax':>6} {'inherent':>8}")
for r in sorted(rows, key=lambda r: -r["tax"]):
    print(f"{r['zone']:12} {r['total']:>6} {r['domain']:>7} {r['tax']:>6} {r['inherent']:>8}")
print(f"{'TOTAL':12} {classified:>6} {dom:>7} {tax:>6} {inh:>8}")

pct = lambda x: 100.0 * x / SRC_TOTAL
print(f"\nHEADLINE (denominator = {SRC_TOTAL} src LOC):")
print(f"  PLATFORM TAX : {tax:6} LOC = {pct(tax):.1f}%")
print(f"  DOMAIN math  : {dom:6} LOC = {pct(dom):.1f}%")
print(f"  INHERENT     : {inh:6} LOC = {pct(inh):.1f}%")
print(f"  domain:tax ratio = 1 : {tax/dom:.1f}")
print(f"  (domain+inherent 'substance'):tax = 1 : {tax/(dom+inh):.1f}")
print(f"\n  of the tax:")
print(f"    pure substrate (near-0 in DOM): {pure_sub:6} LOC = {pct(pure_sub):.1f}% of src")
print(f"    UI chrome (DOM writes cheaper): {ui_chrome:6} LOC = {pct(ui_chrome):.1f}% of src")

out = {
  "src_total_loc": SRC_TOTAL,
  "classified_loc": classified,
  "totals": {"domain": dom, "tax": tax, "inherent": inh},
  "pct_of_src": {"domain": round(pct(dom),1), "tax": round(pct(tax),1), "inherent": round(pct(inh),1)},
  "domain_to_tax_ratio": round(tax/dom,1),
  "substance_to_tax_ratio": round(tax/(dom+inh),1),
  "tax_breakdown": {"pure_substrate_loc": pure_sub, "ui_chrome_loc": ui_chrome,
                    "pure_substrate_pct": round(pct(pure_sub),1), "ui_chrome_pct": round(pct(ui_chrome),1)},
  "zones": rows,
}
with open(os.path.join(os.path.dirname(__file__), "platform_tax.json"), "w") as f:
    json.dump(out, f, indent=2)
print("\nWrote bench_results/platform_tax.json")
