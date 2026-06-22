# Poker Trainer

A browser-based trainer that drills the real-time decision math of poker (pot odds, outs, equity, EV, fold probability, bet sizing) and grades your answers against strict margins under time pressure.

It is written in C++23 and compiled to a single WebAssembly binary. The UI is a real-time Dear ImGui scene rendered through an in-house WebGL2 renderer to one HTML5 canvas. There is no server-side game logic: every scenario is generated deterministically in the browser from a 64-bit Scenario ID, the math is computed and graded locally, and the whole app ships as `poker_trainer.wasm` plus static art and audio served from any static host.

The interesting part of this codebase is not raw speed. It is the architecture: a system decomposed into dependency-ordered zones behind frozen interface headers, an immediate-mode UI given retained-mode behavior (stable keyboard focus, modal focus traps) on top of a renderer that rebuilds the entire UI every frame, a clean asset bind seam that decouples art from code, and a locked decision-math engine that is the same code in the browser and in the native test suite.

---

## What it drills

Each round presents one poker scenario and asks for the math behind the decision.

- **Scenario types** (`engine::ScenarioType`): `Caller`, and three Aggressor variants `AggressorPureBluff`, `AggressorValueBet`, `AggressorSemiBluff`. The type is communicated visually through table state, not a text label, so you learn to read the board the way you would at a real table.
- **The six math inputs** (`engine::InputId`): `PotOdds`, `Outs`, `Equity`, `Ev`, `FoldProbability`, `BetSize`. Which inputs a scenario asks for depends on its type: a Caller answers pot odds / outs / equity / net-call EV; an Aggressor answers per-tier fold probability and EV and picks the optimal bet size; a semi-bluff additionally answers equity-if-called.
- **Bet sizing** (`engine::BetTier`): `OneThirdPot`, `HalfPot`, `FullPot`, `Overbet`. With the Bet Sizing Engine on, an Aggressor scenario presents the same spot at all four sizes (the multi-tier drill) and grades each tier.
- **Grading margins** (`engine/evaluator.hpp`): probabilities within +/-5 percentage points (`kProbabilityMarginPp`), dollar EV within `max($0.50, 5%)` (`kEvAbsoluteFloor`, `kEvRelativeMargin`), outs as an exact integer match. A chosen bet tier is correct when its EV is within the same propagated EV tolerance of the max-EV tier, so tiers that are statistically tied with the best all pass.
- **Time pressure**: scenarios run against a target time that scales by street and scenario type, with an optional flat custom value (`settings::GameplaySettings::time_pressure_custom_seconds`). A full pass requires both correct math and finishing at or under target.

The aesthetic is a high-limit private cardroom (leather, wood, brass, dim lighting), explicitly not a Las Vegas casino floor. In-app currency (Tomatoes) is awarded only on full passes and shown only on the Shop, Profile, and Leaderboard surfaces, never on the HUD or the Post-Round screen, so the active-training surfaces stay focused on math and time.

---

## Architecture

The codebase is organized around a few decisions. Each section below is one problem and the specific mechanism that solves it.

### Zones behind frozen contracts

The system is split into a Phase 0 backbone plus fourteen numbered zones, arranged as an acyclic dependency graph and built in waves. The decomposition is in `ZONES.md`; the design spec is in `ARCHITECTURE.md`.

