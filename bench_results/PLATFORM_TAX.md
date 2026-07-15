# The Platform Tax — cost of shipping the WHOLE app layer in C++/WASM

Thesis under test: what does it cost to write the whole application layer in C++ and
ship it to the browser, instead of the normal split (C++ engine core + JS/TS UI)?
Evidence is source-cited; LOC from `wc -l`. Raw: `platform_tax.json`. Not flattering.

---

## Q1 — The "in-house WebGL2 renderer"

| | |
|---|---|
| **What it is** | `src/bridge/gl_renderer.{cpp,hpp}` — a minimal Dear ImGui render backend: one `#version 300 es` shader pair, a font-atlas upload, per-`ImDrawList` `glBufferData(STREAM_DRAW)`, scissor-clipped `glDrawElements`. **297 LOC** (243 cpp + 54 hpp). |
| **How it differs from stock `imgui_impl_opengl3`** | Stock (upstream `backends/imgui_impl_opengl3.cpp`; **not vendored in this tree**, so LOC not measured here — order ~600–800) handles GL/GLES/WebGL version detection, GL **state save/restore**, `VtxOffset`/large-mesh, sampler bind, polygon-mode, clip-origin, a persistently-grown VBO, and multi-viewport. This one is a **single hardcoded WebGL2/GLES3 path** with none of that — a pared-down re-implementation of a subset of the stock backend. |
| **Why it exists (documented reason?)** | **None found.** README:126 states it as a fact — *"In-house renderer in `src/bridge/gl_renderer.cpp` (no third-party ImGui backend)."* Commit `1e07efc` vendored ImGui as a *"core source drop"* (backends dir deliberately excluded), which is what forced writing one. No comment, ZONES.md, CLAUDE.md, or commit gives a technical reason (WebGL2/`FULL_ES3`, CDN textures, custom shaders — none cited). |
| **Honest verdict** | **No documented reason; appears built from scratch by choice.** Stock `imgui_impl_opengl3` explicitly supports Emscripten/WebGL2 out of the box, so ~297 LOC was paid for something the upstream backend would have provided free. Not a big number, but a pure self-inflicted line item. |

---

## Q2 — Rendering the ToS / Privacy HTML

**Mechanism: punted to the DOM.** The HTML is **not** parsed or rendered in C++. At
runtime the app creates one `<iframe id="pt_html_overlay">` via `EM_ASM` and positions
it over the canvas at the modal's ImGui rect (`src/bridge/html_overlay.cpp`).

Code path: `settings_modal` → `show_html_overlay(url, x, y, w, h)` (`html_overlay.cpp:40`)
→ `EM_ASM` creates/positions the iframe, `f.src = url`. The iframe is
`pointer-events:none` (display-only; clicks/wheel fall through to the canvas), so the
app keeps focus. Scrolling is **manually bridged**: arrow keys + wheel + PageUp/Down in
`platform.cpp` call `scroll_html_overlay()` → `iframe.contentWindow.scrollBy()`.

| Item | LOC / size |
|---|---|
| `html_overlay.cpp` + `.hpp` (iframe create/position/show/hide/scroll glue) | **165** |
| scroll/PageUp-Down forwarding in `platform.cpp` | ~35 |
| shipped `terms.html` / `privacy.html` (Termly-generated, static assets — not code) | 139 KB + 175 KB |

**Price:** ~**200 LOC** of overlay-coordination glue. They did **not** pay for an HTML
parser/renderer — they kept the DOM for the one thing the DOM is unavoidable for. The
tax here is small and the choice is the *right* one; the only cost is that a canvas app
must manually bridge position/scroll/visibility for content the DOM would have laid out,
scrolled, and made selectable for free.

---

## Q3 — Persistence (3,605 LOC) by concern

