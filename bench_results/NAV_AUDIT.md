# Capability Audit — Custom Focus Seam vs. Dear ImGui Native Keyboard Nav

ImGui 1.91.9b (vendored). Native nav read from source; the matrix and the two
"uncomfortable" claims were **driven headlessly against real ImGui nav** (null
backend, key events fed into `io.AddKeyEvent`, nav state read from
`IsItemFocused`/`io.NavVisible`/`GImGui->NavId`). Harnesses: `nav_probe.cpp`,
`nav_customdraw_probe.cpp`. Raw: `nav_probe.json`, `nav_customdraw_probe.json`. This
report does not flatter the custom code.

---

## STEP A — Native keyboard-nav API surface (source-cited)

| Question | Answer | file:line |
|---|---|---|
| **Nav order?** | Two systems. **Tabbing** (Tab/Shift-Tab) = **submission order**, and is *always on regardless of `NavEnableKeyboard`*. **Directional** (arrows) = **spatial scoring** of item rects. | tabbing `imgui.cpp:12685` (`NavProcessItemForTabbingRequest`) + always-on note `13344`; directional scoring `12434` (`NavScoreItem`) |
| **Override the order?** | Only by reordering submission, or `ImGuiItemFlags_NoTabStop` to drop an item. There is no explicit "tab index." | `12704` (`can_stop`, `NoTabStop`) |
| **What activates? Per-widget/scope configurable?** | **Space** → `NavActivate` (PreferTweak); **Enter/KeypadEnter** → `NavActivate` (PreferInput). **Global and hardcoded** — no public per-widget/per-scope activation-key set. | `imgui.cpp:13057-13060` |
| **Can you reserve a key (e.g. Enter) from nav?** | Yes, via the **key-ownership** system: nav reads keys with `ImGuiKeyOwner_NoOwner`, so `SetKeyOwner(ImGuiKey_Enter, my_id)` makes `IsKeyPressed(Enter, NoOwner)` false → nav ignores Enter. | gate at `13057-13060` (`ImGuiKeyOwner_NoOwner`) |
| **BeginPopupModal → nav scope?** | The modal becomes `g.NavWindow`; nav only scores items in the nav window (+ flattened children), so background windows are unreachable = a **trap**. | scoring confined to `g.NavWindow` `13218,13236`; modal focus `SetItemDefaultFocus` `8483` |
| **Save/restore nav on open/close? Nesting?** | Yes. Nav position is stored per-window in `NavLastIds[]`; closing a popup refocuses the parent and **restores `g.NavId = window->NavLastIds[0]`**. Popups nest via the open-popup stack. | restore `12908`, `12259`; `SetFocusID` writes `NavLastIds` `12403` |
| **"Not visible until first keypress" (focus-visible)?** | Yes, this is the default. `ConfigNavCursorVisibleAuto = true`; `NavCursorVisible` starts false, turns true after a nav move, false on mouse use. Exposed as `io.NavVisible`. | default `1451`; on-move `12339-12343`; on-mouse-off `12409-12410`,`4945-4946`; output `13047` |
| **Does clicking sync nav position?** | Yes. Activating/clicking an item calls `SetFocusID` → `g.NavId = id` (highlight stays hidden in mouse mode); the next Tab resumes from there. | `SetFocusID` `12389-12410` |
| **Is nav state authoritatively queryable/settable by the app?** | **Read:** `io.NavActive`, `io.NavVisible` (public), `IsItemFocused()` per item (public). **Set:** `SetKeyboardFocusHere(offset)` `8450`, `SetItemDefaultFocus()` `8483`, `FocusItem()` `8423`, `ActivateItemByID(id)` `8441` — all **submission-time**. The authoritative `g.NavId` and `SetNavID()` are **internal** (`imgui_internal.h`), not public. | as cited |

**Takeaway:** native nav is a full focus engine. Its one real ergonomic gap for an
app is that focus is steered *at submission time* (by position/offset), and the
authoritative focus id (`g.NavId`) is internal — there's no public "the focused
element is app-id X" handle.

---

## STEP B — 16-case matrix vs. native nav (headless, measured)

Classes are strict: `NATIVE-NO` only if the **public** API genuinely cannot get there.

