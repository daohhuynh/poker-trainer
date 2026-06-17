# Poker Trainer V1.1 — Zone Breakdown

This document defines the 14 implementation zones plus Phase 0. Each zone is a parallel-buildable unit with explicit dependencies, owned source paths, and exports.

## Phase 0 — Foundation

**Scope:** Shared headers and contracts that every zone depends on. Defines type shapes, interfaces, and constants. No business logic — only declarations.

**Owns:**
- `src/backbone/event_router.hpp`
- `src/backbone/scenario_events.hpp`
- `src/backbone/screen_state.hpp`
- `src/backbone/modal_state.hpp` (interface only — implementation in Z11)
- `src/backbone/animation_clock.hpp` (interface only — implementation in Z05)
- `src/backbone/focus_manager.hpp`
- `src/persistence/sync_state.hpp`
- `src/engine/scenario_id.hpp`
- `src/engine/rng_seed.hpp`
- `src/settings/settings.hpp`
- `src/assets/asset_paths.hpp`
- `src/assets/tier_config.hpp`
- `src/audio/audio_paths.hpp`
- `src/persistence/persistence_schema.hpp`
- `src/persistence/auth0_config.hpp`
- `src/theme/theme_tokens.hpp`

---

## ZONE 01 — CORE SCENARIO ENGINE

**Scope:** Module 1 in full. Scenario generation, deterministic fold function, bet sizing engine, side pot engine, deck manager, true evaluator (Pure Bluff / Value Bet / Semi-Bluff EV formulas, locked V8.1).

**Depends on:** Phase 0 (scenario_id.hpp, settings.hpp, rng_seed.hpp).

**Owns:** `src/engine/*` — scenario.hpp, generator.cpp, evaluator.cpp, fold_function.cpp, deck.cpp, side_pot.cpp.

**Exports:**
- `generate_scenario(scenario_id, settings) → ScenarioState`
- `evaluate(ScenarioState, UserAnswers) → GradingResult`
- `is_pass(GradingResult) → bool`

---

## ZONE 02 — ASSET PIPELINE

**Scope:** Module 2 texture loader. PNG decoding via stb_image, asset registry, Tier 1/2/3/4 lazy-loading orchestration, asset handle management, per-tier CDN failure handling (3 retries with exponential backoff per tier, error screen on Tier 1/2 fatal failure, silent skip on Tier 3/4 fatal failure).

**Depends on:** Phase 0 (asset_paths.hpp, tier_config.hpp).

**Owns:** `src/assets/*` — loader.cpp, registry.cpp, tier_loader.cpp. Plus `assets/` directory (PNGs, organized by tier and category).

**Exports:**
- `load_tier(tier_n) → future<void>`
- `get_texture(asset_id) → TextureHandle`
- `is_loaded(asset_id) → bool`
- `is_unavailable(asset_id) → bool`

---

## ZONE 03 — AUDIO ENGINE

**Scope:** Module 2 audio. Music streaming via miniaudio + HTML5 audio bridge, shuffle pool playback per genre, crossfades, SFX engine, audio choreography sequencing at scenario spawn, autoplay gesture gate.

**Depends on:** Phase 0 (audio_paths.hpp), Zone 01 (engine::ScenarioType/ScenarioState), Zone 05-bridge (active_scenario())

**Owns:** `src/audio/*` — music.cpp, sfx.cpp, choreography.cpp, shuffle_pool.cpp.

**Exports:**
- `play_music(track_id)`
- `play_sfx(sfx_id)`
- `set_volume(0-100)`
- `add_to_shuffle(genre, track_id)`
- `remove_from_shuffle(genre, track_id)`
- `on_first_user_gesture()`

---

## ZONE 04 — PERSISTENCE LAYER

**Scope:** Module 7 persistence. IDBFS read/write, Auth0 SDK integration (sign-in, sign-up, password reset, sign-out, delete account, change password), Auth0 health check before opening auth-dependent modals, guest→account migration, server-side state sync with exponential backoff retry (5s → 15s → 30s → 60s capped), sync_state primitive mutation on sync failure/success, reconciliation on session start, has_seen_tutorial_prompt flag.

