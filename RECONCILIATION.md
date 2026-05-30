# Contract ↔ Architecture Reconciliation — Correction Document

**Scope:** All 16 frozen Phase 0 contract headers vs. ARCHITECTURE.md (full) + ZONES.md, against current main (Z02 + Z04 + Z06 merged).
**Mode:** Read-only. No files changed, no commits. Output is corrections only.
**Resolution rule (applied uniformly):** conform the contract to the architecture. Escalations flagged only where conforming is genuinely illogical/infeasible.

**Method & confidence:** A 53-agent workflow compared each surface against the architecture, then ran one adversarial verifier per finding (each re-quoting the contract line and the architecture line; default-refute). An independent read of all 16 headers and the full architecture caught single-pass misses and validated every verdict. Every divergence is anchored to a verbatim contract line and verbatim architecture prose. Provenance: ✓ = workflow-found + adversarially verified; ＋ = added from the independent read. One workflow finding was refuted and is listed under Discarded.

**Headline:** Systematic drift is confirmed on every surface except the engine (scenario_id/rng_seed — clean) and persistence (persistence_schema/auth0_config/sync_state — clean bar one comment). Highest-impact: theme_tokens.hpp (Z06 built; ~32 token mismatches), the event-router handler-priority is inverted (tutorial escape would be swallowed by modals), settings.hpp invents preset enums + an architecture-forbidden auto-advance and gets the seed-encoded street weights wrong, and asset_paths.hpp bakes per-theme backgrounds + a 7-asset Frog set + mis-tiers the front Butler.

---

## 1. Summary table

Severity ▸ H/M/L. "Built re-touch" = forces edits to an already-built numbered zone (Z02/Z04/Z06) or an on-disk Phase 0 .cpp/stub.

### THEME — theme_tokens.hpp (Z06 built; consumed by every render zone)

| # | Divergence | Additive? | Blast radius | Built re-touch | Sev |
|---|---|---|---|---|---|
| T1 ✓ | ~27 element-specific tokens (HUD/Cluster/Offline/Outage/Settings/Tutorial/StatModalTab/TimeGrade/AgainCommit…) where arch maps each to an existing semantic token | non-add (remove) | Z08,Z10,Z11,Z12,Z13,Z14 | Z06 | H |
| T2 ✓ | ButtonBgDisabled/Primary/Danger extra (arch: disabled=opacity, primary=accent_primary, danger=state_fail) | non-add (remove) | Z07,Z08,Z11,Z12,Z13 | Z06 | H |
| T3 ✓＋ | BgRoot/BgGame/BgPostRound per-screen vs single bg_primary (Forbidden List: "no per-screen theme overrides") | non-add (collapse) | Z05,Z07,Z08,Z13 | Z06 | M |
| T4 ✓ | Missing bg_button_armed (has only AgainButtonArmed) | additive | Z13 | Z06 | M |
| T5 ✓ | Missing text_button (has only TextOnAccent) | additive | Z07–Z14 | Z06 | M |
| T6 ✓ | Missing text_placeholder | additive | Z09,Z11,Z12 | Z06 | M |
| T7 ✓ | TextDisabled/TextOnAccent extra | non-add (remove) | Z07–Z12 | Z06 | M |
| T8 ✓ | StateWarn/StateOvertime extra (overtime=state_fail) | non-add (remove) | Z10 | Z06 | L |
| T9 ✓ | InputBgFocused extra (focus is border_focus, bg unchanged) | non-add (remove) | Z09 | Z06 | L |
| T10 ✓ | StatModalBg should be named bg_modal_translucent | non-add (rename) | Z13 | Z06 | L |

### BACKBONE — event_router, focus_manager, screen_state, modal_state, animation_clock (Phase 0 impls on disk)

| # | Divergence | Surface | Additive? | Blast radius | Built re-touch | Sev |
|---|---|---|---|---|---|---|
| B1 ✓ | Handler priority inverted: ModalLayer=0, TutorialOverlay=1 — arch makes Tutorial highest | event_router | non-add (fix order) | Z05,Z11,Z14 | event_router.cpp | H |
| B2 ✓ | Event API: install_key/mouse_handler(priority) vs arch register_handler(context,key,fn) predicate model | event_router | non-add (reshape) | Z05,Z07–Z14 | event_router.cpp | H |
| B3 ✓ | current_focus() vs arch get_focused_element() | focus_manager | non-add (rename) | Z05,Z07–Z14 | focus_manager.cpp | H |
| B4 ✓ | register_focus_list(list) missing screen_id param | focus_manager | non-add (add param) | Z05,Z07–Z14 | focus_manager.cpp | H |
| B5 ✓＋ | ScreenId has Error=4 instead of TutorialComplete; TutorialComplete missing | screen_state | non-add (add+flag) | Z05,Z07,Z08,Z13,Z14 | no | M |
| B6 ✓＋ | ScreenStateSnapshot missing Tutorial state field (Inactive/Prompt/Active(n)/Complete) | screen_state | additive | Z05,Z07–Z14 | no | M |
| B7 ✓ | topmost_modal() vs arch current_modal_id()→optional; modal_stack_depth() extra | modal_state | non-add (rename) | Z11 | modal_state_stub.cpp | M |
| B8 ✓＋ | Missing is_modal_locked()→bool (true during tutorial) | modal_state | additive | Z10,Z11,Z14 | modal_state_stub.cpp | M |
| B9 ✓ | wall_clock_ms() vs arch total_ms_since_app_start() | animation_clock | non-add (rename) | Z05,Z08,Z13 | animation_clock_stub.cpp | M |
| B10 ✓＋ | Missing delta_ms_since_last_frame()→float; has animation_time_ms()→uint64 instead | animation_clock | non-add (replace) | Z05,Z07–Z10,Z13 | stub | M |
| B11 ✓＋ | Extra pause()/resume()/is_animation_paused() — modal-pause belongs to Z10 Delta Timer | animation_clock | non-add (remove) | Z05,Z10,Z11 | stub | M |
| B12 ✓ | Comment claims keyboard mode can revert to mouse — arch: "never deactivated" | focus_manager | non-add (comment) | — | no | L |

