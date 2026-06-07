#include "bridge/frame_tick.hpp"

#include <utility>
#include <vector>

namespace poker_trainer::bridge {

namespace {

// The single ordered list of per-frame callbacks. A function-local static (the
// audio_engine() / theme-service pattern in this codebase): the register/run API is
// parameter-less free functions that any zone's install_*() must reach, and the app
// is single-threaded (the browser main thread), so no synchronization is needed.
// Intentionally NOT a backbone primitive and not hung off BridgeRuntime — a zone
// self-registers its tick at install time without a handle to the app-root runtime,
// exactly as it subscribes to the backbone buses.
std::vector<FrameTickFn>& ticks() {
    static std::vector<FrameTickFn> instance;
    return instance;
}

}  // namespace

void register_frame_tick(FrameTickFn tick) {
    if (tick) {
        ticks().push_back(std::move(tick));
    }
}

void run_frame_ticks() {
    // Index-based, not a range-for: a tick is permitted to register another tick,
    // which may reallocate the vector. New ticks appended this frame run this frame,
    // matching the registration-order contract.
    std::vector<FrameTickFn>& list = ticks();
    for (std::size_t i = 0; i < list.size(); ++i) {
        list[i]();
    }
}

void clear_frame_ticks() noexcept { ticks().clear(); }

}  // namespace poker_trainer::bridge