| Zone | Responsibility |
|------|----------------|
| Phase 0 | Backbone primitives and the frozen contract headers every zone builds against (`src/backbone/`, plus `scenario_id.hpp`, `settings.hpp`, `theme_tokens.hpp`, and the other contract headers). |
| Z01 Engine | Deterministic scenario generation, the locked EV formulas, grading (`src/engine/`). |
| Z02 Assets | Tiered PNG load over a CDN/dev fetch, decode, registry (`src/assets/`). |
| Z03 Audio | Music, SFX, spawn choreography over a miniaudio backend (`src/audio/`). |
| Z04 Persistence | Guest-mode IDBFS store, migration, Auth0-backed account/sync (`src/persistence/`). |
| Z05 Bridge | Emscripten platform bring-up, WebGL2 renderer, input, boot, main loop, render seam (`src/bridge/`). |
| Z06 Theme | Four palettes and the token system (`src/theme/`). |
| Z07 Root + Mode | Root and Mode Selection screens, the Custom popup, the button morph (`src/screens/`, `src/animations/`). |
| Z08 Game | The first-person table: felt, cards, chips, dealer, opponents, HUD (`src/screens/game_screen.cpp`, `src/render/`). |
| Z09 Math Interrogator | Math input boxes, bet-size buttons, submission, keybinds (`src/math/`). |
| Z10 Temporal | Delta timer, visual countdown, per-scenario target time (`src/temporal/`). |
| Z11 Modal + Cluster | Modal infrastructure, the persistent icon cluster, outage banner, leaderboard (`src/modal/`). |
| Z12 Settings | The nine-section settings page (`src/settings/`). |
| Z13 Post-Round | The recap screen, front-facing dealer, Again button (`src/screens/post_round_screen.cpp`, `src/render/front_dealer.cpp`). |
| Z14 Tutorial | The onboarding overlay and step sequencer (`src/tutorial/`). |

The contract-first rule is what keeps this honest. A zone may include headers only from its own zone, the Phase 0 backbone, and the specific zones listed as its dependencies in `ZONES.md`. No zone reaches into another zone's internals: all cross-zone coordination flows through the six backbone primitives (event router, focus manager, scenario events bus, screen-state singleton, modal-state observer, animation clock). Once Phase 0 is signed off, the contract headers are immutable, so a zone can be built and tested against a frozen interface before the zone behind that interface exists. Dependencies run one direction only.

### Single-slot, last-writer-wins render dispatch

The main loop has to render "the active screen" every frame without depending on the zones that own screens. The indirection is `src/bridge/screen_dispatch.{hpp,cpp}`: one render callback slot per `ScreenId`, held in a flat `std::array<ScreenRenderFn, kScreenCount>`. A screen-owning zone calls `register_screen_renderer(screen, fn)` during init, and the main loop calls `render_screen(active)`.

Registration overwrites the slot, so the last zone to register a given screen wins it. This is deliberately not a layered stack. A screen is owned by exactly one zone, and when a screen needs to show another zone's content it composes that content by calling the other zone's render function directly. The Game screen is the worked example: `boot.cpp` installs Z09 (the math inputs) first and Z08 (the Game renderer) second, so Z08 ends up owning the Game slot and calls Z09's render path itself. Composition is an explicit function call, not a z-order to reason about.

There is one exception to "one slot per screen": modals and the service-outage banner are a single top-level overlay (`register_overlay_renderer` / `render_overlay`) drawn above the active screen each frame. The main loop renders the Loading and Error screens directly (the bridge owns those) and otherwise runs `render_screen(active)` then `render_overlay()` (`src/bridge/main_loop.cpp`).

### Event router priority stack

DOM input is fed into both ImGui's IO and the backbone event router (`src/backbone/event_router.hpp`). The router dispatches each event through a four-level priority stack:

```
TutorialOverlay (0)  >  ModalLayer (1)  >  ScreenContext (2)  >  BackgroundCatchAll (3)
```

Handlers register with `register_key_handler(context, handler, priority, tag)`. The `context` is a state predicate (a `std::function<bool()>` that reads backbone state, for example "screen is Game and no modal is open"); a handler is eligible only when its predicate passes. The router walks the stack in ascending priority order and the first eligible handler that returns `true` consumes the event. Modals live above screens so an open modal captures Escape and Enter before the screen sees them; the tutorial overlay sits above everything for the same reason. The universal Tab / Shift-Tab navigation handler is installed once at `BackgroundCatchAll` (the lowest priority) in `boot.cpp`, so any screen or modal can override navigation by registering higher.

### The asset bind seam

Art is late-bound and swappable with no code change. Every drawable routes through one point, `bridge::draw_asset_image` in `src/bridge/asset_image.hpp`, which is the single `ImDrawList::AddImage` call shared by backgrounds, the logo, the table, cards, chips, the dealer, and markers. It is keyed by a stable `assets::AssetId` handle, not a path or a pointer.