### ASSETS — asset_paths.hpp (Z02 built: tier_loader.cpp, tools/gen_placeholders.cpp)

| # | Divergence | Additive? | Blast radius | Built re-touch | Sev |
|---|---|---|---|---|---|
| A1 ✓ | Per-theme RootBackground{NoLimit,Slate,Ocean,Sage} + no background_mode/background_game | non-add (replace) | Z02,Z05,Z07,Z08,Z13 | Z02 | H |
| A2 ✓＋ | Frog set is 7 assets; arch = 3 (frog_base+pass+fail). frog_base missing | non-add (remove 4, add 1) | Z02,Z08,Z13 | Z02 | H |
| A3 ＋ | Front Butler (ButlerFrontNeutral/Raised) tagged Tier2; arch promotes to Tier1 | non-add (re-tier) | Z02,Z05,Z13 | Z02 | H |
| A4 ＋ | IconHome tagged Tier2; arch lists Home icon in Tier1 | non-add (re-tier) | Z02,Z05,Z07 | Z02 | M |
| A5 ＋ | Missing Exit door glyph, Copy glyph, Share glyph (arch Tier2) | additive | Z02,Z13 | Z02 | M |
| A6 ＋ | Missing tomato icon (arch Module 2 currency icon) | additive | Z02,Z11,Z13 | Z02 | M |
| A7 ＋ | Missing side-pot stacked-chip icon (stat-modal Overall) — distinct from SidePotAllInMarker | additive | Z02,Z13 | Z02 | M |
| A8 ＋ | SidePotAllInMarker tagged Tier3; arch lists "all-in markers" in Tier2 | non-add (re-tier) | Z02,Z08 | Z02 | M |
| A9 ＋ | Missing 4 Root UI button assets (arch Tier1) — or they're theme-rendered (flag) | additive | Z02,Z07 | Z02 | M |
| A10 ✓ | kAssetCount/enum/array stale once Frog→3 + missing assets added | non-add (recompute) | Z02 | Z02 | M |
| A11 ＋ | Position-indicator PNGs (Tier3) not enumerated in arch (likely text labels) | non-add (flag/remove) | Z02,Z08 | Z02 | L |
| A12 ＋ | Asset filenames drift from arch's named files (butler_side_profile→butler_profile; butler_front_*→butler_neutral/raised) | non-add (rename) | Z02 | Z02 | L |

### SETTINGS — settings.hpp (pure front-load; no .cpp; Z04 stores opaque blob)

