# Focus-Reconciliation Seam — Benchmark & Correctness Report

Subject: the mechanism that reconstructs retained-mode focus semantics (persistent
tab focus, modal traps, nested focus contexts) on top of Dear ImGui's immediate mode.
All numbers below are measured against the **real shipping source files** compiled
as-is. No reconcile logic was reimplemented. Where a thing could not be measured it
is stated explicitly (see §"What I could not measure").

Environment (raw: `environment.json`): Apple M4, arm64, macOS 15.6.1 (24G90), Apple
clang 17.0.0, C++23, ImGui 1.91.9b, native bench objects at `-O2 -DNDEBUG` (no LTO,
so the cross-translation-unit call boundary the app actually has is preserved).

---

## 1. Architecture ground-truth (Step 0) — corrections called out

### 1a. The per-frame reconciliation pass — exact functions

The seam is two cooperating pieces:

| Role | Function (signature) | Location |
|---|---|---|
| Per-frame entry point (called once per surface per frame) | `FocusReconcile begin_focus_reconcile(const FocusRegistry&, backbone::FocusableId last_synced)` | `src/bridge/focus_reconcile.cpp:19` |
| Pure reconcile decision (registry scan + branch) | `FocusReconcile decide_focus_reconcile(const FocusRegistry&, backbone::FocusableId prev, backbone::FocusableId current) noexcept` | `src/bridge/focus_registry.cpp:40` |
| Focus-state read gate | `backbone::FocusableId active_focus_or_none() noexcept` | `src/bridge/focus_registry.cpp:62` |
| Focus state machine (Tab/wrap/push-pop/trap) | `advance_focus / snap_focus_to / push_focus_context / pop_focus_context / get_focused_element` | `src/backbone/focus_manager.cpp` |
| Per-element render glue (ImGui-only) | `grab_keyboard_if_target`, `draw_focus_ring[_rect]` | `src/bridge/focus_reconcile.cpp:36,61,71` |

`begin_focus_reconcile` = `decide_focus_reconcile(registry, last_synced, active_focus_or_none())`
plus **one** `#ifdef __EMSCRIPTEN__` line (`ImGui::ClearActiveID()` gated on
`io.WantTextInput`). Natively that line is compiled out; the rest — the actual
reconciliation algorithm — is identical native vs. wasm.

### 1b. Zones and sealed headers — **CORRECTION**

> Your description: "~14 zones behind ~7 sealed interface headers."

- Zones: **exactly 14** (Z01–Z14) plus **Phase 0**. ✔ (ZONES.md).
- Sealed interface headers: **16, not ~7.** Phase 0 "Owns" seals 16 contract headers
  (ZONES.md lines 10–25; CLAUDE.md §5 "Phase 0 immutability"). The DAG in §Step 5 is
  built over exactly these 16.

### 1c. Modal-trap isolation & push/pop stacking — located

- Trap + stacking: `push_focus_context()` / `pop_focus_context()` —
  `src/backbone/focus_manager.cpp:105` and `:123`. A modal push saves the current
  `FocusContext` onto `g_context_stack` and installs the modal's list **armed** on its
  initial focus; `advance_focus`/`snap_focus_to` operate only on `g_active_context`, so
  focus physically cannot address a base-context element while the modal is open. Pop
  restores the prior context *with its armed state and pointer intact*.
- Modal existence/stack (separate concern): `src/backbone/modal_state.cpp` — fixed
  depth-8 `ModalId` stack, depth mirrored into an atomic for the audio thread.

### 1d. Activation-key semantics — **CONFIRMED**

> Your belief: Space-OR-Enter on every screen EXCEPT Game, where it's Space-only
> because Enter is math submit / tier-advance.

**Correct, verbatim in code:**
- Generic dispatch treats Space and Enter identically → activate
  (`dispatch_focus_key`, `src/bridge/focus_registry.cpp:73–77`).
- Screen button handlers accept both (`root_screen.cpp:192`,
  `mode_selection_screen.cpp:160`, `post_round_screen.cpp:604`,
  `tutorial_complete_screen.cpp:213`, `modal_base.cpp:530`, `shop_view.cpp:328`,
  `leaderboard_view.cpp:554,961`).
- **Game exception**, `src/modal/modal_base.cpp:576–578`:
  ```cpp
  if (g_runtime->cluster.screen == ClusterScreen::Game && enter) {
      return false;  // Game cluster is Space-only
  }
  ```
- Enter on the Game screen is owned by the math zone
  (`src/math/keybinds.cpp:225` `on_enter_key` → `do_submit` / `advance_tier`).