Underneath, `draw_asset_image` calls `bridge::asset_gl_texture(id)` (`src/bridge/texture_bind.hpp`). Zone 02 decodes a PNG to CPU-resident RGBA8; `texture_bind` uploads those pixels to a WebGL2 texture once, caches the GL name per `AssetId`, and returns it. When the texture is not available (still loading, unavailable after a fetch failure, or no GL context in the native test build) it returns 0 and `draw_asset_image` returns `false`, so the caller draws a procedural fallback instead. Because lookups are keyed by `AssetId` and the bytes are uploaded once, dropping real art in later is a file overwrite at the same asset path followed by a rebuild: the exact same code draws the new pixels, with no cache invalidation and nothing in the render code to change. The header is deliberately ImGui-free and returns a plain integer, so the pure, native-testable `bridge` library can supply a no-op stub while the real WebGL2 upload lives in `texture_bind.cpp` and is compiled only into the wasm build.

Asset bytes are fetched at runtime over `bridge::make_cdn_fetch` (`src/bridge/cdn_fetch.cpp`, via `emscripten_async_wget_data`) at asset-root-relative paths like `assets/images/tier1/dealer_button.png`, loaded in tiers (Tier 1 synchronously for Root, Tier 2 in the background, Tier 3 audio, Tier 4 the on-demand easter-egg art).

### The decision-math engine

`src/engine/` is the domain core and the most heavily tested zone. A 64-bit `engine::ScenarioId` seeds a `std::mt19937_64` (Mersenne Twister), which deterministically produces a scenario's identity: hole cards, board, position, scenario type, side-pot status, and stacks. The same ID always regenerates a bit-identical `ScenarioState` (the struct's `operator==` compares every field, doubles included), which is what makes shared scenarios, replays under different settings, and reproducible bug reports possible.

The EV formulas in `src/engine/evaluator.cpp` are locked at version V8.1 and verified at extreme values:

```cpp
double pure_bluff_ev(double p_fold, double pot, double bet)   = p_fold*pot - (1-p_fold)*bet;
double value_bet_ev(double p_call, double bet)                = p_call*bet;
double semi_bluff_ev(double p_fold, double equity, p, b)      = p_fold*pot + (1-p_fold)*(equity*(pot+2*bet) - bet);
double net_call_ev(double equity, double pot, double bet)     = equity*(pot+bet) - (1-equity)*bet;
```

`evaluate(state, answers)` produces one `InputGrade` per input the scenario asks for (an unfilled box grades incorrect), applies the per-input margins, and sets `all_correct`. The extreme-value cases (pure bluff at `P(fold)=1` equals pot, value bet at `P(call)=0` equals 0, the semi-bluff identity, and so on) are pinned as non-negotiable tests in `tests/engine/evaluator_test.cpp`.

---

## Key seam: immediate-mode focus reconciliation

This is the single most interesting piece of the codebase, so it gets its own section.

Dear ImGui is immediate mode: every frame the entire UI is rebuilt from function calls and then discarded. There is no retained widget tree, which means there is no persistent set of focusable elements and no stable notion of "the focused element" that survives from one frame to the next. ImGui has its own per-frame keyboard focus and its own `WantTextInput` flag, but nothing application-level. The trainer needs the opposite: stable, ordered keyboard navigation, digit-key focus jumps, and modal focus traps, all on top of a UI that forgets itself sixty times a second.

**The focus model** lives in `src/backbone/focus_manager.hpp` and is the single source of truth for which element is focused. Elements are identified by `FocusableId`, a 64-bit FNV-1a hash of a string literal computed at compile time by `make_focusable_id("root.play_button")`, so IDs are stable across builds with no hand-maintained table. A screen registers an ordered focus list on entry with `register_focus_list`; the order of that span is the Tab order, and registration relocks the context so nothing is focused until the first Tab, digit, or click arms it. `advance_focus(reverse)` moves the pointer and wraps at the ends; `snap_focus_to(id)` jumps directly (the digit keys 1-6, mouse clicks, and arrow movement within a bounded group all snap). Keyboard mode latches on the first Tab or click and stays on for the session. A modal opens by pushing a focus context (`push_focus_context`) with its own focusables and an initial focus that is armed immediately (the focus trap presents focus with no priming Tab); `pop_focus_context` restores the prior context and its prior armed state. Nothing outside this API touches the focus pointer, and the renderer queries `get_focused_element()` to draw the indicator, so there is exactly one source of truth.

