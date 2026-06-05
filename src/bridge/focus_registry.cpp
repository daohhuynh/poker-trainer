#include "bridge/focus_registry.hpp"

#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"

#include <utility>

// Pure substrate logic: the registry, the reconcile DECISION, and dispatch. No
// ImGui (the render glue lives in focus_reconcile.cpp behind __EMSCRIPTEN__), so
// these objects compile and unit-test under the native compiler.

namespace poker_trainer::bridge {

void FocusRegistry::clear() noexcept { slots_.clear(); }

void FocusRegistry::register_element(backbone::FocusableId id, FocusableEntry entry) {
    for (Slot& slot : slots_) {
        if (slot.id == id) {
            slot.entry = std::move(entry);
            return;
        }
    }
    slots_.push_back(Slot{id, std::move(entry)});
}

const FocusableEntry* FocusRegistry::find(backbone::FocusableId id) const noexcept {
    for (const Slot& slot : slots_) {
        if (slot.id == id) {
            return &slot.entry;
        }
    }
    return nullptr;
}

bool FocusRegistry::is_text_field(backbone::FocusableId id) const noexcept {
    const FocusableEntry* entry = find(id);
    return entry != nullptr && entry->is_text_field;
}

FocusReconcile decide_focus_reconcile(const FocusRegistry& registry, backbone::FocusableId prev,
                                      backbone::FocusableId current) noexcept {
    const FocusableEntry* entry = registry.find(current);
    // A registered non-text stop yields ImGui keyboard capture EVERY frame it is
    // focused -- decided before the unchanged early-out so a pending
    // SetKeyboardFocusHere from the field just left is released next frame.
    if (entry != nullptr && !entry->is_text_field) {
        return {ImGuiFocusAction::YieldKeyboard, backbone::kNoFocus};
    }
    // Everything else acts only on a change: re-grabbing a text field every frame
    // would trap the caret.
    if (current == prev) {
        return {ImGuiFocusAction::None, backbone::kNoFocus};
    }
    // Focus landed on a text field -> steer ImGui's text focus there.
    if (entry != nullptr && entry->is_text_field) {
        return {ImGuiFocusAction::FocusTextBox, current};
    }
    // Focus moved to an element this surface does not own -> leave ImGui untouched.
    return {ImGuiFocusAction::None, backbone::kNoFocus};
}

backbone::FocusableId active_focus_or_none() noexcept {
    return backbone::is_keyboard_mode_active() ? backbone::get_focused_element()
                                               : backbone::kNoFocus;
}

bool dispatch_focus_key(const FocusRegistry& registry, backbone::FocusableId focused,
                        backbone::KeyCode key) {
    const FocusableEntry* entry = registry.find(focused);
    if (entry == nullptr) {
        return false;
    }
    switch (key) {
        case backbone::KeyCode::Space:
        case backbone::KeyCode::Enter:
            if (entry->activate) {
                entry->activate();
                return true;
            }
            return false;
        case backbone::KeyCode::ArrowUp:
        case backbone::KeyCode::ArrowRight:
            if (entry->adjust) {
                entry->adjust(+1);
                return true;
            }
            return false;
        case backbone::KeyCode::ArrowDown:
        case backbone::KeyCode::ArrowLeft:
            if (entry->adjust) {
                entry->adjust(-1);
                return true;
            }
            return false;
        default:
            return false;
    }
}

}  // namespace poker_trainer::bridge