| # | Divergence | Additive? | Blast radius | Sev |
|---|---|---|---|---|
| S1 ＋ | Street weights modeled as preset enum default uniform 0.25; arch = 4 coupled sliders default 15/35/30/20 — seed-encoded | non-add (reshape+value) | Z01,Z12 | H |
| S2 ＋ | Missing Chip denomination mode (Stake-scaled/Fixed) toggle | additive | Z08,Z12 | M |
| S3 ✓ | show_countdown default true; arch default OFF | non-add (value) | Z05,Z08,Z10,Z12 | M |
| S4 ＋ | Time pressure modeled as multiplier presets; arch = street-scaled default + flat custom | non-add (reshape) | Z10,Z12 | M |
| S5 ＋ | time_pressure_custom_seconds range 5–300; arch 1–300 | non-add (value) | Z10,Z12 | L |
| S6 ＋ | auto_advance+delay — architecture-forbidden (Again has "no timeout"; invented) | non-add (remove) | Z12,Z13 | M |
| S7 ✓＋ | Three volumes (master/music/sfx); arch = single Volume slider default 50 | non-add (collapse+value) | Z03,Z12 | M |
| S8 ＋ | include_multi_tier + include_bet_sizing_inputs split; arch = one Bet Sizing Engine toggle | non-add (merge) | Z01,Z09,Z12 | M |
| S9 ＋ | include_side_pots bool; arch = configurable frequency (~10%) | non-add (reshape) | Z01,Z12 | M |
| S10 ＋ | ScenarioTypeFilter bools; arch persists Custom Aggressor/Caller weights (sum 100, default 50/50) | non-add (reshape) | Z01,Z07,Z12 | M |
| S11 ✓ | Missing Reduce Motion toggle | additive | Z06,Z08,Z12 | M |
| S12 ✓ | Missing Background atmospheric movement toggle | additive | Z08,Z12 | M |
| S13 ✓ | Missing Particle drift toggle | additive | Z08,Z12 | M |
| S14 ✓ | Missing Dealer arrival animation toggle (default ON) | additive | Z13,Z12 | M |
| S15 ✓ | Missing Default Aggressor recap tab (Tier1/Summary, default Tier1) | additive | Z13,Z12 | M |
| S16 ✓ | Missing Confirm-before-leaving-site toggle (default ON) | additive | Z12,Z14 | M |
| S17 ✓ | Missing Shop button visibility toggle | additive | Z11,Z12 | M |
| S18 ＋ | keyboard_mode_auto_activate invented (arch: always activates on first tab/click) | non-add (remove) | Z12 | M |
| S19 ＋ | Over-declared: show_detailed_breakdown, show_lifetime_total, show_position_indicators, big_blind_value_cents, defer_until_user_gesture, None genre, scenario_history_retention, language, Legal ack-versions | non-add (remove/flag) | Z12 | L |

### TIER CONFIG — tier_config.hpp (Z02 built)

| # | Divergence | Additive? | Blast radius | Built re-touch | Sev |
|---|---|---|---|---|---|
| C1 ✓ | Retry = geometric 500ms ×2 ×4; arch = explicit list [immediate, 2s, 10s] | non-add (reshape) | Z02 | Z02 | H |
| C2 ＋ | Tier4 doc-comment attributes "front-facing dealer assets" to Tier4; they're Tier1 | non-add (comment) | Z02 | Z02 | L |
| C3 ＋ | fatal_failure_shows_error_screen bool flattens Tier1 (immediate) vs Tier2 (deferred-on-use) | non-add (reshape) | Z02,Z05 | Z02 | L |

### AUDIO — audio_paths.hpp (Z03 not built; pure front-load)

| # | Divergence | Additive? | Blast radius | Sev |
|---|---|---|---|---|
| AU1 ✓ | SFX list wrong: ChipLand extra; Pass/Fail extra (forbidden); missing ButtonClickConfirmation/SlideIn/SlideOut; ScenarioSpawn→CardDeal | non-add (reshape) | Z02,Z03 | H |
| AU2 ✓ | price_cents = 2500 ($25); currency is 25 tomatoes | non-add (rename+value) | Z11 | M |
| AU3 ＋ | Music tracks .ogg; arch specifies MP3 ("transcoded to ~3MB MP3") | non-add (value) | Z02,Z03 | L |

### PERSISTENCE

| # | Divergence | Additive? | Blast radius | Built re-touch | Sev |
|---|---|---|---|---|---|
| P1 ＋ | MusicLibraryState comment: shuffle toggling "via the Settings Audio section"; arch = Shop only | non-add (comment) | Z04 | Z04 | L |
| P2 ＋ | has_completed_tutorial redundant with arch's single has_seen_tutorial_prompt | non-add (flag) | Z04,Z14 | Z04 | L |

**Counts:** 56 confirmed divergences (1 workflow finding refuted). By severity: 8 High, 33 Medium, 15 Low. Built-zone re-touch required for theme (Z06), assets + tier_config (Z02), and the backbone Phase 0 stubs/impls; everything else is pure front-load. Engine: clean. Persistence: effectively clean (2 comment/redundancy notes only).

---

## 2. Full entries — highest blast radius first

Format per entry: CONTRACT (verbatim + line) · ARCHITECTURE (verbatim + section) · CORRECTION (additive/non-additive) · BLAST RADIUS · ESCALATION.

### 2.1 theme_tokens.hpp (Z06 built — every removal/rename/add re-touches the four palette .cpp + palettes.hpp + imgui_style_refresh.cpp)

> **Verified built-zone coupling:** palettes.hpp:114-116 already sets BgRoot=BgGame=BgPostRound = s.bg_primary (the three are redundant aliases of one value); palettes.hpp:137-139 map ButtonBgDisabled/Primary/Danger; imgui_style_refresh.cpp:56,58 use ButtonBgPrimary. So Z06 must be edited alongside every theme-token correction.

