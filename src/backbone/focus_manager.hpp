#pragma once

#include "backbone/event_router.hpp"
#include "backbone/screen_state.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace poker_trainer::backbone {

// A focusable element identifier. Each focusable element in the UI
// has a unique ID. IDs are assigned by zones when they register
// focus lists; the focus manager treats them as opaque.
//
// Convention: zones use hashed string literals for IDs (compile-time
// computed via a constexpr fnv-1a or similar). This gives stable IDs
// across builds without manually maintained ID assignments.
struct FocusableId {
    std::uint64_t value{0};
    constexpr bool operator==(const FocusableId&) const noexcept = default;
    constexpr auto operator<=>(const FocusableId&) const noexcept = default;
};

inline constexpr FocusableId kInvalidFocusableId{0};

// Sentinel: focus is on nothing. Used when keyboard mode is inactive
// or when a context has no focusable elements.
inline constexpr FocusableId kNoFocus{0};

// Read the currently focused element. Returns kNoFocus when keyboard
// mode is inactive or no element is focused. The event router consults
// this to route enter-key activation to the focused element.
[[nodiscard]] FocusableId get_focused_element() noexcept;

// Returns true if keyboard navigation mode is currently active.
// Becomes true on the first Tab keypress OR the first mouse click on a
// focusable element, and once true it persists for the remainder of the
// session — it is never deactivated (per the architecture's focus spec).
[[nodiscard]] bool is_keyboard_mode_active() noexcept;

// Activate keyboard navigation mode. Called automatically on the first
// Tab keypress and on the first mouse click on a focusable element;
// there is no setting gating this. Idempotent — subsequent calls are
// no-ops.
void activate_keyboard_mode() noexcept;

// Snap focus to a specific element. Used when entering a new screen
// or modal to set the initial focus, and when arrow keys move focus
// within a bounded group (like the bet size tier buttons or a
// coupled-slider group).
void snap_focus_to(FocusableId target) noexcept;

// Advance focus to the next element in the current context. Wraps
// to the first element when called past the last. Called by Z05's
// Tab handler.
void advance_focus(bool reverse) noexcept;

// Register a focus list for the given screen. Replaces the existing
// active list (used at screen transitions). The order of the span
// determines Tab order. `screen_id` identifies the screen the list
// belongs to.
void register_focus_list(ScreenId screen_id,
                         std::span<const FocusableId> focusables) noexcept;

// Push a new focus context onto the stack. Used when a modal opens.
// The current focus is saved; the new context starts with the
// provided focusables and the provided initial focus.
//
// The `tag` parameter is a human-readable string for debugging.
void push_focus_context(std::span<const FocusableId> focusables,
                        FocusableId initial_focus,
                        std::string_view tag) noexcept;

// Pop the topmost focus context. Used when a modal closes. The
// prior context's focus is restored. If the stack is at its
// base context, this is a no-op (the base context is the
// permanent screen-level context).
void pop_focus_context() noexcept;

// The current focus context depth. 0 means base (screen) context;
// each modal push increments by 1. Used by debug tooling.
[[nodiscard]] std::size_t context_depth() noexcept;

// Reset all focus state to initial (no focus, no contexts beyond
// base, keyboard mode inactive). Used by the integration test.
void reset_focus_manager_for_testing() noexcept;

// Compile-time helper: compute a FocusableId from a string literal
// via FNV-1a hash. This is the recommended way to define focusable
// IDs in zone code.
//
// Example:
//     inline constexpr FocusableId kFocusPlayButton =
//         make_focusable_id("root.play_button");
[[nodiscard]] constexpr FocusableId make_focusable_id(
    std::string_view name) noexcept {
    std::uint64_t hash = 0xCBF29CE484222325ULL;  // FNV-1a 64-bit offset basis
    for (char c : name) {
        hash ^= static_cast<std::uint64_t>(static_cast<std::uint8_t>(c));
        hash *= 0x100000001B3ULL;  // FNV-1a 64-bit prime
    }
    // Reserve 0 as kNoFocus / invalid; if the hash collides with 0,
    // remap to 1. Collision probability is negligible.
    if (hash == 0) {
        hash = 1;
    }
    return FocusableId{hash};
}

}  // namespace poker_trainer::backbone
