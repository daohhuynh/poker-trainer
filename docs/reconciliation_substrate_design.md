# Focus / Input Reconciliation Substrate — Design Pass

**Status:** Design only. No code was changed in producing this document. This is the
written spec the human reviews before any build prompt is cut.

**Scope of the eventual build (NOT this pass):** a single primitive that owns, in one
place, two things Z09 and the Custom popup currently re-derive per surface:

1. **focus → ImGui ActiveId coupling** — `SetKeyboardFocusHere` when a *text field* is
   focused; `ClearActiveID` when a *non-text* element (slider/button) is focused.
2. **focused-element key dispatch** — Enter/Space (activate) and arrow keys (adjust) routed
   to the focused element's handler.

The healthy **mouse gate** (`platform.cpp::router_should_see_mouse`) and the keyboard-capture
gate are explicitly out of scope and must not be touched. (See §0.3 — the keyboard-capture
gate does not actually exist yet; this matters for staging.)

---

## 0. Ground truth — read the real code first

Before the design, three premises in the task prompt do **not** match the code on `main`.
They are load-bearing for the staged plan, so they are stated up front with evidence. None
of this blocks the design; it reshapes it.

### 0.1 `reconcile_imgui_focus` does not exist — the focus↔ActiveId half is unbuilt everywhere

The prompt names `src/math/input_boxes.cpp :: reconcile_imgui_focus` as "the working Z09
pattern this generalizes." There is no such function. Verified:

- `grep -rn reconcile_imgui_focus src/ tests/` → **no match**.
- `grep -rn "SetKeyboardFocusHere\|ClearActiveID" src/ tests/` → **no match anywhere**.

What Z09 actually has (`input_boxes.cpp`) is only the **focus-ring overlay**: `focus_on(id)`
(`is_keyboard_mode_active() && get_focused_element() == id`) gating an `AddRect` 2px
`BorderFocus` outline (`draw_numeric_box`, lines 207–231). The ActiveId-coupling half — the
substrate's job #1 — is implemented in **zero** surfaces. `input_boxes.cpp:216` says so
explicitly: *"keyboard char events are not yet fed into ImGui IO … so live typing lands once
that feed + a router keyboard gate … are wired."*

**Consequence:** the substrate does not *generalize* an existing reconcile; for job #1 it
*introduces* it. There is no working reference implementation to mirror.

### 0.2 Root and Mode Selection have no activate handler to "re-point"

The prompt lists `root_screen.cpp` / `mode_selection_screen.cpp` as having "the Space/Enter
activate handlers." They do not. Their only `register_key_handler` calls are **Escape**
(`root.escape`, `mode.escape`); the rest is **mouse** (`root.click`, `mode.click`). Verified:
the only Enter/Space activate dispatch in the whole tree is `custom_popup.cpp ::
on_custom_popup_activate` (lines 121–148).

ARCHITECTURE *does* specify the behavior — *"Enter on a focused element activates it"* for
both Root (line 13) and Mode Selection (line 25) — it is simply **unimplemented**. So
"re-pointing" Root/Mode is really **first-time implementation of a spec'd behavior via the new
substrate**, i.e. a deliberate behavior *addition*, not a no-op refactor. That changes how it
must be verified (you are confirming a new capability, not confirming nothing regressed).