**The hard part** is that `focus_manager` owns "which element is focused and where the outline draws," while ImGui independently owns keyboard text capture (the active text widget and `WantTextInput`). Nothing couples them, so the visible focus ring and the actual typing target drift apart. `src/bridge/focus_registry.hpp` is the reconciliation substrate that couples them every frame. It is three separable capabilities:

1. **A per-surface registry** mapping each `FocusableId` to a `FocusableEntry { is_text_field, activate, adjust }`. A surface (the math inputs, the Custom popup, a text-field modal) populates it when it registers its focus list and clears it on re-registration.
2. **The reconcile decision**, `decide_focus_reconcile(registry, prev, current)`, a pure function with no ImGui that compares the element ImGui was last steered to (`prev`) against the element `focus_manager` now reports (`current`) and returns `None`, `FocusTextBox(current)`, or `YieldKeyboard`. The rules are subtle and each one fixes a real bug:
   - A registered non-text element yields the keyboard *every* frame, and this is checked before the unchanged early-out, because a pending `SetKeyboardFocusHere` from the text field you just left would otherwise re-grab focus permanently.
   - `current == prev` returns `None`, because re-grabbing a text field every frame traps the caret.
   - A registered text field returns `FocusTextBox`; an id this surface does not own returns `None`.
3. **Dispatch**, `dispatch_focus_key(registry, focused, key)`, which routes Space/Enter to the focused element's `activate` hook and arrows to `adjust(+/-1)`.

The render glue applies the decision: `begin_focus_reconcile` performs the once-per-frame yield (`ClearActiveID`, gated on `io.WantTextInput` so a mid-click button is left alone), `grab_keyboard_if_target` calls `SetKeyboardFocusHere` on the targeted text box during its layout, and `draw_focus_ring` / `draw_focus_ring_rect` draw the 2px ring (in a color the caller passes in, because the bridge layer has no theme dependency).

The split is what makes it testable. The pure parts (the registry, the reconcile decision, dispatch) carry no ImGui and are unit-tested in `tests/bridge/focus_reconcile_test.cpp`. The only ImGui-touching code is the render glue, compiled behind `#ifdef __EMSCRIPTEN__` and a no-op natively, so the whole `bridge` library stays ImGui-free under the native test compiler. One shared `FocusRegistry` lives off the app-root `BridgeRuntime`; `boot.cpp` wires the same instance into the math interrogator and the Custom popup, and the modal layer reaches it for text-field modals. A per-frame watcher re-registers a screen's base focus list after a Game or Post-Round visit overwrites it, so Tab keeps working when you navigate back.

---

## Tech stack

| Layer | Technology | Purpose |
|-------|------------|---------|
| Language | C++23 (strict, `-Wall -Wextra -Wpedantic -Werror`) | Application and engine code. |
| Target | WebAssembly via Emscripten | Single-binary browser deployment. |
| UI | Dear ImGui (vendored core, `third_party/imgui/`) | Immediate-mode widget and draw-list layer. |
| Rendering | WebGL2 / OpenGL ES 3.0 | In-house renderer in `src/bridge/gl_renderer.cpp` (no third-party ImGui backend). |
| Build | CMake (>= 3.24) | Configures the Emscripten app build and the native test build. |
| Tests | GoogleTest v1.14.0 (`FetchContent`) | Native unit tests for the deterministic zones. |
| Audio | miniaudio + stb_vorbis (`third_party/`) | SFX device and Ogg/Vorbis decode; HTML5 audio for music. |
| Images | stb_image (`third_party/stb/stb_image.h`) | PNG decode to RGBA8. |
| Persistence | IDBFS (`-lidbfs.js`, `src/persistence/idbfs.cpp`) | Durable guest-mode local storage in IndexedDB. |
| Accounts | Auth0 (`src/persistence/auth.cpp`, `auth0_config.hpp`) | Authentication and server sync for the leaderboard. |
| Offline | Service worker (`src/bridge/service_worker.js`) | Versioned PWA caching of the bundle and assets. |

---

## Getting started

### Native tests

The tests compile to native code (not WebAssembly) and need no browser or emsdk. This is the fast inner loop.