### 1e. LOC split (raw counts)

Reconciliation seam = **588 LOC** across 5 files:

| Component | Files | LOC |
|---|---|---|
| Bridge substrate (registry + reconcile decision + dispatch + ImGui glue) | `focus_registry.hpp/.cpp`, `focus_reconcile.cpp` | 326 |
| Backbone state machine (`focus_manager`) | `focus_manager.hpp/.cpp` | 262 |

Whole codebase: **src 33,780 LOC / 244 files; tests 11,867 LOC / 76 files.** By zone dir:
backbone 1462, engine 1675, assets 1127, audio 1214, persistence 3605, bridge 5889,
theme 746, screens 3682, math 1604, temporal 419, modal 3869, settings 3996, tutorial
1970, render 2039, animations 401, easter_egg 64.

---

## 2. Results tables (Steps 1–7)

### Step 1 — Reconciliation pass latency (native)

Harness: `reconcile_bench.cpp` (written by me). Drives the **real** `begin_focus_reconcile`.
Batched timing (see §Harness disclosure). Two real per-frame cases; raw: `reconcile_bench.json`.

| Case | mean | median | p90 | p99 | p99.9 | min | max | stddev |
|---|---|---|---|---|---|---|---|---|
| **steady frame** (focus unchanged → `None`; the ~99% case) | **2.512** | 2.441 | 2.523 | 3.418 | 8.301 | 2.359 | 58.51 | 0.440 |
| focus-change frame (→ `YieldKeyboard`, full 3-elem scan) | 3.006 | 2.930 | 3.254 | 4.070 | 9.359 | 2.766 | 60.38 | 0.507 |

All values **nanoseconds per call**. 200,000 batch-samples × 512 = **102,400,000 real
calls** per case, after a 10,000-batch warmup. (Native vs. wasm: the one ImGui
`ClearActiveID` line is compiled out — see §Harness disclosure.)

### Step 2 — Allocation behavior on the reconcile path

Harness: global `operator new/new[]` override (`alloc_counter.hpp`), tracked around
1,000,000 real `begin_focus_reconcile` calls.

| Metric | Value |
|---|---|
| Allocation count | **0** |
| Bytes allocated | **0** |
| Per-call | 0.000000 |

Confirmed zero-allocation on the hot path. Allocation happens only in
`FocusRegistry::register_element` (a `vector::push_back`), which runs at **surface
registration**, not per frame.

### Step 3 — Scaling sweep (registry size)

Harness: `time_decide_scaling` in `reconcile_bench.cpp`, real `decide_focus_reconcile`,
worst case (focus on the last-registered element → full `find()` scan). 100,000
batch-samples × 512 each. Raw: `scaling_sweep.csv`.

| n elements | mean (ns) | median (ns) | p99 (ns) |
|---|---|---|---|
| 1 | 0.941 | 0.896 | 1.221 |
| 4 | 1.629 | 1.547 | 2.197 |
| 8 | 3.053 | 2.522 | 5.047 |
| 16 | 4.940 | 4.883 | 6.918 |
| 32 | 8.678 | 8.545 | 12.045 |
| 64 | 18.725 | 15.707 | 68.196 |
| 128 | 35.446 | 34.748 | 48.992 |

**Empirical complexity: O(n)** — a linear registry scan. Least-squares:
`median ≈ 0.32 + 0.263·n` ns (R²=0.997); `mean ≈ 0.61 + 0.273·n` ns (R²=0.999) — i.e.
≈0.26 ns per element compared plus ≈0.3–0.6 ns fixed overhead.
Doubling n doubles the cost (4.88→8.55→(15.71)→34.75 across n=16→128). This matches the
implementation: `FocusRegistry::find()` is a linear `std::vector` scan. Real surfaces
hold ≤ ~10 stops, so real cost sits near the n=1..8 rows (~1–3 ns). (n=8 and n=64 rows
carry a few scheduler-preemption outliers that inflate their p99/max/stddev; medians
are stable.)

### Step 4 — Frame-budget context

Harness: `frame_context.cpp` (written by me). Real ImGui 1.91.9b `NewFrame → build →
Render` under a **null backend**. Raw: `frame_context.json`.

| Quantity | Value |
|---|---|
| Representative "game-like" ImGui frame build | **15,826 ns** (15.83 µs) mean; median 15.83 µs; p99 20.3 µs |
| Minimal ImGui frame build | 1,055 ns (1.06 µs) |
| Reconcile (steady, 2.512 ns) / game-like frame | **0.0159 %** |
| Reconcile / minimal frame | 0.238 % |
| Reconcile / 16.67 ms (60 fps) budget | **0.0000151 %** |
| Game-like frame / 16.67 ms budget | 0.095 % |

