#include "bridge/focus_registry.hpp"

#include "backbone/focus_manager.hpp"

#ifdef __EMSCRIPTEN__
#include <imgui.h>
#include <imgui_internal.h>  // ImGui::ClearActiveID -- release an active InputText
#endif

// Render glue for the reconcile + ring capability. The only ImGui-touching part of
// the substrate, compiled behind __EMSCRIPTEN__ so this library stays ImGui-free
// under the native test compiler (the rest of the bridge layer follows the same
// rule). Natively these are no-ops: rendering is browser-verified, never unit-
// tested (CLAUDE.md §9). The pure reconcile DECISION (decide_focus_reconcile) lives
// in focus_registry.cpp and is unit-tested there.

namespace poker_trainer::bridge {

FocusReconcile begin_focus_reconcile(const FocusRegistry& registry,
                                     backbone::FocusableId last_synced) {
    const FocusReconcile reconcile =
        decide_focus_reconcile(registry, last_synced, active_focus_or_none());
#ifdef __EMSCRIPTEN__
    // YieldKeyboard fires every frame a non-text stop holds focus. Only release
    // ImGui's active item when a TEXT field is actually active (io.WantTextInput) --
    // the field that a pending SetKeyboardFocusHere re-activated after focus moved
    // to the non-text stop. Gating on WantTextInput leaves a non-text button being
    // clicked (ActiveId set, WantTextInput NOT set) alone, so its click registers.
    if (reconcile.action == ImGuiFocusAction::YieldKeyboard && ImGui::GetIO().WantTextInput) {
        ImGui::ClearActiveID();
    }
#endif
    return reconcile;
}

void grab_keyboard_if_target([[maybe_unused]] const FocusReconcile& reconcile,
                             [[maybe_unused]] backbone::FocusableId id) {
#ifdef __EMSCRIPTEN__
    // Steer ImGui's keyboard focus to the field the outline just landed on (called
    // only on the focus-change frame -- see decide_focus_reconcile -- never every
    // frame), so the typing target follows keyboard navigation without a click.
    if (reconcile.action == ImGuiFocusAction::FocusTextBox && reconcile.target == id) {
        ImGui::SetKeyboardFocusHere();
    }
#endif
}

#ifdef __EMSCRIPTEN__
namespace {

// True when keyboard mode is active and `id` is the focused element (the same
// gate the ring is drawn behind). Used only by the ring draws below, so it is
// compiled only in the wasm build to keep the native object warning-clean.
[[nodiscard]] bool focus_on(backbone::FocusableId id) {
    return backbone::is_keyboard_mode_active() && backbone::get_focused_element() == id;
}

}  // namespace
#endif

void draw_focus_ring([[maybe_unused]] backbone::FocusableId id,
                     [[maybe_unused]] std::uint32_t ring_color) {
#ifdef __EMSCRIPTEN__
    if (focus_on(id)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ring_color, 0.0f, 0, 2.0f);
    }
#endif
}

void draw_focus_ring_rect([[maybe_unused]] backbone::FocusableId id,
                          [[maybe_unused]] float min_x, [[maybe_unused]] float min_y,
                          [[maybe_unused]] float max_x, [[maybe_unused]] float max_y,
                          [[maybe_unused]] std::uint32_t ring_color) {
#ifdef __EMSCRIPTEN__
    if (focus_on(id)) {
        ImGui::GetWindowDrawList()->AddRect(ImVec2{min_x, min_y}, ImVec2{max_x, max_y}, ring_color,
                                            0.0f, 0, 2.0f);
    }
#endif
}

}  // namespace poker_trainer::bridge