```sh
cmake -B build-test -DENABLE_TESTS=ON
cmake --build build-test -j
ctest --test-dir build-test
```

`ENABLE_TESTS=ON` fetches GoogleTest via `FetchContent` and builds the test executables. `ctest` runs all fourteen of them.

### WebAssembly build

Activate emsdk first (its environment is not sourced automatically):

```sh
source /path/to/emsdk/emsdk_env.sh
mkdir -p build && cd build
emcmake cmake -DCMAKE_BUILD_TYPE=Release ..
emmake make -j
```

This produces `poker_trainer.js` and `poker_trainer.wasm` in the build directory and copies `service_worker.js` next to them (a post-build step, so the worker is served from the bundle root where its scope can control the page). The wasm link flags request WebGL2 (`-sMIN_WEBGL_VERSION=2`, `-sMAX_WEBGL_VERSION=2`, `-sFULL_ES3=1`), memory growth, a persistent runtime (`-sEXIT_RUNTIME=0`), and the IDBFS backend.

### Serving locally

The app draws to a canvas and fetches art at asset-root-relative paths, so serve a directory that contains the bundle, the service worker, a minimal host page, and the `assets/` tree. The host page is just a canvas plus the loader:

```html
<!doctype html><html><head><meta charset="utf-8">
<style>html,body{margin:0;height:100%;background:#111}canvas{display:block}</style></head>
<body><canvas id="canvas" oncontextmenu="event.preventDefault()"></canvas>
<script>var Module={canvas:document.getElementById('canvas')};</script>
<script src="poker_trainer.js"></script></body></html>
```

Then serve over HTTP (a `file://` open will not work, the wasm fetch needs a server):

```sh
python3 -m http.server 8000
```

**Service worker caching.** `service_worker.js` serves the core bundle (`poker_trainer.js` / `poker_trainer.wasm` and HTML navigations) network-first, so a rebuilt bundle loads on a normal reload. Static assets (PNGs, audio) are served cache-first, keyed by `CACHE_VERSION`. That means a swapped art or audio file will not appear until you bump `CACHE_VERSION` (the activate handler drops prior caches), clear the service-worker cache in devtools, or load in an incognito window.

---

## A short code tour

Read these in order to follow the system from its contracts up to a running screen.

1. **`src/backbone/event_router.hpp`**: a representative backbone contract. Shows the house style (snake_case, `enum class` with explicit underlying types, no hidden globals) and the input priority stack everything else routes through.
2. **`src/bridge/screen_dispatch.hpp` / `.cpp`**: the render seam. One slot per screen, last writer wins, plus the single top-level overlay. Small file, central idea.
3. **`src/bridge/boot.cpp`**: how it all wires together. Backbone init order, the IDBFS-gated second half of boot, and the deliberate install order that makes the Game screen compose the math inputs.
4. **`src/backbone/focus_manager.hpp` + `src/bridge/focus_registry.hpp`**: the focus model and the reconciliation substrate from the key-seam section above. The reconcile rules repay a careful read.
5. **`src/engine/evaluator.cpp`**: the locked V8.1 EV formulas and the grader. The domain core in under 170 lines.
6. **`src/bridge/asset_image.hpp` + `src/bridge/texture_bind.hpp`**: the asset bind seam. One `AddImage` point over a cached, `AssetId`-keyed GPU upload with a procedural fallback.
7. **`src/screens/screen_registration.cpp`**: a real zone registering its renderers and handlers against those seams, with the per-frame focus-reentry watcher.

---

## Testing and build posture

`ctest --test-dir build-test` runs fourteen test executables, all passing. Thirteen are GoogleTest suites totaling 473 test cases (engine determinism and the locked EV formulas, the focus reconcile decision, the event router and focus manager, asset loading, persistence, audio, and the screen/modal wiring); the fourteenth is the Phase 0 sign-off gate (`tests/all_headers_test.cpp`), which includes every contract header and instantiates one of each type to confirm they compile together. Rendering, audio output, and modal layout are verified by running the app rather than unit-tested, by design.

Everything compiles as strict C++23 under `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Werror`. Low-level Emscripten/WebGL binding code runs under a reduced but still `-Werror` warning baseline.
