#include "backbone/focus_manager.hpp"

#include <cstddef>
#include <utility>
#include <vector>

// Threading model:
// - All public functions must be called on the main thread (the browser event
//   loop thread). focus_manager is a pure state machine driven by event_router
//   dispatch and rendering-layer queries, both of which run on the main thread.
// - No internal synchronization. Calling from multiple threads is undefined
//   behavior.

namespace poker_trainer::backbone {

namespace {

struct FocusContext {
    std::vector<FocusableId> list;
    std::size_t pointer{0};
    ScreenId screen{ScreenId::Root};  // Screen the base list belongs to; N/A for modal contexts.
    // Per-context "armed" latch. A freshly registered screen context starts
    // UNARMED: nothing is focused (the indicator is suppressed) even though a
    // pointer exists, until the first keyboard navigation into the context. The
    // first Tab/Shift-Tab arms it and lands on the first/last element WITHOUT
    // advancing past it; snap_focus_to (digit/mouse focus) arms it directly; a
    // modal push starts ARMED (its initial focus shows immediately). This makes
    // every screen start from "nothing focused," so the first Tab lands on item 1
    // uniformly rather than overshooting to item 2.
    bool armed{false};
};

FocusContext g_active_context;
std::vector<FocusContext> g_context_stack;
bool g_keyboard_mode_active{false};

}  // namespace

FocusableId get_focused_element() noexcept {
    // Three independent gates: keyboard mode must be active (mouse-mode users see
    // no indicator), the context must have focusables, and the context must be
    // armed (a freshly registered screen shows nothing until first navigation).
    if (!g_keyboard_mode_active || g_active_context.list.empty() ||
        !g_active_context.armed) {
        return kNoFocus;
    }
    return g_active_context.list[g_active_context.pointer];
}

bool is_keyboard_mode_active() noexcept {
    return g_keyboard_mode_active;
}

void activate_keyboard_mode() noexcept {
    g_keyboard_mode_active = true;
}

void snap_focus_to(FocusableId target) noexcept {
    const auto& list = g_active_context.list;
    for (std::size_t i = 0; i < list.size(); ++i) {
        if (list[i].value == target.value) {
            g_active_context.pointer = i;
            // Digit/mouse focus shows immediately: arm the context AND activate
            // keyboard mode, so get_focused_element returns this element (both gates
            // must be set, not just armed).
            g_active_context.armed = true;
            g_keyboard_mode_active = true;
            return;
        }
    }
    // Target not in active list — silent no-op.
}

void advance_focus(bool reverse) noexcept {
    const std::size_t n = g_active_context.list.size();
    if (n == 0) return;
    g_keyboard_mode_active = true;
    if (!g_active_context.armed) {
        // First navigation into a freshly registered (relocked) context: arm it and
        // land on the first element (or the last, when arriving via Shift-Tab)
        // WITHOUT advancing past it. register_focus_list left pointer at 0, so
        // forward arming keeps item 1.
        g_active_context.armed = true;
        g_active_context.pointer = reverse ? n - 1 : 0;
        return;
    }
    if (reverse) {
        g_active_context.pointer = (g_active_context.pointer + n - 1) % n;
    } else {
        g_active_context.pointer = (g_active_context.pointer + 1) % n;
    }
}

void register_focus_list(ScreenId screen_id,
                         std::span<const FocusableId> focusables) noexcept {
    g_active_context.list.assign(focusables.begin(), focusables.end());
    g_active_context.pointer = 0;
    g_active_context.screen = screen_id;
    // Relock on screen entry: the screen starts with nothing focused until the
    // first Tab/digit/click arms it. (register_focus_list runs once per screen
    // entry, not per frame, so this does not fight live navigation.)
    g_active_context.armed = false;
}

void push_focus_context(std::span<const FocusableId> focusables,
                        FocusableId initial_focus,
                        std::string_view /*tag*/) noexcept {
    g_context_stack.push_back(std::move(g_active_context));
    FocusContext new_ctx;
    // A modal opens already armed: its initial focus is shown immediately (a focus
    // trap presents a focused control on open, no priming Tab required).
    new_ctx.armed = true;
    new_ctx.list.assign(focusables.begin(), focusables.end());
    for (std::size_t i = 0; i < new_ctx.list.size(); ++i) {
        if (new_ctx.list[i].value == initial_focus.value) {
            new_ctx.pointer = i;
            break;
        }
    }
    g_active_context = std::move(new_ctx);
}

void pop_focus_context() noexcept {
    if (g_context_stack.empty()) return;
    g_active_context = std::move(g_context_stack.back());
    g_context_stack.pop_back();
}

std::size_t context_depth() noexcept {
    return g_context_stack.size();
}

void reset_focus_manager_for_testing() noexcept {
    g_active_context = FocusContext{};
    g_context_stack.clear();
    g_keyboard_mode_active = false;
}

}  // namespace poker_trainer::backbone