| Concern | LOC | Files |
|---|---:|---|
| Auth0 sign-in/up/reset/delete + token/session | **924** | `auth0_backend.*` (532), `auth.*` (392) |
| Server sync backend (Supabase HTTP + RLS + leaderboard/report RPC) | **887** | `supabase_backend.*` (810), `supabase_config.hpp` (77) |
| Local store: binary serializer + IDBFS async flush | **612** | `idbfs.cpp/.hpp` (531) + `bridge/idbfs_backend.cpp` (81) |
| Sync engine: offline queue + backoff + reconcile gate + conflict | **509** | `sync.*` (388), `sync_state.*` (121) |
| Persistence service (orchestration) | **257** | `persistence_service.*` |
| Schema / versioning / migration | **285** | `persistence_schema.hpp` (186), `migration.*` (99) |
| Economy (tomatoes currency rules) — *inherent, not tax* | **179** | `economy.*` |
| clock | **33** | `clock.hpp` |

**What IDBFS's async flush forces (vs a synchronous native fs):**
`StorageBackend::write()` writes the blob to MEMFS synchronously (`fopen/fwrite`), then
calls `FS.syncfs(false, cb)` — **async, fire-and-forget, no completion barrier, no error
propagation** (`idbfs_backend.cpp:26-32`). Consequences a native `fsync` would not
impose:
1. `save_state()` returns **before durability**; a tab close in the write→flush window
   loses that write silently (no confirmation, no flush retry).
2. Boot must **sequence around an async mount**: startup `FS.syncfs(true)`
   (IndexedDB→MEMFS) must complete *before the first read*, so `app_init` is split into
   `begin_persistence_load → pt_boot_on_idbfs_ready → finish_boot_after_persistence`
   (`boot.cpp:211-221`). A synchronous fs just `read()`s at boot.
3. Best-effort local durability is *why* the server-sync backstop exists for logged-in
   users.

**Offline→online:** the `SyncEngine` holds an ordered pending-write queue. On push
failure it goes `SyncFailing`, increments `consecutive_failures`, schedules backoff
(**5s→15s→30s→60s capped**, `sync.hpp:216`), and the offline indicator shows. `pump()`
(called each frame) retries when due; on success it flushes the **whole queue oldest-
first**, resets failures, `SyncOk`. A **session-start reconcile gate** withholds all
pushes until the first successful server fetch this session (so local writes can't
clobber authoritative server state).

**Conflict resolution:** **server-authoritative wholesale replace (last-write-wins)** —
`adopt_server_state()` replaces local state with the server's, with three exceptions:
local Auth0 identity is pinned, `display_name` is taken from the server, and the two
tutorial latches are **monotonic OR-merged** (`idbfs.cpp:339-365`). There is **no
field-level merge, no vector clocks, no CRDT**. Honest limitation: a logged-in user's
offline progress not pushed before a reconcile on another device is **lost** (server
wins). Simplest correct model; the "offline is hardest" difficulty is real and lives in
the 509-LOC sync engine + the two-phase async-boot, not in merge logic.

---

## Q4 — The "audio thread" atomic: **VESTIGIAL** in the shipped binary

`modal_state.cpp` mirrors modal depth into `std::atomic<std::size_t>` "so Z03's audio
update … reads a consistent count with no data race." **In the wasm build there is no
second thread**, so the atomic guards a race that cannot occur:

- **No threading in the link:** no `-pthread`, `-sUSE_PTHREADS`, `-sAUDIO_WORKLET`, or
  SharedArrayBuffer anywhere in `CMakeLists.txt`.
- **The code says so:** `audio_backend.cpp:76` — *"no internal job thread (the build has
  no -pthread)"*; `audio_engine.hpp:62` — *"the app is single-threaded (the browser main
  thread)."* miniaudio's Web-Audio device without threads drives a **ScriptProcessorNode
  whose `onaudioprocess` runs on the main thread**.
- **The consumer runs on the main thread:** the only reader, `audio.cpp:125`
  (`modal_stack_depth()`), sits inside `audio_update()`, which is registered as a
  **frame tick** (`audio.cpp:88`, `register_frame_tick(audio_update)`) — i.e. it runs in
  the `emscripten_set_main_loop` callback, the *same* thread as the modal writer (Z11).

**Verdict:** the atomic is **vestigial in production** — real cross-thread safety that
the shipped single-threaded wasm target does not need. It is defensible only as
native-build/forward-compat correctness (native miniaudio *does* use a real audio
callback thread), but the comment ("from the audio thread") overstates the wasm reality.
Do not claim a cross-thread story for the wasm binary.