**T1 — element-specific tokens that architecture maps to semantic tokens ✓**
- CONTRACT: HudPotText=26 … OutageBannerBg=32 … SettingsSliderHandle=39 … TutorialScrim=40 … StatModalTabActive=45 … TimeGradeOvertime=47 … AgainButtonCommit=50 (theme_tokens.hpp:58-96).
- ARCHITECTURE: Token Application Rules (§ Color Tint Theme, ~line 164) maps each element onto the semantic set, e.g. "Tier tabs in the stat modal: active tab uses accent_primary … inactive tabs use bg_button_default"; "Time Grade row … overtime: state_fail … undertime: state_pass"; "Tutorial overlay (grey lens): … not theme-controlled"; "Persistent cluster icons: text_primary … bg_button_hover for hover"; "Offline … icon color is text_secondary"; "banner uses bg_modal … text_primary". The palette is enumerated and closed: "every color-bearing element … must reference one of the named tokens."
- CORRECTION (non-additive, remove): delete the ~27 element-specific tokens; consumers reference the semantic tokens per the application rules. (AgainButtonArmed is the one exception — keep it, it is bg_button_armed; see T4.)
- BLAST RADIUS: Z08, Z10, Z11, Z12, Z13, Z14 (not built) + Z06 (built).
- ESCALATION: none. (Architecture is explicit and closed; this is conformance, not preference.)

**T2 — extra button-state tokens ✓**
- CONTRACT: ButtonBgDisabled=16, ButtonBgPrimary=17, ButtonBgDanger=18 (theme_tokens.hpp:42-44).
- ARCHITECTURE: background button tokens are exactly bg_button_default/hover/active/armed; disabled = "rendered with reduced opacity (~40%)"; primary CTA = bg_button_default (Sign In/Up); destructive = state_fail (leave-drill Yes, Delete Account).
- CORRECTION (non-additive, remove): delete all three; disabled→opacity on bg_button_default, primary→bg_button_default/accent_primary per element, danger→state_fail.
- BLAST RADIUS: Z07,Z08,Z11,Z12,Z13 + Z06 (palettes.hpp:137-139, imgui_style_refresh.cpp:56,58). ESCALATION: none.

**T3 — per-screen backgrounds vs single bg_primary ✓＋**
- CONTRACT: BgRoot=0, BgGame=1, BgPostRound=2 (theme_tokens.hpp:20-22).
- ARCHITECTURE: "bg_primary: main screen background tint applied over the blurred room"; Forbidden List: "No per-screen theme overrides. There is one global theme; all screens … use it."
- CORRECTION (non-additive, collapse): replace the three with one BgPrimary. Low-risk: palettes.hpp already assigns all three the same s.bg_primary value. Theme-side half of already-decided #1.
- BLAST RADIUS: Z05,Z07,Z08,Z13 + Z06. ESCALATION: none.

**T4/T5/T6 — missing required tokens ✓**
- CONTRACT: no bg_button_armed (only AgainButtonArmed=49); no text_button (only TextOnAccent=9); no text_placeholder (theme_tokens.hpp:31,49,95).
- ARCHITECTURE: Token Palette lists bg_button_armed, text_button (button text), text_placeholder (placeholder text in input boxes when empty; applied to Sign-in fields and the leaderboard "Search leaderboard").
- CORRECTION (additive): add text_button, text_placeholder; treat AgainButtonArmed as the canonical bg_button_armed (rename optional — the architecture itself scopes "armed" to the Again button).
- BLAST RADIUS: Z07–Z14 + Z06. ESCALATION: none.

**T7/T8/T9/T10 — extra/misnamed tokens ✓** — remove TextDisabled, TextOnAccent (T7); StateWarn, StateOvertime (T8; overtime = state_fail); InputBgFocused (T9; focus is border_focus, fill stays bg_input); rename StatModalBg→bg_modal_translucent (T10). All re-touch Z06. ESCALATION: none.

### 2.2 Backbone (Phase 0 interfaces; stubs/impls on disk)

**B1 — handler priority inverted (functional bug) ✓ — highest-correctness-impact item**
- CONTRACT: `enum class HandlerPriority { ModalLayer = 0, TutorialOverlay = 1, ScreenContext = 2, BackgroundCatchAll = 3 }` (event_router.hpp:105-110).
- ARCHITECTURE: "Tutorial overlay handlers (highest priority — captures escape, enter, etc. when tutorial is active) / Topmost modal handlers / Active screen handlers / Global default handlers" (Global Event Router).
- CORRECTION (non-additive, fix order): TutorialOverlay = 0, ModalLayer = 1. As written, a modal out-prioritizes the tutorial overlay, so the tutorial's Escape→Skip-confirmation handler would be swallowed.
- BLAST RADIUS: Z05, Z11, Z14 + Phase 0 event_router.cpp. ESCALATION: none.

**B2 — event-router registration model ✓**
- CONTRACT: install_key_handler(handler, HandlerPriority, tag) / install_mouse_handler(...) (event_router.hpp:119-126, impl event_router.cpp:71).
- ARCHITECTURE: "Zones register handlers via register_handler(context, key, handler_fn) where context is a state predicate (e.g., 'current screen is Game AND no modal is open AND tutorial is not active')."
- CORRECTION (non-additive, reshape): introduce a context-predicate registration; the static priority enum (after B1) can remain as the coarse layer ordering, but the per-handler context predicate is the architecture's specified mechanism and is currently absent.
- BLAST RADIUS: Z05, Z07–Z14 + Phase 0 event_router.cpp. ESCALATION: none, though confirm the intended predicate signature.