The reconcile pass is ~**1/6,300** of one representative UI-build frame and ~**1/6.6
million** of a 60 fps frame budget. It is negligible. ⚠ The "game-like frame" is a
**representative proxy, not the app's exact frame** — see §Harness disclosure / §What I
could not measure.

### Step 5 — Static dependency structure (sealed interface headers)

Tool: `step5_dag.py` (written by me). Parses `#include "…"` among the 16 sealed Phase 0
headers, Kahn topological sort + cycle detection. Raw: `dependency_graph.json`.

| Metric | Value |
|---|---|
| Nodes (sealed interface headers) | **16** |
| Edges (A includes B, both in set) | **10** |
| Acyclic | **Verified true** — Kahn's algorithm emitted all 16 nodes (0 left in a cycle) |
| Max dependency depth (longest include chain) | **3** (`focus_manager → event_router → screen_state → scenario_id`) |
| Out-of-set project includes among the 16 | 0 (they include only each other + stdlib) |

Edges: `event_router→screen_state`, `scenario_events→scenario_id`,
`screen_state→scenario_id`, `focus_manager→{event_router, screen_state}`,
`rng_seed→scenario_id`, `settings→theme_tokens`, `asset_paths→tier_config`,
`persistence_schema→{scenario_id, sync_state}`. Topological order (prereqs first):
tier_config, audio_paths, animation_clock, modal_state, scenario_id, auth0_config,
sync_state, theme_tokens, asset_paths, scenario_events, screen_state, rng_seed,
persistence_schema, settings, event_router, focus_manager. **Acyclicity is a verified
property, not a claim.**

### Step 6 — Focus correctness matrix

Harness: `focus_matrix.cpp` (written by me). Drives the **real** `focus_manager` +
`dispatch_focus_key`. Raw: `focus_matrix.json`. Coverage: `coverage.json` (via
`cov_driver.cpp` + llvm-cov).

| # | Test | Category | Mode | Result |
|---|---|---|---|---|
| 1 | tab forward lands 1→2→3 | traversal | executed | PASS |
| 2 | wraparound forward (end→start) | wraparound | executed | PASS |
| 3 | tab backward lands last→prev | traversal | executed | PASS |
| 4 | wraparound backward (start→end) | wraparound | executed | PASS |
| 5 | fresh context unarmed → no focus | traversal | executed | PASS |
| 6 | modal-trap: focus cannot escape | modal-trap | executed | PASS |
| 7 | modal-trap: depth == 1 | modal-trap | executed | PASS |
| 8 | modal-trap exit restores prior focus | modal-trap | executed | PASS |
| 9 | nested push/pop restores each level | nested-context | executed | PASS |
| 10 | Space activates | activation-key | executed | PASS |
| 11 | Enter activates (generic) | activation-key | executed | PASS |
| 12 | arrows adjust, not activate | activation-key | executed | PASS |
| 13 | non-dispatch key (Tab) not consumed | activation-key | executed | PASS |
| 14 | Game cluster Space-only, Enter reserved | activation-key | **source** | PASS |
| 15 | generic screens Space-OR-Enter | activation-key | **source** | PASS |
| 16 | Game Enter reserved for math submit | activation-key | **source** | PASS |

**16/16 pass** — 13 executed behavioral + 3 source-verified (the Game Space-only rule
lives in a TU-static `on_cluster_key` not independently invocable natively; those rows
read the real source and assert the guard clauses — tagged `source`, see disclosure).

Coverage of the seam's pure logic (llvm-cov, real source files, all branches driven):

| File | Line cov | Function cov | Branch cov |
|---|---|---|---|
| `backbone/focus_manager.cpp` | 91.55 % | 100 % | 66.67 % |
| `bridge/focus_registry.cpp` | 98.57 % | 100 % | 95.65 % |
| `bridge/focus_reconcile.cpp` | 45.45 % | 25 % | – |
| **Total** | **91.45 %** | 85.71 % | 85.71 % |

`focus_reconcile.cpp`'s low number is its **ImGui render glue** (`grab_keyboard_if_target`,
`draw_focus_ring[_rect]`, the `__EMSCRIPTEN__` branch) — compiled out / no-op natively,
browser-verified per CLAUDE.md §9, not unit-testable off-browser.

### Step 7 — WASM deployment metrics