---

## Q5 — The full bill

**Rule.** TAX = a file whose primary job is substrate the browser/DOM/JS gives directly
(input/focus/keyboard nav, text inputs, scrolling, text/image layout, animation timing,
theming, drawing UI to canvas, audio plumbing, asset fetch/decode, persistence plumbing,
HTTP/token/auth, wasm↔browser glue). DOMAIN = the poker decision mathematics. INHERENT =
other product logic you'd write in any target (scenario generation/determinism, tutorial
script, game/economy/settings *semantics*). Mixed dirs split by reading the files; `*` =
judgment call.

| zone | total | domain | tax | inherent | note |
|---|---:|---:|---:|---:|---|
| bridge | 5889 | 0 | **5378** | 511 | wasm↔browser glue, gl_renderer, canvas, html_overlay, idbfs, cdn, transitions, focus seam, sfx, ambient; inherent = game_launch + shared_scenario |
| modal | 3869 | 0 | **3869** | 0 | modal fw, cluster, leaderboard table, shop, banners = `<dialog>`/`<table>`/scroll free |
| screens | 3682 | 0 | **3682** | 0 | `*`screen layout/render; DOM gives layout/reflow/focus/scroll free |
| persistence | 3605 | 0 | **3426** | 179 | IDBFS/serializer/Auth0/Supabase/sync; inherent = economy |
| settings | 3996 | 0 | **3338** | 658 | settings/account/auth-form/search UI chrome; inherent = settings semantics + denylist |
| render | 2039 | 0 | **2039** | 0 | `*`hand-drawing table/cards/chips/HUD to canvas; DOM composites `<img>`+CSS near-free |
| assets | 1127 | 0 | **1127** | 0 | PNG decode + tiered CDN load + retry = `<img>`/fetch free |
| math | 1604 | 482 | **1122** | 0 | domain = submission + tier state (482); tax = input-box render + keybinds + bet buttons |
| audio | 1214 | 0 | **1112** | 102 | WebAudio/miniaudio plumbing; inherent = choreography |
| tutorial | 1970 | 0 | **1098** | 872 | tax = overlay lens/spotlight render; inherent = sequencer + content |
| backbone | 1462 | 0 | **1071** | 391 | event_router/focus_manager/screen_state/modal_state/clock; inherent = scenario_events + game_mode |
| theme | 746 | 0 | **746** | 0 | token palettes + restyle = CSS variables free |
| animations | 401 | 0 | **401** | 0 | button morph + chip slide = CSS transitions free |
| temporal | 419 | 93 | **326** | 0 | domain = target-time calc; tax = timer + countdown render |
| engine | 1675 | **665** | 0 | 1010 | domain = EV/fold/side-pot/hand-eval; inherent = scenario gen + MT19937_64 |
| easter_egg | 64 | 0 | 0 | 64 | product content |
| **TOTAL** | **33762** | **1240** | **28735** | **3787** | |

### Headline (denominator = 33,780 src LOC)

| | LOC | % of src |
|---|---:|---:|
| **PLATFORM TAX** | **28,735** | **85.1 %** |
| DOMAIN (poker decision math) | 1,240 | 3.7 % |
| INHERENT (product logic) | 3,787 | 11.2 % |
| **domain : tax** | | **1 : 23** |
| (domain + inherent) : tax | | 1 : 5.7 |

**Honest split of the tax** (not all of it vanishes in a DOM app):
- **Pure substrate**, near-zero code in a DOM/TS app (rendering, boot glue, focus,
  events, timing, theming, asset load, audio, persistence plumbing, text-input render):
  **17,846 LOC = 52.8 % of src.**
- **UI chrome** (settings/modal/screens content) — a DOM app *still writes this*, just
  far more cheaply (JSX+CSS vs hand-drawn canvas): **10,889 LOC = 32.2 % of src.**

So the *recoverable* tax is less than 85 % — the ~53 % pure-substrate slice is the part
the browser would have given for free; the ~32 % chrome slice would shrink, not vanish.
Either way, **the furniture dwarfs the substance**: the poker math is 1,240 LOC.

