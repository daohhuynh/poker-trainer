#include "backbone/modal_state.hpp"

#include <atomic>

namespace poker_trainer::backbone {

namespace {
std::atomic<std::size_t> g_modal_depth{0};
std::atomic<std::uint32_t> g_topmost_value{0};
}  // namespace

bool is_any_modal_open() noexcept {
    return g_modal_depth.load(std::memory_order_acquire) > 0;
}

ModalId topmost_modal() noexcept {
    return ModalId{g_topmost_value.load(std::memory_order_acquire)};
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

void reset_for_testing() noexcept {
    g_modal_depth.store(0, std::memory_order_release);
    g_topmost_value.store(0, std::memory_order_release);
}

}  // namespace poker_trainer::backbone
