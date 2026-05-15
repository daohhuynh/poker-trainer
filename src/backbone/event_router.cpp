#include "backbone/event_router.hpp"

#include <algorithm>
#include <atomic>
#include <utility>
#include <vector>

// Threading model:
// - All install/uninstall/dispatch entry points are expected to be invoked
//   on the browser main thread (Z05 forwards browser-native events synchronously).
// - g_handler_counter is std::atomic as a defensive measure so handle minting
//   stays well-defined if a non-main thread ever installs a handler. The
//   storage vectors themselves are not mutex-protected; cross-thread mutation
//   would be a contract violation.
// - Handlers may reentrantly install or uninstall handlers from inside their
//   own callback. The dispatch loops snapshot the active vector before
//   invoking, so concurrent mutation during dispatch is safe and never visits
//   a handler that was uninstalled before this dispatch began.
//
// Handle layout (HandlerHandle::value, 64 bits):
//   bit 63        : 0 = key handler, 1 = mouse handler
//   bits 62..0    : monotonic counter (>= 1, so value never collides with
//                   kInvalidHandlerHandle which is all-zero).

namespace poker_trainer::backbone {

namespace {

struct KeyHandlerEntry {
    HandlerPriority priority;
    HandlerHandle handle;
    KeyHandler handler;
};

struct MouseHandlerEntry {
    HandlerPriority priority;
    HandlerHandle handle;
    MouseHandler handler;
};

constexpr std::uint64_t kHandlerTypeBit = std::uint64_t{1} << 63;
constexpr std::uint64_t kKeyHandlerTag = 0;
constexpr std::uint64_t kMouseHandlerTag = kHandlerTypeBit;

std::atomic<std::uint64_t> g_handler_counter{1};

std::vector<KeyHandlerEntry> g_key_handlers;
std::vector<MouseHandlerEntry> g_mouse_handlers;

constexpr bool is_mouse_handle(HandlerHandle h) noexcept {
    return (h.value & kHandlerTypeBit) != 0;
}

template <typename Entry>
void insert_sorted(std::vector<Entry>& vec, Entry entry) {
    // lower_bound positions the new entry at the FIRST slot whose priority
    // is >= the new entry's priority. Inserting there means a newer entry
    // at the same priority runs before older entries at that priority
    // (LIFO within a priority level — natural for stacked modals).
    auto it = std::lower_bound(
        vec.begin(), vec.end(), entry,
        [](const Entry& a, const Entry& b) {
            return static_cast<std::uint8_t>(a.priority) <
                   static_cast<std::uint8_t>(b.priority);
        });
    vec.insert(it, std::move(entry));
}

}  // namespace

HandlerHandle install_key_handler(KeyHandler handler,
                                  HandlerPriority priority,
                                  std::string_view /*tag*/) noexcept {
    const std::uint64_t counter =
        g_handler_counter.fetch_add(1, std::memory_order_relaxed);
    const HandlerHandle h{kKeyHandlerTag | counter};
    insert_sorted(g_key_handlers,
                  KeyHandlerEntry{priority, h, std::move(handler)});
    return h;
}

HandlerHandle install_mouse_handler(MouseHandler handler,
                                    HandlerPriority priority,
                                    std::string_view /*tag*/) noexcept {
    const std::uint64_t counter =
        g_handler_counter.fetch_add(1, std::memory_order_relaxed);
    const HandlerHandle h{kMouseHandlerTag | counter};
    insert_sorted(g_mouse_handlers,
                  MouseHandlerEntry{priority, h, std::move(handler)});
    return h;
}

void uninstall_handler(HandlerHandle handle) noexcept {
    if (handle.value == 0) return;
    if (is_mouse_handle(handle)) {
        auto it = std::find_if(
            g_mouse_handlers.begin(), g_mouse_handlers.end(),
            [handle](const MouseHandlerEntry& e) {
                return e.handle.value == handle.value;
            });
        if (it != g_mouse_handlers.end()) {
            g_mouse_handlers.erase(it);
        }
    } else {
        auto it = std::find_if(
            g_key_handlers.begin(), g_key_handlers.end(),
            [handle](const KeyHandlerEntry& e) {
                return e.handle.value == handle.value;
            });
        if (it != g_key_handlers.end()) {
            g_key_handlers.erase(it);
        }
    }
}

void dispatch_key_event(const KeyEvent& event) noexcept {
    // Snapshot so reentrant install/uninstall during dispatch does not
    // invalidate iteration state.
    std::vector<KeyHandlerEntry> snapshot = g_key_handlers;
    for (const auto& entry : snapshot) {
        if (entry.handler && entry.handler(event)) {
            return;
        }
    }
}

void dispatch_mouse_event(const MouseEvent& event) noexcept {
    std::vector<MouseHandlerEntry> snapshot = g_mouse_handlers;
    for (const auto& entry : snapshot) {
        if (entry.handler && entry.handler(event)) {
            return;
        }
    }
}

void reset_event_router_for_testing() noexcept {
    g_key_handlers.clear();
    g_mouse_handlers.clear();
    g_handler_counter.store(1, std::memory_order_relaxed);
}

}  // namespace poker_trainer::backbone