**B3/B4 — focus_manager signatures ✓** — rename current_focus()→get_focused_element() (focus_manager.hpp:34, impl :29); add the screen_id parameter to register_focus_list(...) (focus_manager.hpp:63). Both architecture-named verbatim. Re-touch Phase 0 focus_manager.cpp; 8 consumer zones. ESCALATION: none.

**B5/B6 — screen_state ✓＋**
- CONTRACT: `enum class ScreenId { Root, ModeSelection, Game, PostRound, Error }; ScreenStateSnapshot { current; active_scenario; }` (screen_state.hpp:13-31).
- ARCHITECTURE: "Current screen: Root | ModeSelection | Game | PostRound | TutorialComplete … Tutorial state: Inactive | Prompt | Active(step_n) | Complete."
- CORRECTION: add TutorialComplete (non-negotiable — architecture-enumerated) and add a tutorial_state field (additive). The runtime tutorial_state is distinct from the persisted has_seen_tutorial_prompt.
- ESCALATION (judgment flag): the contract's Error=4 is not in the architecture's 5-value screen-state list. The Z05 boot-failure error screen is real, but the architecture tracks it nowhere. Recommend: add TutorialComplete; decide separately whether Error is a tracked screen-state (keep as 6th value) or a Z05-internal concern. Do NOT simply "replace Error with TutorialComplete" — that drops a needed state.
- BLAST RADIUS: Z05,Z07,Z08,Z09,Z10,Z13,Z14.

**B7/B8 — modal_state ✓＋** — rename topmost_modal()→current_modal_id() (return optional), add is_modal_locked()→bool (true during tutorial; required by Z11/Z14 modal-lock + Z10). modal_stack_depth() is an extra the architecture doesn't list (keep as internal if useful). Re-touch modal_state_stub.cpp. ESCALATION: none.

**B9/B10/B11 — animation_clock ✓＋**
- CONTRACT: wall_clock_ms(), animation_time_ms() (pausable), pause()/resume()/is_animation_paused() (animation_clock.hpp:11-33).
- ARCHITECTURE: "Exposes: total_ms_since_app_start()→uint64_t … delta_ms_since_last_frame()→float." The clock is the single monotonic source; pause/resume of scenario time is the Z10 Delta Timer ("Zone 10 subscribes to modal_opened/modal_closed to pause and resume the Delta Timer").
- CORRECTION: rename wall_clock_ms→total_ms_since_app_start (B9); replace animation_time_ms() with delta_ms_since_last_frame()→float (B10); remove pause/resume/is_animation_paused (B11) — a pausing animation clock would freeze modal slide-in/dealer-fade animations, which must run while a modal is open. Pause semantics belong to Z10.
- BLAST RADIUS: Z05,Z07,Z08,Z09,Z10,Z11,Z13 + Phase 0 stub. ESCALATION: none — but note B11 reassigns a responsibility Z11's header comments currently rely on; the Z10/Z11 modal-pause path must be the home for it.

**B12 ✓** — focus_manager.hpp:37-40 comment about "mouse interaction returning the user to mouse mode" contradicts "keyboard mode … is never deactivated." Comment-only fix.

### 2.3 asset_paths.hpp (Z02 built — tier_loader.cpp iterates kAssetCount; tools/gen_placeholders.cpp + tools/placeholder_layout.hpp switch on the exact enumerators)