| # | Behavior (from `focus_matrix.cpp`) | Class | Evidence |
|---|---|---|---|
| 1 | tab forward 1→2→3 | **NATIVE-YES** | measured: `b0,b1,b2` |
| 2 | wraparound forward (end→start) | **NATIVE-YES** | measured: `b2→b0` (NavTabbingResultFirst) |
| 3 | tab backward last→prev | **NATIVE-YES** | measured: `b0→b2→b1` |
| 4 | wraparound backward (start→end) | **NATIVE-YES** | measured: `b0→b2` |
| 5 | fresh context: nothing focused until first key | **NATIVE-YES** | measured: `NavVisible=false` at start (ConfigNavCursorVisibleAuto) |
| 6 | modal-trap: focus cannot escape | **NATIVE-YES** | measured: focus stays in `{m0,m1}`, never `b0..b2` |
| 7 | modal depth == 1 | **NATIVE-YES (equiv.)** | native has the open-popup stack / `GetTopMostPopupModal`; depth is a stack query, not a focus behavior |
| 8 | modal exit restores prior focus | **NATIVE-YES** | measured: `pre=b0 → post=b0` after close |
| 9 | nested push/pop restores each level | **NATIVE-YES** | popup stack nests + per-window `NavLastIds`; 1 level measured, deeper by construction |
| 10 | Space activates | **NATIVE-YES** | measured: `activated=b0` |
| 11 | Enter activates (generic screens) | **NATIVE-YES** | measured: `activated=b0` |
| 15 | generic screens Space-OR-Enter | **NATIVE-YES** | both keys hardcoded to NavActivate |
| 12 | arrows adjust a single-tab-stop group (not activate) | **NATIVE-EFFORT** | native arrows = spatial nav between items; a one-stop group with arrow-cycled sub-selection is a custom widget (an `InvisibleButton` that reads arrows itself) — a few LOC |
| 14 | Game cluster Space-only, **Enter reserved** | **NATIVE-EFFORT** | Enter is hardcoded to activate; reserve it with `SetKeyOwner(ImGuiKey_Enter, …)` so nav's `IsKeyPressed(Enter, NoOwner)` is false — ~1–3 LOC per scope |
| 16 | Game Enter reserved for math submit | **NATIVE-EFFORT** | same key-ownership mechanism as #14 |
| 13 | "non-dispatch key not consumed" | **MODEL-DIFF** | native answers "did the focus layer consume this key?" via key-ownership, not an app dispatch return value — not a capability gap, a different model |

**Tally: 12 NATIVE-YES · 3 NATIVE-EFFORT (really one mechanism — key ownership — plus
one custom widget) · 1 MODEL-DIFF · 0 NATIVE-NO.** Nothing in the matrix is impossible
with the public API.

---

## STEP C — Native nav on the REAL app: what actually breaks

**It is not user error, and it is not a config mistake.** The blocker is
architectural and measurable:

- The five core navigable surfaces render **zero ImGui items** — every control is a
  draw-list primitive with manual mouse hit-testing:

  | surface | ImGui item-widgets | rendering |
  |---|---|---|
  | `root_screen.cpp` | **0** | `GetBackgroundDrawList()` + image slots + `IO.MousePos` hit-test |
  | `mode_selection_screen.cpp` | **0** | custom draw list |
  | `game_screen.cpp` | **0** | custom draw list |
  | `post_round_screen.cpp` | **0** | custom draw list |
  | `cluster.cpp` | **0** | `draw_button_icon` + `cluster_hit_test` |

  Real ImGui items (`Button`/`InvisibleButton`/`InputText`/`Checkbox`) exist **only**
  in the modal/settings dialogs (`modal_base`, `shop_view`, `leaderboard_view`,
  `settings_modal`, `account_modal`, `custom_popup`, `confirm_modal`, `help_modal`,
  `bet_size_buttons`).

- **Native nav can only focus items that call `ItemAdd()`.** Proven headlessly
  (`nav_customdraw_probe.cpp`): a window with one real `ImGui::Button` beside a
  custom-drawn rect, `NavEnableKeyboard` on, Tab hammered 6× →
  **`[ real real real real real real ]`, 1 distinct nav target, custom rect NEVER
  reached.**

So flipping on `ImGuiConfigFlags_NavEnableKeyboard` today gives you working nav
*inside the modals/settings* (real items) and **nothing on Root/Mode/Game/Post-Round**
— Tab there has no item to land on. That presents as "native nav doesn't work," and
it's a genuine architecture mismatch, **not** a mis-set flag. The fix is not a config
knob: wrap each custom-drawn control in an `ImGui::InvisibleButton` at its rect (real
`ItemAdd`) and draw the visual on top — the standard ImGui idiom for custom buttons.
That refactor (a few LOC per control) would make native nav see the controls and would
replace most of the seam.

---

## STEP D — Verdict

Native ImGui keyboard nav already expresses about **13 of your 16 behaviors out of the
box** — tab forward/backward, both wraps, focus-not-visible-until-keypress, Space+Enter
activation, modal trap with save/restore, and nesting — with the other three being
modest app-side effort (key-ownership to reserve Enter on the Game screen; a custom
widget for the single-stop arrow group) and **none impossible**. Your seam is
load-bearing only because your five core surfaces render every control as custom
draw-list primitives that never call `ItemAdd()`, so native nav — which can only focus
real ImGui items — is structurally blind to them (proven headlessly). Had those
controls been wrapped in `InvisibleButton`s (a few lines each), native nav would have
delivered most of the seam's 588 LOC for free, and the outline-vs-typing-target *drift*
the reconcile exists to fix would largely not arise, because ImGui would own focus
instead of `focus_manager` owning it in parallel. What the seam genuinely gives you that
native nav does not is small: a **stable app-level `FocusableId`** decoupled from
ImGui's internal `NavId` (convenient for the digit-1–6 direct-focus keys, for
deterministic *pure* unit tests, and for driving your own focus-ring on custom-drawn
controls), plus the per-screen **Space-only / Enter-reserved** policy. Honest bottom
line: most of the seam re-implements native nav, and it is justified by your
custom-rendering aesthetic — not by a capability gap in ImGui nav.

---

## Raw artifacts
| File | Contents |
|---|---|
| `nav_probe.cpp` / `nav_probe.json` | headless 16-case drive of real native nav |
| `nav_customdraw_probe.cpp` / `nav_customdraw_probe.json` | proof native nav can't focus custom draw-list controls |
| built binaries | `nav_probe`, `nav_customdraw_probe` |
