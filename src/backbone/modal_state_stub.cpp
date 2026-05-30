#include "backbone/modal_state.hpp"

#include "backbone/screen_state.hpp"

#include <atomic>
#include <optional>

namespace poker_trainer::backbone {

namespace {
std::atomic<std::size_t> g_modal_depth{0};
std::atomic<std::uint32_t> g_topmost_value{0};
}  // namespace

bool is_any_modal_open() noexcept {
    return g_modal_depth.load(std::memory_order_acquire) > 0;
}

std::optional<ModalId> current_modal_id() noexcept {
    const std::uint32_t v = g_topmost_value.load(std::memory_order_acquire);
    if (v == 0) {
        return std::nullopt;
    }
    return ModalId{v};
}

bool is_modal_locked() noexcept {
    // Modal interaction is locked while the tutorial walkthrough is active.
    // Derived from the backbone tutorial phase rather than an independently
    // set flag; Z11's full implementation may refine which phases lock.
    return read_screen_state().tutorial_state.phase == TutorialPhase::Active;
}

std::size_t modal_stack_depth() noexcept {
    return g_modal_depth.load(std::memory_order_acquire);
}

void notify_modal_opened(ModalId id) noexcept {
    g_modal_depth.fetch_add(1, std::memory_order_acq_rel);
    g_topmost_value.store(id.value, std::memory_order_release);
}

void notify_modal_closed(ModalId /*id*/) noexcept {
    if (g_modal_depth.load(std::memory_order_acquire) > 0) {
        g_modal_depth.fetch_sub(1, std::memory_order_acq_rel);
    }
    if (g_modal_depth.load(std::memory_order_acquire) == 0) {
        g_topmost_value.store(0, std::memory_order_release);
    }
}

void reset_modal_state_for_testing() noexcept {
    g_modal_depth.store(0, std::memory_order_release);
    g_topmost_value.store(0, std::memory_order_release);
}

}  // namespace poker_trainer::backbone