Raw: `wasm_metrics.json`. Local `build-wasm/poker_trainer.wasm` is **byte-identical** to
what production serves (identity `content-length` = 1,449,593 from
`poker-trainer-pearl.vercel.app`).

| Metric | Value |
|---|---|
| Emscripten | **5.0.7** |
| Build type / opt | Release, **-O3** (compile and link) |
| LTO / closure | none / none |
| `ASSERTIONS` | **=1 (on)** — unusual for a size-min prod build; costs bytes |
| Key link flags | `-sMIN/MAX_WEBGL_VERSION=2 -sFULL_ES3=1 -sALLOW_MEMORY_GROWTH=1 -sEXIT_RUNTIME=0 -sASSERTIONS=1 -sEXPORTED_RUNTIME_METHODS=HEAP… -lidbfs.js` |
| **Raw** wasm | 1,449,593 B (1.38 MiB / 1.45 MB) |
| **gzip -9** (local) | 568,726 B (555.4 KiB) |
| gzip (production on-wire) | 572,631 B (559.2 KiB) |
| brotli (production on-wire, Vercel dynamic) | 567,597 B (554.2 KiB) |
| **brotli -q11** (local static, best case) | 466,431 B (455.5 KiB) |
| JS glue (`poker_trainer.js`) | 158,936 B raw / 43,734 B gzip |

**CORRECTION to the 477 KB "gzipped" figure:** it is **not** the gzip size (real gzip ≈
**569 KB / 555 KiB**). 477 KB is in the neighborhood of the **best-case static
brotli-q11** size (measured **466 KB / 455 KiB**), off by ~2 %, likely an earlier build
revision. More importantly, **production isn't actually shipping that**: Vercel serves
the wasm with low-quality on-the-fly compression, so today's real transfer size is
**≈ 554–567 KiB** (brotli and gzip come out nearly equal on the wire). To actually hit
~466 KB you'd need static q11 precompression at deploy time.

---

## 3. Harness disclosure (what I wrote, the loops, the measurement points, real-vs-stub)

Every harness below was **written by me** and lives in `bench_results/`. Each links and
calls the **real shipping functions** — no reconcile logic is reimplemented.

### Steps 1–3 — `reconcile_bench.cpp`
Compiled by `build_bench.sh`, which builds the real `src/bridge/focus_registry.cpp`,
`src/bridge/focus_reconcile.cpp`, `src/backbone/focus_manager.cpp` at `-O2 -DNDEBUG`
(no LTO) and links the harness. The frame-driving/timing loop and measurement points:

```cpp
for (std::size_t i = 0; i < n_samples + warmup; ++i) {
    const auto t0 = Clock::now();                       // <-- measurement point
    for (int b = 0; b < kBatch; ++b) {                  // batch of 512 real calls
        const br::FocusReconcile r =
            br::begin_focus_reconcile(reg, last_synced); // <-- THE REAL per-frame entry
        sink += static_cast<std::uint64_t>(r.action) + r.target.value; // consume
    }
    const auto t1 = Clock::now();                       // <-- measurement point
    do_not_optimize(sink);
    if (i >= warmup) samples.push_back(elapsed_ns / kBatch);
}
```
Step 2 wraps 1,000,000 real calls with `bench::g_alloc_track = true` (global
`operator new` counter). Step 3 swaps `begin_focus_reconcile` for the real
`decide_focus_reconcile(reg, prev, current)` over registries of size n.

**Confirms real function:** `begin_focus_reconcile` / `decide_focus_reconcile` are the
production symbols from `src/bridge/`, not copies.
**Batching rationale:** the Apple-Silicon monotonic clock ticks at ~41.7 ns and cannot
resolve a single ~2 ns call; each *sample* is a 512-call batch mean, percentiles are
over the batch-mean distribution, total calls ≫ 100k.
**Stub disclosure:** natively, one line of `begin_focus_reconcile` (`ImGui::ClearActiveID()`,
behind `#ifdef __EMSCRIPTEN__`, gated on `io.WantTextInput`) is compiled out. That is a
single constant-time ImGui call, **not** part of the reconcile algorithm, and
un-measurable without a live browser/GL context. Everything else measured is identical
to the wasm build. Nothing else was stubbed.

### Step 4 — `frame_context.cpp`
Real ImGui 1.91.9b, null backend. Frame loop / measurement points:

```cpp
ImGui::NewFrame();
build();            // representative game-like surface (real ImGui widgets + draw list)
ImGui::Render();    // produces ImDrawData — the per-frame CPU UI-build work
// timed around NewFrame..Render with Clock::now(); frames are µs, resolvable directly
```
**Stub disclosure (important):** this is **not the poker-trainer app's exact frame.** The
app's real render layer (screen render hooks, in-house WebGL2 renderer, texture binds,
CDN asset draws) is compiled behind `#ifdef __EMSCRIPTEN__` and needs a live GL context
+ loaded assets; `src/main.cpp` natively is an empty `return 0`, so there is no runnable
native full-frame app. I therefore built a representative Game-screen-shaped surface
**with the same ImGui library the app renders through** (a fullscreen window, HUD text,
3 `InputText`, a 6-button row, ~200 draw-list primitives for table/chips/cards). It is a
same-library, same-order-of-magnitude proxy used only to contextualize the reconcile
cost. The reconcile number itself (Steps 1–3) is the real function. The `-O0`
software font-atlas + dummy `TexID` are the only setup stubs and are outside the timed
region.

### Step 6 — `focus_matrix.cpp` (+ `cov_driver.cpp` for coverage)
Drives the real `backbone::focus_manager` and `bridge::dispatch_focus_key`; asserts on
observable state (`get_focused_element`, `context_depth`, hook side effects). Pattern:

```cpp
register_base(); bb::advance_focus(false);           // REAL focus_manager
check("tab_forward…", "traversal", "executed", bb::get_focused_element() == A);
bb::push_focus_context(modal, X, "modal");           // REAL modal trap
// … assert focus stays in {X,Y}, never A/B/C …
```
**Source-verified rows (14–16):** the Game "Space-only, Enter reserved" rule is in
`on_cluster_key`, a TU-static in `modal_base.cpp` fused to ImGui + a file-static runtime
— not invocable natively. Those rows `file_contains_all(...)` the real source and assert
the guard clauses (`"ClusterScreen::Game && enter"`, `"Game cluster is Space-only"`,
etc.). They are tagged `mode:"source"` and counted separately (3/3) so they are never
conflated with the 13 executed behavioral tests.

---

## 4. What I could not measure, and why

1. **The app's real end-to-end per-frame render cost (Step 4).** The render layer is
   `#ifdef __EMSCRIPTEN__`-gated and requires a live WebGL2 context + loaded CDN assets;
   `main.cpp` natively is `return 0`. Measured a same-library ImGui proxy frame instead
   (disclosed). A true number would require an in-browser profiler (`performance.now`),
   which has ~coarse timer resolution and clamping — the very reason the reconcile
   micro-benchmark is done natively.
2. **The ImGui-side reconcile glue on the hot path** (`ClearActiveID`,
   `SetKeyboardFocusHere`, `AddRect`). These are `__EMSCRIPTEN__`-only, no-ops natively,
   browser-verified per CLAUDE.md §9. They are O(1) constant ImGui calls, not part of
   the reconcile algorithm.
3. **The Game "Space-only" activation rule as an executed test** — `on_cluster_key` is a
   TU-static entangled with ImGui/g_runtime; verified by source assertion instead
   (Step 6 rows 14–16).
4. **Absolute wasm compression the user pays in production at q11** — production (Vercel)
   applies dynamic low-quality compression, so the on-wire size (~554–567 KiB) is larger
   than the local static brotli-q11 best case (466 KiB). Measured both; cannot change
   what the CDN chooses to do at request time from here.

---

## 5. Raw data files (all under `bench_results/`)

| File | Contents |
|---|---|
| `environment.json` | CPU/OS/compiler/ImGui/timing method + measured-function map |
| `reconcile_bench.json` | Step 1 distributions + Step 2 allocation + Step 3 scaling (JSON) |
| `scaling_sweep.csv` | Step 3 raw series (n vs ns) |
| `frame_context.json` | Step 4 frame-build distributions + budget percentages |
| `dependency_graph.json` | Step 5 nodes/edges/topo order/acyclicity |
| `focus_matrix.json` | Step 6 per-test matrix + executed/source split |
| `coverage.json` | Step 6 llvm-cov summary of the seam source files |
| `wasm_metrics.json` | Step 7 sizes / toolchain / flags / 477 KB correction |
| **Harness source (written for this report):** | |
| `reconcile_bench.cpp`, `alloc_counter.hpp`, `build_bench.sh` | Steps 1–3 |
| `frame_context.cpp` | Step 4 |
| `step5_dag.py` | Step 5 |
| `focus_matrix.cpp`, `cov_driver.cpp` | Step 6 |
| **Built binaries:** `reconcile_bench`, `frame_context`, `focus_matrix`, `seam_cov` | runnable |