Also note the key set: ARCHITECTURE uses **Enter** for activate everywhere; **Space** is
spec'd only to *toggle a focused checkbox* (Sign Up modal). The popup's `Space || Enter`
(line 124) is a superset of the spec. The substrate should standardize on **Enter = activate
(universal)**; treat **Space** as activate only where an element opts in (checkboxes, and the
popup's existing buttons if we choose to preserve current behavior). See open question Q3.

### 0.3 There is no keyboard-capture gate, and ImGui receives no keyboard at all

The prompt names `src/bridge/input_routing.cpp` as "the keyboard-capture gate." That file
does not exist. `grep -rn router_should_see_key` → the only hit is a **comment** in
`custom_popup.cpp:324` describing it as *future* work. What exists in `platform.cpp` is the
**mouse** gate only (`router_should_see_mouse`, line 85). Keyboard events
(`on_key_down`/`on_key_up`) are dispatched to the backbone router **unconditionally**, and
ImGui IO is fed **mouse only** — `grep` for `AddKeyEvent` / `AddInputCharacter` →
**none**. ImGui gets zero keyboard input today.

**Consequence (the big one):** `SetKeyboardFocusHere` only has an observable effect if ImGui
then receives characters to route into the focused field. With no keyboard feed, the
text-field half of job #1 is **inert** for *every* surface — typing into the Z09 boxes and
into the Custom popup inputs is equally dead today (both rely on an ImGui
`CallbackCharFilter` that never fires). The `ClearActiveID` half is *partially* meaningful now
(mouse-activated sliders/inputs set ActiveId → `WantCaptureMouse` true → gates the router; if
keyboard focus tabs away while ImGui keeps the slider active, the mouse gate can mis-read).
So job #1 splits into a buildable-now part (ClearActiveID) and a part blocked on a
prerequisite (SetKeyboardFocusHere + the keyboard feed/gate the prompt assumed exists).

### 0.4 Per-surface behavior today (the overlap map)

| Surface | Focus-ring render | ActiveId couple | Enter/Space activate | Arrow adjust | Notes |
|---|---|---|---|---|---|
| **Root** (`root_screen.cpp`) | `focus_on` + `ru::button(...focused)` | none | **none** (spec'd, unbuilt) | n/a | only Escape + mouse handlers |
| **Mode Sel** (`mode_selection_screen.cpp`) | `focus_on` + `ru::button` | none | **none** (spec'd, unbuilt) | n/a | only Escape + mouse handlers |
| **Custom popup** (`custom_popup.cpp`) | `draw_focus_ring(id)` ×8 | none | `on_custom_popup_activate` (Space\|Enter → Save/Reset/Play/X) | `on_custom_popup_arrow` (sliders L/R/U/D, inputs U/D → `step_weight`) | ModalLayer handlers, `state.open`-gated; explicitly tagged "SUBSTRATE SEAM" |
| **Z09 math** (`input_boxes.cpp`, `bet_size_buttons.cpp`, `keybinds.cpp`) | `focus_on` + `AddRect` (per box + bet-row bbox) | none | **Enter = submit-ALL override**, *not* activate-focused (`keybinds.cpp:103`) | **none** — arrows are no-ops by spec (ZONES Z09: *"Arrow keys within math input boxes are no-ops"*) | number keys 1–6 jump focus / 1–4 select bet tier — Z09-specific, **not** generic |

**The genuinely duplicated thing today is exactly one pattern: the focus-ring read+render**
(`focus_on`/`draw_focus_ring`, 4 files). The activate/adjust *dispatch* exists in **one**
place (the popup). The ActiveId couple exists **nowhere**.

**Critical scoping correction:** Z09 overlaps the substrate **only on job #1 (reconcile +
ring)**. It has no per-element activate (Enter is a global submit-all override) and no arrow
adjust (no-op by spec). The bet-size group's 1–4 selection and the 1–6 focus jumps are
Z09-local keybinds and must stay in `keybinds.cpp`. Do **not** route Z09 through the
activate/adjust dispatch.

---

## 1. The spec (six points)

### 1.1 Contract — what the substrate owns vs. what it leaves alone

**The substrate owns:**

- **(A) Render-time reconcile + ring** — a stateless, ImGui-dependent helper a surface calls
  while drawing each focusable. Reads `focus_manager` (the single source of truth) and:
  - if focused **and** text field → `ImGui::SetKeyboardFocusHere()` **before** the widget is
    submitted (focuses it for the next frame's char routing);
  - if focused **and** non-text → `ImGui::ClearActiveID()` (release any stale ImGui active
    widget so it stops capturing mouse/keyboard);
  - draw the 2px `BorderFocus` ring (the existing `focus_on` overlay), centralized.
- **(B) Event-time dispatch logic** — given a key event, look up the focused element
  (`get_focused_element()`) in a registry and invoke its `activate_fn` (Enter, and Space where
  opted in) or `adjust_fn` (arrows). This is the **body** of dispatch, exposed as a free
  function.
- **(C) The registry** — `focusable_id → { is_text_field, activate_fn, adjust_fn? }`, the data
  (A) and (B) both read.

**The substrate explicitly does NOT own (left exactly as-is):**

- The **mouse gate** `router_should_see_mouse` (`platform.cpp`) — healthy, untouched.
- The **keyboard-capture gate** — out of scope; it does not exist yet (§0.3). The substrate
  must not create or modify it. When it is eventually built (separate work), it must exempt
  the keys the substrate dispatches; that coordination is noted, not implemented here.
- **`focus_manager`** internals — the substrate is a *reader* of `get_focused_element()` /
  `is_keyboard_mode_active()` and a *caller* of `snap_focus_to`/`advance_focus` only through
  the existing public API. It adds no focus state and renders no focus indicator outside the
  centralized ring (which is still driven by `focus_manager` as the single source of truth,
  satisfying the backbone focus-render rule).
- **Router registration + priority/context** — each surface keeps its **own**
  `register_key_handler` at its **own** `HandlerPriority` and context predicate, and calls the
  substrate's dispatch function from inside. This is deliberate: the modal-over-screen
  arbitration (popup at `ModalLayer` above `mode.escape` at `ScreenContext`) is load-bearing
  and correct today; centralizing registration would put that arbitration at risk. The
  substrate centralizes the *dispatch body and the registry*, not the *who-outranks-whom*.
- **Z09's Enter-submit override and its 1–6 / 1–4 keybinds** — stay in `keybinds.cpp`.

### 1.2 Registry shape, population, and lifetime

```
struct FocusableHandlers {
    bool is_text_field;                                  // drives SetKeyboardFocusHere vs ClearActiveID
    std::function<bool()> activate_fn;                   // Enter (and Space if opted in); returns consumed
    std::function<bool(AdjustDir)> adjust_fn;            // arrows; empty => not adjustable
};
enum class AdjustDir : uint8_t { Up, Down, Left, Right };
// registry: ordered/hashed map FocusableId -> FocusableHandlers
```

Two viable population models; **recommend Model A**, note Model B:

- **Model A — context-scoped registration (recommended).** A surface registers its handlers
  at the same moment and with the same lifetime as its focus list. Root/Mode register
  alongside `register_focus_list(...)` (already done lazily in `screen_registration.cpp`
  `render_*_dispatch`); the popup registers on `push_custom_popup_focus()` and clears on
  `close_custom_popup`. Lifetime == the active focus context. This mirrors the existing
  focus-list lifecycle exactly and keeps the std::function closures' captured state valid for
  as long as the context is live (the surfaces' state objects — `ScreensRuntime`,
  `CustomPopupState`, `InterrogatorRuntime` — are app-lifetime, owned by boot, so closures
  never dangle). Z09 registers `is_text_field=true, activate_fn=empty, adjust_fn=empty` for
  its boxes (it needs only the reconcile half) and `is_text_field=false` for the bet group.

- **Model B — immediate-mode inline (alternative).** No separate registry; the render-time
  call carries the handlers (`reconcile(id, is_text_field, activate_fn, adjust_fn)`), and the
  substrate caches "the focused element's handlers as last rendered" for the event handler to
  use between frames. Idiomatic for ImGui, fewer moving parts, but introduces a one-frame
  coupling between render and the next key event (a real seam where bugs hide). Workable
  because focus only changes via keys/clicks that also re-render, but Model A's explicit
  lifetime is easier to reason about and to unit-test.

**Who populates:** each surface, in its existing install/entry path — no central registrar
that has to know about every screen.

### 1.3 Where the registry lives — Phase 0 amendment vs. bridge layer

This is the decision that needs the human. There is genuine tension:

- **CLAUDE.md §10** — "no global mutable state outside the backbone primitives that own it."
  A focus-dispatch registry is global mutable state and is conceptually a focus concern →
  argues for **backbone**.
- **CLAUDE.md §5** — `src/backbone/*.hpp` (incl. `focus_manager.hpp`) are **sealed Phase 0
  contracts**; adding to them needs explicit approval.
- **The render-time half (A) is ImGui-dependent** and therefore **cannot live in backbone at
  all** — backbone is ImGui-free (`focus_manager.cpp` includes no ImGui), and pulling
  `<imgui.h>` into a Phase 0 header would be a far larger contract change. Screen code already
  includes `<imgui.h>` directly (`input_boxes.cpp:15`, `custom_popup.cpp:11`), so the helper
  belongs in a zone allowed to use ImGui.

So the substrate must split by ImGui-dependence:

- **(A) render helper** → must live where ImGui is allowed. Not backbone.
- **(B)+(C) dispatch + registry** → ImGui-free; *could* be backbone (sealed) or a runtime-owned
  bridge object.

**Recommended home: the bridge layer (Z05), state owned by `BridgeRuntime`/boot, threaded by
reference.** Rationale:

- It avoids a Phase 0 contract change entirely.
- It is §10-compliant *without* a new free global: boot already owns app-root mutable state by
  reference (`g_runtime`, `g_boot`; `boot.cpp:46` explicitly cites §10 as the bridge being the
  app-root). A `FocusDispatch` member of `BridgeRuntime`, threaded by reference exactly like
  `ScreensRuntime`/`InterrogatorRuntime`, fits the established pattern.
- ImGui is already a bridge concern; the render helper can be a bridge header that screens
  include (they already include bridge headers — `game_launch.hpp`, `screen_dispatch.hpp`).

**The cost of the bridge home (must be surfaced):** it leans on a **Z07→Z05 and Z09→Z05
dependency that ZONES.md does not list.** ZONES.md says Z07 depends on 02/06/14 and Z09 on
01/06/14 — neither lists Zone 05. Yet the code *already* includes bridge headers from both
zones (so the edge is de-facto present but **undocumented**). Per CLAUDE.md §6 this is itself a
stop-and-ask. The clean fix is a one-line ZONES.md amendment adding Zone 05 to Z07's and Z09's
"Depends on" (or formalizing a small bridge "UI services" surface). That is a docs change, not
a sealed-contract change — strictly cheaper than amending Phase 0.

**Alternative home (if the human prefers backbone for (B)+(C)):** amend the sealed
`focus_manager.hpp`. Exact proposed addition, written for approval under CLAUDE.md §5/§14
(Phase 0 immutability / stop-and-ask):

```cpp
// ── PROPOSED §5 Phase-0 amendment to src/backbone/focus_manager.hpp ──
// (ImGui-FREE dispatch registry only; the render helper canNOT live here.)
#include <functional>

enum class AdjustDir : std::uint8_t { Up, Down, Left, Right };

struct FocusableHandlers {
    bool is_text_field{false};
    std::function<bool()> activate;            // Enter / opted-in Space; true == consumed
    std::function<bool(AdjustDir)> adjust;     // arrows; empty == not adjustable
};

// Associate handlers with a focusable id within the CURRENT focus context.
// Cleared by register_focus_list / push_focus_context / pop_focus_context so
// lifetime tracks the context (same lifecycle as the focus list itself).
void set_focusable_handlers(FocusableId id, FocusableHandlers handlers) noexcept;

// Dispatch helpers the per-surface router handlers call (they keep their own
// priority + context predicate; this is only the by-id body):
[[nodiscard]] bool dispatch_activate_to_focused() noexcept;        // returns consumed
[[nodiscard]] bool dispatch_adjust_to_focused(AdjustDir dir) noexcept;

// Query for the render helper living in a UI zone:
[[nodiscard]] bool focused_is_text_field() noexcept;
```

This is a real expansion of a frozen contract and of `focus_manager`'s responsibility (it would
now hold std::function callbacks, not just opaque ids). **Recommendation: do not do this.** Take
the bridge home + the cheap ZONES.md edge instead. Decision belongs to the human (Q1).

### 1.4 Re-point list — exactly what each surface drops/gains

**Custom popup** (`custom_popup.cpp`) — the cleanest true refactor:
- Drop the hand-wired by-id switches `on_custom_popup_activate` and `on_custom_popup_arrow`
  (and helpers `slider_delta` / `input_delta`). Keep `step_weight`/`solve_from` (the coupled
  solver — unrelated, stays).
- Drop `draw_focus_ring(id)`; replace each of the 8 `draw_focus_ring(...)` calls with the
  centralized render helper.
- On `push_custom_popup_focus`, register the 8 handlers: 4 buttons → `activate_fn` firing the
  same actions the mouse path fires (`save_weights` / `reset_to_saved` / launch+close /
  close); 2 sliders → `adjust_fn` = L/D dec, R/U inc via `step_weight`; 2 inputs →
  `adjust_fn` = U inc, D dec, `is_text_field=true`.
- Keep its **own** `register_key_handler` at `ModalLayer`, `state.open`-gated; its body now
  calls `dispatch_activate_to_focused()` / `dispatch_adjust_to_focused(dir)`. Escape handler
  unchanged. `just_opened` click-outside guard unchanged (mouse-gate territory, untouched).

**Root + Mode Selection** (`root_screen.cpp`, `mode_selection_screen.cpp`) — *new* behavior
via the substrate (§0.2): they have nothing to drop, they **gain** Enter-activate:
- Register each focus-list element's `activate_fn` to fire the *same* action as its existing
  mouse branch (`root.click` / `mode.click`): Play→`morph.start`, Custom→open popup,
  STANDARD/Aggressor/Caller→`emit_launch`, Home→`on_mode_selection_escape`/reload, etc. All
  non-text, `adjust_fn` empty.
- Add **one** `ScreenContext` key handler per screen whose body calls
  `dispatch_activate_to_focused()` (Enter). Replace the inline `focus_on(...)` ring calls with
  the centralized helper (or leave the existing `ru::button(...focused)` path and only add the
  helper for ActiveId — see Q4: these screens draw through the background draw list, not ImGui
  windows, so they may need only the ring, not ActiveId coupling).

**Z09 math** (`input_boxes.cpp`, `bet_size_buttons.cpp`) — reconcile/ring **only**:
- Replace `focus_on`+`AddRect` in `draw_numeric_box` and `render_bet_size_group` with the
  centralized helper. Boxes: `is_text_field=true`. Bet group: `is_text_field=false` (single
  tab stop; ring around the row bbox stays).
- **No** activate/adjust registration. `keybinds.cpp` (Enter submit-all, 1–6, 1–4) is
  untouched. This is the surface where the SetKeyboardFocusHere effect is currently inert
  (§0.3) — it becomes live only when the keyboard feed lands, with no further Z09 change.

### 1.5 Staged build plan — each stage independently browser-verifiable

Ordering is chosen so the riskiest seam (the live focus↔ImGui boundary) is approached last and
behind a feature that already works by mouse, and so the keyboard-feed blocker (§0.3) does not
gate early stages.

- **Stage 0 — ZONES.md / contract decision (no code).** Human resolves Q1 (bridge vs. backbone
  home) and Q2 (ZONES.md edge). Nothing builds until the home is chosen.

- **Stage 1 — primitive + registry standalone, unit-tested.** Build (B)+(C): the registry and
  `dispatch_activate/adjust_to_focused` against a fake focus context. Pure, ImGui-free,
  native-testable (`build-test/`, googletest). No surface re-pointed yet. **Verifiable in
  isolation; nothing in the app changes.**

- **Stage 2 — render helper (A), wired to ONE surface (Z09 ring).** Centralize the focus-ring
  for Z09 boxes + bet group through the helper, ActiveId-coupling included but with
  SetKeyboardFocusHere understood-inert (§0.3). No behavior change expected vs. today (ring
  looks identical). First browser checkpoint: Z09 still renders, Tab still rings each box, bet
  group still rings its bbox.

- **Stage 3 — re-point the Custom popup (true refactor).** Swap the popup's two by-id switches
  for substrate dispatch and its 8 `draw_focus_ring` for the helper. This is the highest-value
  de-dup and the one with a *working* reference behavior to diff against. Browser-verify
  arrow-adjust and Enter/Space-activate behave **byte-for-byte** as before.

- **Stage 4 — Root/Mode Enter-activate (new capability).** Register activate handlers + one
  `ScreenContext` dispatch handler each; centralize their ring. Browser-verify the *new*
  Enter-activate against ARCHITECTURE lines 13/25.

- **Stage 5 (separate track, blocked, NOT this substrate) — keyboard feed + capture gate.**
  When the ImGui `AddKeyEvent`/`AddInputCharacter` feed and `router_should_see_key` gate are
  built, the SetKeyboardFocusHere half of Stage 2/3 becomes live with no substrate change.
  Flagged here only so the dependency is explicit; it is out of this substrate's scope and is
  its own prompt.

Each stage compiles + links clean (`-Werror`) and passes prior stages' tests before the next
starts (CLAUDE.md §8 integration gates).

### 1.6 Verification per stage — frame traces + human browser checks

**Stated plainly: unit-green proves nothing about this seam.** The focus↔ImGui boundary lives
between `focus_manager`, ImGui's `ActiveId`, the DOM event feed, and the per-frame render
order. None of that is exercised by a native googletest. A passing unit run on Stage 1 means
only that the by-id dispatch logic is correct in isolation — it says nothing about whether a
real Tab+Enter in the browser activates the right button. (This is also where a prior window
reportedly hallucinated a passing test; treat green as necessary-not-sufficient.)

- **Stage 1 (unit, the only fully-unit-coverable stage):** tests for `dispatch_*_to_focused`
  given a focused id with/without handlers, adjust on a non-adjustable element returns false,
  activate on an element with no `activate_fn` returns false, registry cleared on context
  pop. **These prove the logic, not the seam.**

- **Frame traces to write (Stages 2–4):** a per-frame debug log (behind a compile flag, dumped
  to the browser console) emitting, each frame: `get_focused_element()`, whether the helper
  called `SetKeyboardFocusHere`/`ClearActiveID`, ImGui `ActiveId` / `WantCaptureMouse` after
  the frame, and which dispatch branch fired on the last key event. The trace is what makes the
  seam observable; assertions over a captured trace are the closest thing to a regression test
  here.

- **Human browser checks (the real gate), per stage:**
  - **S2 (Z09 ring):** Tab through the boxes — ring moves box→box, identical to pre-change;
    bet group shows one bbox ring; no flicker; mouse-click a box still rings it. (Typing is
    *expected dead* per §0.3 — do not treat dead typing as a regression here.)
  - **S3 (popup):** open Custom; Tab through all 8; on each slider, arrows move it and the
    coupled % updates (sum stays 100); on each input, Up/Down step by 1; Enter/Space on
    Save/Reset/Play/X do exactly what the mouse does; Escape and click-outside still dismiss;
    open-frame self-dismiss does **not** happen (the `just_opened` guard). Diff every behavior
    against the pre-S3 build.
  - **S4 (Root/Mode):** Tab to each element, press Enter — Play morphs, Custom opens the popup,
    STANDARD/Aggressor/Caller launch, cluster/Home behave per ARCHITECTURE 13/25. This is *new*
    behavior; verify it exists and matches spec (not "unchanged").
  - **Cross-cutting each stage:** confirm the mouse gate is untouched — a click on an open
    popup never reaches the screen beneath; screen buttons (background draw list) still click.

---

## 2. Open questions for the human (resolve before any build prompt)

- **Q1 — Home of the registry/dispatch.** Recommended: **bridge** (`BridgeRuntime`-owned,
  threaded by reference; no Phase 0 change). Alternative: amend sealed `focus_manager.hpp`
  (exact text in §1.3) — not recommended. Which?
- **Q2 — ZONES.md dependency edge.** The bridge home formalizes an already-present-but-
  undocumented Z07→Z05 / Z09→Z05 include. OK to add Zone 05 to Z07's and Z09's "Depends on"
  (a docs edit), or do you want a different structural home (e.g. a small shared UI-services
  surface)?
- **Q3 — Activate key set.** Standardize on **Enter = activate** (ARCHITECTURE) and keep the
  popup's extra **Space** only where it already exists / for checkboxes? Or make Space a
  universal activate (superset of spec)?
- **Q4 — Do background-draw-list screens (Root/Mode) need ActiveId coupling at all?** They draw
  via `GetBackgroundDrawList`, not ImGui windows/items, so they likely need only the ring + the
  Enter dispatch, never `SetKeyboardFocusHere`/`ClearActiveID`. Confirm the helper may be
  ring-only for these (ActiveId coupling applies only to real ImGui widgets — Z09 boxes, popup
  inputs/sliders).
- **Q5 — Stage 5 sequencing.** The text-field half of job #1 is inert until the keyboard feed +
  capture gate exist (§0.3). Build the substrate now with that half dormant (recommended,
  staged above), or sequence the keyboard feed first so Stage 2/3 are fully live and
  verifiable end-to-end?