**A1 — backgrounds ✓ (already-decided #1)** — replace RootBackground{NoLimit,Slate,Ocean,Sage} (:17-20, paths :107-110) with the three theme-independent blur variants: background_root.png, background_mode.png (Mode Selection + Post-Round), background_game.png — the latter two are entirely absent today. Theming via the bg_primary wash (T3). Non-additive; Z02 re-touch.

**A2 — Frog asset set ✓＋ (already-decided #2)**
- CONTRACT: FrogSideProfile=83, FrogFrontNeutral=84, FrogFrontRaised=85, FrogExpressionPass=86, FrogExpressionFail=87, FrogExpressionOvertime=88, FrogExpressionPerfect=89 (:84-90).
- ARCHITECTURE: "Frog dealer assets (one base PNG with two transparent expression overlay PNGs): frog_base.png … frog_expression_pass.png … frog_expression_fail.png. The Frog is always rendered front-facing … there is no side-profile Frog variant." Expression is binary pass/fail.
- CORRECTION (non-additive): remove FrogSideProfile, FrogFrontNeutral, FrogFrontRaised, FrogExpressionOvertime, FrogExpressionPerfect; add FrogBase; keep FrogExpressionPass/Fail. Net Frog = exactly 3.
- BLAST RADIUS: Z02,Z08,Z13 + Z02 built (tools/gen_placeholders.cpp:483,512, placeholder_layout.hpp:53,58). ESCALATION: none.

**A3 — front Butler tier ＋**
- CONTRACT: ButlerFrontNeutral=74, ButlerFrontRaised=75 under Tier 2 (:68-70,194-195).
- ARCHITECTURE: Tier 1 = "… and the front-facing Butler dealer assets (butler_neutral.png and butler_raised.png — promoted to Tier 1 to eliminate any race condition where the user could … reach the Post-Round Screen before the front-facing Butler has loaded)."
- CORRECTION (non-additive, re-tier): move both front-Butler assets to Tier1. (Also fixes tier_config C2.) Z02 re-touch. ESCALATION: none.

**A4 — Home icon tier ＋** — IconHome is Tier2 (:65,190); architecture Tier 1 includes "the Home icon." Re-tier to Tier1. Z02 re-touch.

**A5/A6/A7 — missing assets ＋** — architecture's Module 2 / Tier 2 inventory lists, and the contract omits: Exit door glyph, Copy glyph, Share glyph (Tier 2); the tomato icon (currency on Shop/Profile/Leaderboard); the side-pot stacked-chip icon for the stat-modal Overall row (Tier 2) — a different asset from the table-side SidePotAllInMarker. Add all (additive); Z02 re-touch.

**A8 — all-in marker tier ＋** — SidePotAllInMarker is Tier3 (:81,206); architecture lists "all-in markers" in Tier 2. Re-tier. Z02 re-touch.

**A9 — Root button assets ＋** — architecture Tier 1 lists "the 4 main UI button assets for the Root screen"; the contract has none. ESCALATION (judgment flag): the morph spec ("label transforms from PLAY into STANDARD") suggests these may be theme-rendered ImGui buttons rather than baked PNGs. Confirm whether the 4 Root buttons are assets (add to Tier 1) or themed widgets (then the architecture's Tier-1 "button assets" phrasing is the looser party).

**A10 — kAssetCount/arrays ✓** — kAssetCount=90 (:93) and kAssetEntries must be recomputed after A1–A9. Don't hard-code; recompute from the corrected enum. Z02 re-touch.

**A11/A12 ＋ (low)** — position-indicator PNGs (:73-78, Tier3) aren't enumerated as assets anywhere (positions read as text labels in the HUD); flag for removal/confirmation. Asset filenames drift from architecture's named files (butler_side_profile.png vs butler_profile.png; butler_front_* vs butler_neutral/raised.png) — align names.

### 2.4 settings.hpp (pure front-load — no settings.cpp; validate() declared-only; Z04 persists an opaque settings_blob, so Z04 is not re-touched)

**S1 — street weights (seed-encoded → correctness-critical) ＋**
- CONTRACT: `enum class StreetWeightPreset { Uniform, FlopHeavy, TurnHeavy, RiverHeavy, Custom }`, default Uniform; custom defaults 0.25/0.25/0.25/0.25 (:24-30,98,103-106).
- ARCHITECTURE: "Street split weights — defaults 15/35/30/20 (Pre-flop / Flop / Turn / River). Configurable via four coupled sliders using the most-recently-touched-locks rule." And §Reproducibility: "only Gameplay settings that affect scenario generation (street split weights) are seed-encoded."
- CORRECTION (non-additive, reshape + value): remove the preset enum; store four weights with defaults 15/35/30/20 (0.15/0.35/0.30/0.20). Because these feed deterministic generation, the wrong default/model would change every reconstructed scenario.
- BLAST RADIUS: Z01 (generation), Z12 (UI). ESCALATION: none.

**S2 — chip denomination mode missing ＋** — architecture Gameplay: "Chip denomination mode toggle. Two values: Stake-scaled (default) / Fixed." Add enum (additive). Z08/Z12.

**S3 — countdown default ✓** — show_countdown{true} (:147); architecture "Show countdown timer toggle (default OFF)." Set false. (Also misplaced in Display vs architecture's Gameplay section — minor.)

**S4/S5 — time pressure ＋** — TimePressurePreset {Off, Beginner×2.0, Standard, Aggressive×0.75, Brutal×0.5, Custom}, custom 5–300 (:33-40,78). Architecture: defaults scale by street (10/18/22/22 s; +50% multi-tier, +25% side-pot) with a flat custom override 1–300 s — no multiplier presets. Reshape to default-vs-custom; fix the custom floor 5→1. Street-scaled base times live in Z10 (target_time), not here. Z10/Z12.

**S6 — auto-advance (forbidden feature) ＋** — auto_advance{false} + auto_advance_delay_seconds (:204-212). Architecture Again Button: "The armed state persists indefinitely; there is no timeout." No auto-advance feature exists; the contract comment even mislabels its own invention as "the architecture default." Remove both (non-additive). Z12/Z13. (Per CLAUDE.md §11 "never invent features.")

**S7 — volumes ✓＋** — master_volume{80}, music_volume{60}, sfx_volume{75} (:164-170). Architecture Audio: a single "Volume slider, range 0-100%, default 50%," plus Mute all / Mute SFX / Mute music; "All SFX play through the global Volume slider." Collapse to one Volume (default 50) + the three mute toggles. Z03/Z12.

**S8 — bet-sizing toggle split ＋** — include_multi_tier{true} + include_bet_sizing_inputs{true} (:91,115) duplicate the single architecture "Bet sizing engine toggle (default ON)," whose enabling is what produces multi-tier. Merge to one. Z01/Z09/Z12.

**S9 — side pots ＋** — include_side_pots bool (:95); architecture: "triggers … approximately 10% of the time. This frequency is configurable." Reshape to a frequency (default ~0.10). Z01/Z12.

**S10 — scenario type model ＋** — ScenarioTypeFilter/scenario_types_enabled[2] (:15-20,84-87). Architecture selects mode at play time (STANDARD/Aggressor/Caller/Custom) and persists Custom Aggressor/Caller weights (sum 100, default 50/50) via the Custom popup's Save. Replace the enabled-bools with persisted custom weights. Z01/Z07/Z12.

**S11–S17 — missing toggles ✓** (additive; all Z12 + the owning zone): Reduce Motion, Background atmospheric movement, Particle drift (Display); Dealer arrival animation (default ON) and Default Aggressor recap tab (Tier1/Summary, default Tier1) (Recap); Confirm-before-leaving-site (default ON) (General); Shop button visibility (Tomatoes). Each named verbatim in the architecture and entirely absent. (Reduce Motion has the widest reach — Motion Layer Tiers 1/2/4 across Z08/Z13.) ESCALATION (defaults): architecture specifies existence but not the default for Reduce Motion / atmospheric-movement / particle-drift — recommend default ON for the two ambient toggles and confirm Reduce Motion's default (accessibility convention = the toggle defaults off, i.e. motion on, with prefers-reduced-motion respected).

**S18 — keyboard auto-activate ＋** — keyboard_mode_auto_activate{true} with a comment about disabling it (:153-156) contradicts the focus_manager spec: keyboard mode "Set to true on the first tab keypress OR the first mouse click," always, no setting. Remove. Z12.

**S19 — over-declarations ＋ (low, bundled)** — show_detailed_breakdown, show_lifetime_total, show_position_indicators, big_blind_value_cents, defer_until_user_gesture, ActiveMusicGenre::None, scenario_history_retention, Language, Legal ack-version fields are not specified by the architecture; flag for removal or confirmation as forward-compat extras.

### 2.5 tier_config.hpp (Z02 built)

**C1 — retry schedule ✓ (already-decided #3)**
- CONTRACT: "Subsequent retries use exponential backoff: this delay, then 2x, then 4x" with initial_retry_delay = 500ms → 500/1000/2000 (:42-44,52-73).
- ARCHITECTURE: "retries … up to three times with exponential backoff (immediate, 2 seconds, 10 seconds)" — identical across Tier 1/2/3/4.
- CORRECTION (non-additive, reshape): replace initial_retry_delay + geometric comment with an explicit `std::array<ms,3>{0ms, 2000ms, 10000ms}` (max_retries=3 already correct). Z02 re-touch. ESCALATION: none.

**C2 ＋** — Tier4 doc-comment (:28-32) says it's "Used for the Frog easter egg assets and the front-facing dealer assets that appear on the Post-Round Screen." Front-facing dealer is Tier 1 (A3); Tier 4 = Frog + alternate-genre/purchased tracks. Fix comment.

**C3 ＋** — fatal_failure_shows_error_screen (bool) collapses Tier 1's immediate error screen and Tier 2's deferred "mark unavailable → error screen only when a navigation needs it" into one flag; the architecture distinguishes them. Reshape to a tri-state (immediate / deferred-on-use / silent) or document the deferral. Both Z02.

### 2.6 audio_paths.hpp (Z03 not built — pure front-load)

**AU1 — SFX list ✓**
- CONTRACT: {ChipPush, ChipLand, SidePotSplit, ModalSwooshOpen, ModalSwooshClose, ScenarioSpawn, Pass, Fail, FrogToggle} (:12-50).
- ARCHITECTURE: the 9 SFX are Card Deal, Button Click Confirmation, Chip Push, Side Pot Split, Modal Open Swoosh, Modal Close Swoosh, Frog Easter Egg Trigger, Slide In, Slide Out. And explicitly: "Audio on Post-Round Screen entry consists only of the Slide In SFX … no … correct/incorrect audio cue … deliberately omits performance-feedback audio."
- CORRECTION (non-additive, reshape): remove ChipLand, Pass, Fail (the latter two architecturally forbidden); add ButtonClickConfirmation, SlideIn, SlideOut; rename ScenarioSpawn→CardDeal. Count coincidentally stays 9 but membership is wrong.
- BLAST RADIUS: Z02 (Tier-3 SFX load list), Z03. ESCALATION: none.

**AU2 — track price unit ✓** — `std::uint32_t price_cents; // 2500 ($25)` (:124); architecture: "Each paid track costs 25 tomatoes. Total … 200 tomatoes." Rename price_cents→price_tomatoes, values 2500→25. Currency is tomatoes, not USD. Z11.

**AU3 ＋ (low)** — music paths .ogg; architecture: "transcoded to ~3MB MP3." Align extension.

### 2.7 Engine — clean ✓

scenario_id.hpp (64-bit ScenarioId, parse/format, 0 reserved-invalid) and rng_seed.hpp (std::mt19937_64, locked, non-copyable) match Module 1 verbatim. Zero divergences. (The scenario property enums — position/type/street — live in Z01's scenario.hpp, not Phase 0, so their absence here is correct.)

### 2.8 Persistence — effectively clean ✓

persistence_schema.hpp (dual-track Tomatoes, unlocked tracks, per-genre pool, has_seen_tutorial_prompt, opaque settings_blob), sync_state.hpp (status + consecutive_failures for backoff), and auth0_config.hpp (health check before Sign-in/Sign-up/Forgot/Delete, JWKS endpoint) match the architecture. Two minor notes only: **P1 ＋** MusicLibraryState comments (:43-55) say shuffle toggling happens "via the Settings Audio section" — architecture: "track-level interactions … happen exclusively in the Shop UI." **P2 ＋** has_completed_tutorial (:109) duplicates the architecture's single has_seen_tutorial_prompt semantics. Both comment/redundancy-level; no structural change required. (The placeholder kAuth0ClientId="REPLACE_WITH_ACTUAL_CLIENT_ID" is a deploy-time fill, not a spec divergence.)

---

## 3. Already-decided entries (restated with conform-to-architecture corrections)

1. **Backgrounds** — theme-independent blur variants + bg_primary tint. Asset side A1: replace 4 per-theme RootBackground* with background_root/mode/game.png (mode + game currently missing). Theme side T3: collapse BgRoot/BgGame/BgPostRound→bg_primary. Built re-touch: Z02 + Z06.
2. **Dealer expression** — binary pass/fail; Frog always front-facing. A2: Frog = exactly frog_base.png + frog_expression_pass.png + frog_expression_fail.png; remove the 5 extras, add frog_base. Butler set verified binary and complete: butler_profile (Game side) + butler_neutral (pass) + butler_raised (fail) — only the tier of the front Butler is wrong (A3: Tier2→Tier1). Built re-touch: Z02 (incl. tools/gen_placeholders.cpp, placeholder_layout.hpp).
3. **Retry backoff** — explicit [immediate, 2s, 10s] schedule. C1: reshape tier_config's initial_retry_delay (geometric 500ms×2×4) into an explicit 3-entry list [0ms, 2000ms, 10000ms], identical for all four tiers; max_retries=3 already correct. Built re-touch: Z02. (Distinct from the Z04 sync retry 5/15/30/60s, which sync_state/Z04 already handle correctly.)

---

## 4. Discarded / refuted findings (transparency)

- **AgainButtonArmed "misnamed"** — refuted. The architecture itself scopes bg_button_armed as "Again button's armed state," so the PascalCase AgainButtonArmed is a faithful transcription, not a divergence. (The real Again-button issue is the extra AgainButtonDefault/AgainButtonCommit tokens, captured under T1.)
- A handful of agent corrections carried arithmetic/scope slips (e.g., the kAssetCount→88/87 confusion, and "replace Error with TutorialComplete" which would drop a needed state). Corrected inline (A10, B5) rather than propagated.

---

## 5. ZONES.md note

There is no uncommitted ZONES.md change to report. The working tree is clean. ZONES.md was last modified in committed change 599ef45 "fix zones.md sync_state path to src/persistence" (with 4ae636b "add architecture and zone breakdown docs" before it). ZONES.md's Z02 ("3 retries with exponential backoff") and Z04 ("5s → 15s → 30s → 60s capped") descriptions are consistent with the architecture and with the C1 / sync-retry corrections — no ZONES.md edit is implied by this sweep.

---

## 6. Recommended amendment sequencing (guidance only)

Because three surfaces re-touch already-built zones, sequence to minimize churn: (1) backbone interface fixes first (B1–B11) — they gate the most zones and B1 is a latent functional bug; (2) theme_tokens.hpp (T1–T10) co-edited with Z06 palettes; (3) asset_paths.hpp + tier_config.hpp (A*/C*) co-edited with Z02's tier_loader and placeholder tooling; (4) settings.hpp (S1–S19) — front-load, but do S1 carefully since it's seed-encoded; (5) audio_paths.hpp (AU*) — fully front-load. Engine and persistence need no amendment. Re-run the Phase 0 tests/all_headers_test.cpp gate after each surface, and bump no scenario-format version (none of these corrections touch the locked RNG/EV math — scenario_id/rng_seed/evaluator are untouched).