**Depends on:** Phase 0 (persistence_schema.hpp, auth0_config.hpp, sync_state.hpp).

**Owns:** `src/persistence/*` — idbfs.cpp, auth.cpp, sync.cpp, migration.cpp.

**Exports:**
- `load_state() → AppState`
- `save_state(AppState)`
- `sign_in(...)`, `sign_up(...)`, `sign_out()`
- `migrate_guest_to_account()`
- `auth0_health_check() → bool`

---

## ZONE 05 — EMSCRIPTEN BRIDGE / MAIN LOOP

**Scope:** Module 3. `emscripten_set_main_loop` driver, loading tier orchestration, service worker registration, CDN fetch wrappers, compression handling, the loading screen render (heavily-blurred Root background + dealer button + dashed-ring white-arc progress fill), the Tier 1/2 fatal-failure error screen with Retry button (page reload), canvas-to-window dimension binding (canvas dimensions continuously match browser viewport per Notes — Canvas Layout and Scaling), minimum-size and mobile-fallback message rendering.

**Depends on:** Modeled as two layers so the graph stays an acyclic DAG (the composition layer depends on the screen-owning zones, which in turn depend on the contract layer):
- `bridge` (contract layer — exports `register_screen_renderer`, `request_game_launch`): Zone 04 (the persistence-backed Custom-weights store wraps Zone 04's IdbfsStore).
- `bridge_platform` (composition / boot layer): Zone 02 (tier load triggers), Zone 04 (state load during boot); composes the `bridge` contract layer and the screen-owning zones into the wasm app.

**Owns:** `src/bridge/*` — main_loop.cpp, boot.cpp, loading_screen.cpp, error_screen.cpp, canvas_sizing.cpp, service_worker.js.

**Exports:**
- `app_init()`, `app_frame()`, `app_shutdown()`
- `loading_progress(0-1)`
- `canvas_dims() → Dims`

---

## ZONE 06 — THEME SYSTEM IMPLEMENTATION

**Scope:** Token-based color theme system. Theme struct definition, No Limit / Slate / Ocean / Sage palette implementations, ImGui style refresh on theme change, theme persistence integration, Theme struct as single source of truth for both ImGui-rendered and custom-rendered elements.

**Depends on:** Phase 0 (theme_tokens.hpp — token enum + struct shape).

**Owns:** `src/theme/*` — theme.cpp, palette_no_limit.cpp, palette_slate.cpp, palette_ocean.cpp, palette_sage.cpp, imgui_style_refresh.cpp.

**Exports:**
- `set_theme(theme_id)`
- `get_active_theme() → const Theme&`
- `get_color(token_id) → ImVec4`

---

## ZONE 07 — ROOT + MODE SELECTION SCREENS

**Scope:** Root screen layout (logo, 2x2 button grid, Home icon). Mode Selection layout (STANDARD button, Aggressor/Caller/Custom row). Button morph animation (Root → Mode Selection, eased+staggered). Custom Mode Configuration popup (coupled sliders/inputs, Save/Reset/Play). Background blur variant crossfade synchronized with morph. Per-screen focus list registration with focus_manager (Root: Play → Settings → Shop → Help → Home; Mode Selection: STANDARD → Aggressor → Caller → Custom → cluster icons; Custom popup: Aggressor input → Aggressor slider → Caller input → Caller slider → Save → Reset → Play → X). Arrow key increment/decrement on bounded integer inputs and sliders within the Custom popup per Notes — Keyboard Focus Behavior.

**Depends on:** Zone 02 (assets), Zone 06 (theme), Zone 14 (transitions — ceremonial transition out to Game), Zone 05 `bridge` (contract layer — `register_screen_renderer`, focus/input reconciliation substrate).

**Owns:** `src/screens/root_screen.cpp`, `src/screens/mode_selection_screen.cpp`, `src/screens/custom_popup.cpp`, `src/animations/button_morph.cpp`.

**Exports:**
- `render_root_screen()`
- `render_mode_selection_screen()`
- `open_custom_popup() → CustomConfig`

---

## ZONE 08 — GAME SCREEN RENDERING

**Scope:** Game screen layout in full. Poker table rendering, dealer in profile (Butler asset), community cards, hole cards, opponent positions with stack values, chip stack rendering convention (denomination columns for bets/pot/side pot), pushed-forward chip animation in Caller scenarios, side pot visualization with all-in marker, chip denomination legend (top-left), HUD overlay (pot/blinds/floating bet amount), scenario state communication via visual cues (no text labels), dealer click counter for Frog easter egg, Butler↔Frog asset swap, Frog expression overlay compositing on Post-Round Screen. Per-screen focus list registration with focus_manager (math inputs → bet size group (single tab stop with bounded focus indicator) → cluster icons → X). For a multi-tier Aggressor scenario (Bet Sizing Engine on) the math area shows one tier at a time — a per-tier size indicator plus that tier's Fold Probability + EV and the persistent Bet Size group — rather than all four tiers stacked; Z09 owns the tier state machine and re-registers the per-tier focus list on each Enter-advance, so the math-input portion of the focus list is the current tier's. Dealer is not in the focus list; mouse-only for easter egg.

**Depends on:** Zone 01 (scenario state to render), Zone 02 (table/dealer/card/chip assets), Zone 06 (theme tokens for chip colors and HUD), Zone 03 (chip push SFX, side pot split SFX).

**Owns:** `src/screens/game_screen.cpp`, `src/render/table.cpp`, `src/render/dealer.cpp`, `src/render/chips.cpp`, `src/render/cards.cpp`, `src/render/opponents.cpp`, `src/render/hud.cpp`, `src/animations/chip_slide.cpp`, `src/easter_egg/frog_toggle.cpp`.

**Exports:**
- `render_game_screen(scenario_state, ui_state)`

---

## ZONE 09 — MATH INTERROGATOR

**Scope:** Module 5 in full. Math input box rendering (left-middle of Game screen), focus state with white outline overlay (border_focus token), keybind handling (1-6 for input focus, Tab/Shift-Tab cycling via focus_manager, Enter submission / per-tier advance), Bet Size focus-grouped button row with 1-4 keys for tier selection (single tab stop with bounded focus indicator per Notes — Keyboard Focus Behavior), dynamic input spawning per scenario branch (Caller vs Aggressor sub-types), sequential multi-tier bet sizing presentation (one tier per screen at four fixed bet sizes — 1/3, 1/2, full, overbet — with a per-tier size indicator; per-tier Fold Probability + EV; tier-1-only Equity if Called, reused for grading at every tier; a single persistent, editable Bet Size pick present on every tier screen and graded once; an Enter-advance state machine that is forward-only and submits on the last tier; Tab cycles focus within the current tier screen and does not advance the tier), submission flow (gather every tier's Fold/EV + the single Bet Size pick + any bet-size-independent input answered once — an answer set identical in shape to a single-screen submit — send to evaluator, transition to Post-Round). Caller and single-tier Aggressor scenarios are unchanged: all inputs on one screen, Enter submits all. Arrow keys within math input boxes are no-ops (math inputs accept unbounded decimal values, arrow keys reserved for bounded inputs).

**Depends on:** Zone 01 (evaluator), Zone 06 (input field theming), Zone 14 (Game→Post-Round slide transition trigger), Zone 05 `bridge` (contract layer — `register_screen_renderer`, focus/input reconciliation substrate).

**Owns:** `src/math/input_boxes.cpp`, `src/math/bet_size_buttons.cpp`, `src/math/submission.cpp`, `src/math/keybinds.cpp`.

**Exports:**
- `render_math_inputs(scenario_state)`
- `on_submit() → GradingResult`

---

## ZONE 10 — TEMPORAL LAYER

**Scope:** Module 6 in full. Delta timer (tracks time since scenario spawn, pauses on modal open, resumes on modal close), Visual Countdown render (top-right below cluster, integer seconds, switches to red/state_fail in overtime, counts upward in overtime), target time computation per street + scenario type + multi-tier + side pot modifiers.

**Depends on:** Zone 06 (for state_fail / text_secondary tokens), modal pause/resume signals from Zone 11.

**Owns:** `src/temporal/delta_timer.cpp`, `src/temporal/visual_countdown.cpp`, `src/temporal/target_time.cpp`.

**Exports:**
- `timer_start(scenario_state)`
- `timer_pause()`, `timer_resume()`
- `timer_elapsed_ms()`
- `render_countdown()`

---

## ZONE 11 — MODAL INFRASTRUCTURE + PERSISTENT CLUSTER

**Scope:** The persistent top-right cluster (Shop/Help/Settings/Home or X) — rendering, click handlers, fourth-icon variation by screen. Offline sync indicator rendering to the left of the leftmost cluster icon, reading state from sync_state.hpp Phase 0 primitive (no direct Zone 04 dependency). Modal popup system (centered overlay, X close, click-outside dismissal, top-left icon-plus-name pill header). Modal lock during tutorial (40% opacity, click suppression, banner). Confirmation modal pattern (leave-drill, leave-site, section reset, delete account, skip tutorial — Yes red / No grey default focus; uniform No → Yes → X focus list). Equation Reference Modal (Help). Service Outage Banner system (top-center, ~1/6 canvas width, 5s with countdown bar, slide in/out at 300ms, dismiss on click, no stacking, triggered by call from any zone). Modal open/close swoosh SFX. Modal pause signal to Zone 10. Focus context push/pop on modal open/close via focus_manager. Confirmation modal focus list registration (No → Yes → X close). Leaderboard view rendering inside Shop modal frame, including search bar (32 char limit, case-insensitive plain-substring match, no regex parsing), retry button on fetch failure (re-attempts without closing modal).

**Depends on:** Zone 02 (icon assets), Zone 06 (theme), Zone 03 (swoosh SFX), Phase 0 (sync_state.hpp for offline indicator state).

**Owns:** `src/modal/cluster.cpp`, `src/modal/offline_indicator.cpp`, `src/modal/modal_base.cpp`, `src/modal/confirm_modal.cpp`, `src/modal/help_modal.cpp`, `src/modal/auth_modals.cpp`, `src/modal/outage_banner.cpp`, `src/modal/leaderboard_view.cpp`.

**Exports:**
- `open_modal(modal_id)`, `close_modal()`
- `is_any_modal_open() → bool`
- `render_persistent_cluster(screen_context)`
- `trigger_outage_banner(message_text)`

---

## ZONE 12 — SETTINGS PAGE

**Scope:** Settings modal contents. Section sidebar + scrollable list, fuzzy-match search (Fuse.js), per-section setting renders for all 9 sections (Gameplay, Units, Display, Audio, Recap, Tomatoes, Account, General, Legal). Coupled-slider street-split widget with per-slider focus stops and arrow key increment/decrement. Difficulty range slider with two-handle focus (low handle, high handle as separate tab stops), arrow keys on focused handle, 0-100 user-facing display (0-1 internal storage). Volume slider with paired text input and arrow keys (both as separate tab stops). Time pressure custom value text input with arrow keys. Account section states (logged-out, logged-in, sign-in/sign-up/forgot-password flows), Auth0 health check before opening Sign In or Sign Up modals (triggers outage banner via Z11 on failure). Theme dropdown (No Limit → Slate → Ocean → Sage). Reset section modal (multi-select with red highlight, confirm). Reset all settings flow. ToS/Privacy/About sub-modals. Settings modal focus list registration (search input → sidebar sections → body settings continuously → X close, with sidebar enter scroll-to-section + focus-jump behavior).

**Depends on:** Zone 11 (hosted in modal infra), Zone 04 (auth flows + persist settings + health check), Zone 06 (theme picker).

**Owns:** `src/settings/settings_modal.cpp`, `src/settings/sections/*.cpp` (one per section), `src/settings/search.cpp`.

**Exports:**
- `render_settings_modal()`
- `on_setting_change(setting_id, value)`

---

## ZONE 13 — POST-ROUND SCREEN

**Scope:** Notes — Post-Round Screen in full. Front-facing dealer asset rendering (Butler neutral/raised based on pass/fail, Frog with overlay composite). Stat modal at 65% opacity with three-column layout, tier tabs for multi-tier Aggressor scenarios (single tab stop with arrow key navigation, 1-5 keys for direct selection, bounded focus indicator) + Summary tab, Time Grade row with overtime/undertime coloring, side-pot icon next to "Overall". The Bet Size pick is a single scenario-level answer (the user's one optimal-sizing choice, graded once), shown on the Summary tab and optionally echoed on each tier tab for symmetry the way bet-size-independent inputs are — it is not a per-tier answer. Per-tier Fold Probability and EV remain per-tier. Again button state machine (default → armed → commit double-confirm). Scenario ID display + Copy/Share utility buttons (clipboard + Web Share API with fallback to a Z13-internal mini-modal showing the text in a selectable field for manual copy). Exit door glyph. Dealer arrival animation (sequenced fade-in: background → dealer 600ms → modal 600ms). Per-screen focus list registration (tier tab strip first if multi-tier Aggressor → Again → Exit → Copy → Share → cluster icons → Home; non-multi-tier list starts at Again). Post-Round → Game slide transition trigger. Post-Round → Mode Selection ceremonial transition trigger.

**Depends on:** Zone 01 (grading result + pass/fail), Zone 02 (front-facing Butler + Frog assets), Zone 06 (theme — pass/fail row colors), Zone 09 (grading result handoff), Zone 14 (slide + ceremonial transitions), Zone 03 (slide SFX).

**Owns:** `src/screens/post_round_screen.cpp`, `src/render/front_dealer.cpp`, `src/render/stat_modal.cpp`, `src/screens/again_button.cpp`, `src/screens/clipboard_fallback_modal.cpp`.

**Exports:**
- `render_post_round_screen(grading_result, scenario_state)`

---

## ZONE 14 — TUTORIAL SYSTEM

**Scope:** Notes — Tutorial System in full. Overlay layer (grey lens at 60% opacity, spotlight cutout, callout panels, Next button, Skip Tutorial button + confirm). Step sequencer (8 steps across Root/Mode Selection/Game-Caller/Post-Round/Game-Aggressor/Post-Round/TutorialComplete). Click interception at scripted moments. Hardcoded scenario seed loading. Forced settings management (HUD on, countdown off, Delta Timer disabled, bet sizing on) with restoration on completion or skip. Modal lock signal to Zone 11. Two-stage callout pattern for math input walkthrough. Enter-key behavior during tutorial: for Caller / single-tier Aggressor, suppress Enter until all visible inputs are filled then submit; for a multi-tier Aggressor (Step 6), the walkthrough is sequential one-tier-per-screen — tier 1 is fully walked (Fold Probability, EV, then the persistent Bet Size pick), then Enter advances through tiers 2-4 and submits on the last, suppressed on each tier until that tier's inputs are filled (Tab moves focus within a screen and does not advance the tier). Tutorial Complete terminal screen (Sign Up / Continue Playing / Home buttons, account-state-dependent). Auth0 health check on Tutorial Complete Sign Up click (triggers outage banner via Z11 on failure). has_seen_tutorial_prompt flag lifecycle. When tutorial overlay is active, register an escape handler at the highest priority below the modal layer; handler: `tutorial_skip_confirmation_open()`.

**Depends on:** Zone 04 (flag persistence + Sign Up modal trigger + Auth0 health check), Zone 07, Zone 08, Zone 09 (Enter behavior override), Zone 10 (Delta Timer disable signal), Zone 11 (modal lock cooperation + outage banner trigger), Zone 13.

**Owns:** `src/tutorial/overlay.cpp`, `src/tutorial/step_sequencer.cpp`, `src/tutorial/forced_settings.cpp`, `src/tutorial/callout.cpp`, `src/screens/tutorial_complete_screen.cpp`.

**Exports:**
- `tutorial_start()`, `tutorial_skip()`, `tutorial_advance()`
- `is_tutorial_active() → bool`

---

## Build Waves (Kahn's Topological Sort)

Each wave can be built in parallel internally. Waves must be built sequentially.

- **Wave 0:** Phase 0
- **Wave 1:** Z01, Z02, Z04, Z06 (4 zones parallel)
- **Wave 2:** Z03, Z05, Z07, Z09 (4 zones parallel)
- **Wave 3:** Z08, Z11, Z13 (3 zones parallel)
- **Wave 4:** Z10, Z12 (2 zones parallel)
- **Wave 5:** Z14 (final, alone)