> Note on your hook: the 1,604-LOC figure you cited as "the decision-math engine" is the
> `math/` dir (Z09 interrogator), which is **~70 % input-box rendering + keybinds (tax)**;
> only ~482 LOC of it is grading logic. The true pure math is `engine/` (665 LOC decision
> math + 1,010 scenario-gen). The substance is **smaller** than the number you quoted.

---

## Q6 — Top 5 sharpest "browser would have given me this free" line items

| # | Item | Paid (LOC) | Browser/DOM equivalent |
|---|---|---:|---|
| 1 | **Settings dialog** (`settings_modal` 1763 + `account_modal` 656 + `auth_form` 246 + fuzzy `search` 281) | **2,946** | `<form>`, `<input>`, `<select>`, `<details>`, `:focus`, native scroll; Fuse.js for search |
| 2 | **Leaderboard + modal framework** (`leaderboard_view` 1114 + `modal_base` 651) | **1,765** | `<dialog>` (free focus trap + backdrop), `<table>`, native scrollbars, `<input>` search |
| 3 | **Tutorial overlay** (`overlay.cpp` 1072 — lens dimming, spotlight cutout, callout panels, click interception) | **1,072** | any product-tour lib, or an absolute-positioned div + `box-shadow` spotlight + backdrop |
| 4 | **Math text inputs + keyboard** (`input_boxes` 440 + `keybinds` 389 + `bet_size_buttons` 154) | **983** | `<input type="number">` — free caret, selection, validation, mobile keyboard, IME, clipboard |
| 5 | **The focus seam** (`focus_registry` + `focus_reconcile` + `focus_manager`) | **588** | `tabindex` + `:focus-visible` + browser Tab order (does 100 % of it; ImGui native nav does ~13/16 — see NAV_AUDIT.md) |

Honorable mentions: `screen_transition` 266 + `ambient` 263 = CSS animations/transitions;
`gl_renderer` 243 = the DOM itself; the `idbfs.cpp` serializer ~230 = `JSON.stringify` +
`localStorage`; `outage_banner` 191 = a toast div.

---

## Q7 — What the C++23 actually bought (even-handed)

| Candidate | Real? | Assessment |
|---|---|---|
| **Deterministic MT19937_64 scenario gen + native-testable pure engine** | **Yes — the strongest win** | `engine/` is pure logic reconstructable from a 64-bit ID; it is covered by the bulk of the **11,867 LOC test suite** that compiles to **native binaries** and runs in CI **without a browser** (fast, headless, deterministic). A JS UI app testing scenario math through a DOM/headless browser is slower and flakier. This genuinely pays. |
| **Single-binary distribution** | Yes, modest | One `poker_trainer.wasm` + static assets, no server game logic, any static host. A JS bundle is also ~single-artifact, so the edge is small. |
| **Sealed-contract zone architecture (16-header DAG) + type safety across 34k LOC** | Partly | Real discipline and compile-time enforcement — but **much of it exists to manage complexity the platform tax created**. TS would give most of the type safety in a DOM app that is a fraction of the size. |
| **Performance** | Not the payoff | The reconcile is 2.5 ns, frame build ~16 µs — but a DOM app wouldn't have written most of it. Perf did not motivate this cost. |

**Honest verdict: the C++ payoff is real but smaller than the tax.** The defensible win
is *determinism + a pure engine tested natively at scale* (~1.7k LOC of engine exercised
by ~12k LOC of native tests). That is worth having — but it is ~**1.2–2.2k LOC of genuine
advantage against ~28.7k LOC of platform tax**. The same product as a C++/WASM engine
core (`engine/` + determinism, compiled to wasm and called from a TS/DOM UI) would have
kept the one real win and deleted most of the ~18k-LOC pure-substrate bill. Writing the
*whole* app layer in C++ bought type-safety-at-scale and a single binary, and paid for it
at roughly **23 LOC of furniture per LOC of poker math**.

---

## Raw data
`platform_tax.json` (per-zone classification + totals), `platform_tax.py` (the
classification + rule, re-runnable). LOC via `wc -l`; source citations inline above.
