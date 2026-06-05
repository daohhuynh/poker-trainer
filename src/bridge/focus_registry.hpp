#pragma once

#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"

#include <cstdint>
#include <functional>
#include <vector>

// Shared focus / input reconciliation substrate (bridge layer).
//
// focus_manager is the single source of truth for which element is focused (Tab /
// digit keys / clicks move it; it draws the outline). ImGui keeps its OWN keyboard
// focus and decides WantCaptureKeyboard. Nothing couples them, so the outline and
// the typing target drift apart. This module is the reusable coupling glue every
// keyboard-and-text surface (Z09 math inputs, the Custom popup, Root/Mode) needs,
// extracted from Z09's originally-hand-rolled reconcile so later surfaces point at
// one implementation instead of re-deriving it.
//
// Three SEPARABLE capabilities; a surface opts into whichever it needs:
//   1. Registry  — per-element { is_text_field, activate, adjust }, populated
//      per-surface (lifetime tied to the focus-list registration), one instance
//      owned by reference off BridgeRuntime (CLAUDE.md §10).
//   2. Reconcile + ring — drive ImGui's keyboard focus from focus_manager + the
//      registry's is_text_field (SetKeyboardFocusHere / ClearActiveID, change-frame
//      guarded) and draw the focus ring.
//   3. Dispatch — given the focused id + key, invoke the registry's activate
//      (Space/Enter) or adjust (arrows). Each surface keeps its own event-router
//      registration at its own priority/context and just calls this body.
//
// The pure parts (registry, the reconcile DECISION, dispatch) carry no ImGui and
// are unit-tested. The render glue (begin_focus_reconcile / grab_keyboard_if_target
// / draw_focus_ring*) is the only part that calls ImGui; it is compiled behind
// `#ifdef __EMSCRIPTEN__` (a no-op natively), keeping this library ImGui-free under
// the native test compiler exactly like the rest of the bridge layer. Ring colors
// are passed in by the caller (the bridge layer has no theme dependency).

namespace poker_trainer::bridge {

// ===== Capability 1: per-element registry =====

// One focusable element's reconciliation behavior. `is_text_field` drives the
// reconcile (a text field grabs ImGui keyboard focus; a non-text stop yields it).
// `activate` / `adjust` are the optional dispatch hooks; a surface that handles a
// key itself (e.g. Z09's submit-all Enter) leaves them empty.
struct FocusableEntry {
    bool is_text_field{false};
    std::function<void()> activate{};      // Space/Enter; empty => no activation
    std::function<void(int)> adjust{};     // arrow nudge, delta -1/+1; empty => none
};

// Maps focusable ids to their reconciliation behavior for the active surface.
// Repopulated whenever the surface (re)registers its focus list; the reconcile
// guard is reset on clear() so a fresh surface re-syncs from scratch.
class FocusRegistry {
public:
    // Drop all entries (called when a surface re-registers its focus list).
    void clear() noexcept;

    // Register (or replace) the entry for `id`.
    void register_element(backbone::FocusableId id, FocusableEntry entry);

    // The entry for `id`, or nullptr when `id` is not registered.
    [[nodiscard]] const FocusableEntry* find(backbone::FocusableId id) const noexcept;

    // True when `id` is a registered text field. False for non-text stops AND for
    // unregistered ids (an element this surface does not own).
    [[nodiscard]] bool is_text_field(backbone::FocusableId id) const noexcept;

private:
    struct Slot {
        backbone::FocusableId id{};
        FocusableEntry entry{};
    };
    std::vector<Slot> slots_;
};

// ===== Capability 2: reconcile + ring =====

// What the render path must do this frame to keep ImGui's keyboard focus in sync.
enum class ImGuiFocusAction : std::uint8_t {
    None,           // focus unchanged, or on an element this surface does not own
    FocusTextBox,   // SetKeyboardFocusHere on `target` — a text field gained focus
    YieldKeyboard,  // ClearActiveID — a non-text element gained / holds focus
};

struct FocusReconcile {
    ImGuiFocusAction action{ImGuiFocusAction::None};
    backbone::FocusableId target{backbone::kNoFocus};  // valid iff FocusTextBox
};

// Pure reconcile decision (no ImGui), given the element ImGui was last reconciled
// to (`prev`) and the element focus_manager now reports (`current`). Generalizes
// Z09's hand-rolled rule via the registry's is_text_field:
//   * `current` is a registered NON-text element -> YieldKeyboard, EVERY frame
//     (checked before the unchanged early-out: a pending SetKeyboardFocusHere from
//     the field the user just left would otherwise re-activate it permanently);
//   * `current == prev` -> None (re-grabbing a text field every frame traps the
//     caret);
//   * `current` is a registered TEXT field -> FocusTextBox(current);
//   * otherwise (unregistered / not owned here) -> None.
[[nodiscard]] FocusReconcile decide_focus_reconcile(const FocusRegistry& registry,
                                                    backbone::FocusableId prev,
                                                    backbone::FocusableId current) noexcept;

// Focus_manager's currently focused element, or kNoFocus when keyboard mode is
// inactive. The reconcile reads focus through this gate; surfaces record it into
// their per-surface `last_synced` at the end of the frame.
[[nodiscard]] backbone::FocusableId active_focus_or_none() noexcept;

// Begin-of-frame reconcile: read the focused element, decide against `last_synced`,
// and APPLY the once-per-frame yield (ClearActiveID, gated on io.WantTextInput so a
// non-text button mid-click is left alone). Returns the decision so the caller can
// drive the per-element grab. ImGui-only; a no-op returning None natively.
[[nodiscard]] FocusReconcile begin_focus_reconcile(const FocusRegistry& registry,
                                                   backbone::FocusableId last_synced);

// Per text element, during its layout: steer ImGui's keyboard focus here when this
// frame's decision targets it (SetKeyboardFocusHere). ImGui-only; no-op natively.
void grab_keyboard_if_target(const FocusReconcile& reconcile, backbone::FocusableId id);

// Draw the 2px focus ring (color = packed RGBA, resolved by the caller from the
// border_focus token) around the most recent ImGui item when `id` is the focused
// element. ImGui-only; no-op natively.
void draw_focus_ring(backbone::FocusableId id, std::uint32_t ring_color);

// Same as draw_focus_ring but around an explicit rect — for a focus group whose
// outline spans a whole row rather than a single item (the bet-size group).
void draw_focus_ring_rect(backbone::FocusableId id, float min_x, float min_y,
                          float max_x, float max_y, std::uint32_t ring_color);

// ===== Capability 3: dispatch =====

// Invoke the focused element's registered hook for `key`: Space/Enter -> activate;
// ArrowUp/ArrowRight -> adjust(+1); ArrowDown/ArrowLeft -> adjust(-1). Returns true
// when a hook ran (the key is consumed); false when `focused` is unregistered, the
// key is not a dispatch key, or the matching hook is empty. Pure (no ImGui): the
// caller's own event-router registration decides priority/context.
[[nodiscard]] bool dispatch_focus_key(const FocusRegistry& registry,
                                      backbone::FocusableId focused,
                                      backbone::KeyCode key);

}  // namespace poker_trainer::bridge
