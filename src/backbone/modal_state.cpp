#include "backbone/modal_state.hpp"

#include "backbone/screen_state.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>

// Zone 11 owns the implementation of the sealed modal_state.hpp contract. This is
// the real modal stack: a small fixed-depth stack of ModalId, pushed on open and
// popped on close. It REPLACES modal_state_stub.cpp in the `backbone` library; the
// stub is retained solely for phase0_integration_test (whose sign-off gate asserts
// the stub's behavior).
//
// Threading: the stack is mutated only on the browser main thread (Z11 opens /
// closes modals during dispatch / render). The depth is mirrored into an atomic so
// Z03's audio update — which edge-polls modal_stack_depth() from the audio thread
// to fire the open / close swoosh — reads a consistent count with no data race.
// current_modal_id() reads the stack itself and is main-thread-only (event router
// + render), matching every other modal_state consumer.
//
// Conforming to the sealed polling interface (no modal_opened / modal_closed event
// API was added): driving the stack so modal_stack_depth() changes on each open /
// close is exactly what Z03 already observes and what Z10 will observe.

namespace poker_trainer::backbone {

namespace {

// Modals essentially never nest more than two deep in this product (a confirmation
// over a Settings/Shop modal); 8 is a generous ceiling that makes overflow a
// no-op rather than UB.
constexpr std::size_t kMaxModalDepth = 8;

std::array<ModalId, kMaxModalDepth> g_stack{};
std::size_t g_depth = 0;                       // authoritative stack size (main thread)
std::atomic<std::size_t> g_depth_atomic{0};    // cross-thread mirror for the audio poll

}  // namespace

bool is_any_modal_open() noexcept {
    return g_depth_atomic.load(std::memory_order_acquire) > 0;
}

std::optional<ModalId> current_modal_id() noexcept {
    if (g_depth == 0) {
        return std::nullopt;
    }
    return g_stack[g_depth - 1];
}

bool is_modal_locked() noexcept {
    // Modal interaction is locked while the tutorial walkthrough is active. Derived
    // from the backbone tutorial phase rather than an independently set flag; the
    // tutorial that sets the phase is a Z14 seam (currently never Active), so this
    // path is dormant-but-built.
    return read_screen_state().tutorial_state.phase == TutorialPhase::Active;
}

std::size_t modal_stack_depth() noexcept {
    return g_depth_atomic.load(std::memory_order_acquire);
}

void notify_modal_opened(ModalId id) noexcept {
    if (g_depth >= kMaxModalDepth) {
        return;  // overflow guard: ignore rather than corrupt the stack
    }
    g_stack[g_depth] = id;
    ++g_depth;
    g_depth_atomic.store(g_depth, std::memory_order_release);
}

void notify_modal_closed(ModalId id) noexcept {
    if (g_depth == 0) {
        return;
    }
    // Per the header contract the id should match the topmost modal; when it does
    // not, the mismatch is a bug to fix at the call site — pop anyway so the stack
    // never wedges.
    (void)id;
    --g_depth;
    g_stack[g_depth] = kNoModal;
    g_depth_atomic.store(g_depth, std::memory_order_release);
}

void reset_modal_state_for_testing() noexcept {
    g_depth = 0;
    g_depth_atomic.store(0, std::memory_order_release);
    g_stack.fill(kNoModal);
}

}  // namespace poker_trainer::backbone
