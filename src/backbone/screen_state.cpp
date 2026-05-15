#include "backbone/screen_state.hpp"

#include <atomic>

namespace poker_trainer::backbone {

namespace {

// Single instance of the snapshot, guarded by a mutex-free protocol:
// reads and writes are serialized by std::atomic ordering on the
// `version` counter. Reader spins reading the version, the snapshot,
// and the version again; if the version matches, the read is coherent.
// Writer increments the version twice — once to mark the write as
// in-progress, once to mark it as complete.
//
// This is a seqlock pattern, identical to sync_state.cpp's storage
// protocol. Appropriate because writes are rare (one per screen
// transition) and reads happen on every frame from rendering zones.

struct ScreenStateStorage {
    std::atomic<std::uint64_t> version{0};
    ScreenStateSnapshot snapshot{};
};

ScreenStateStorage& storage() {
    static ScreenStateStorage s;
    return s;
}

void write_snapshot(ScreenStateStorage& s, const ScreenStateSnapshot& snap) noexcept {
    const std::uint64_t v = s.version.load(std::memory_order_relaxed);
    s.version.store(v + 1, std::memory_order_release);  // mark in-progress
    s.snapshot = snap;
    s.version.store(v + 2, std::memory_order_release);  // mark complete
}

}  // namespace

ScreenStateSnapshot read_screen_state() noexcept {
    auto& s = storage();
    while (true) {
        const std::uint64_t v1 = s.version.load(std::memory_order_acquire);
        if (v1 & 1) {
            // Write in progress, retry.
            continue;
        }
        const ScreenStateSnapshot result = s.snapshot;
        const std::uint64_t v2 = s.version.load(std::memory_order_acquire);
        if (v1 == v2) {
            return result;
        }
        // Write happened during read, retry.
    }
}

void set_screen(ScreenId screen,
                std::optional<engine::ScenarioId> active_scenario) noexcept {
    // Per the header contract: only Game and PostRound retain an active
    // scenario. Any other screen clears it regardless of what was passed.
    if (screen != ScreenId::Game && screen != ScreenId::PostRound) {
        active_scenario.reset();
    }
    ScreenStateSnapshot snap;
    snap.current = screen;
    snap.active_scenario = active_scenario;
    write_snapshot(storage(), snap);
}

bool is_in_scenario() noexcept {
    const ScreenStateSnapshot s = read_screen_state();
    return s.current == ScreenId::Game || s.current == ScreenId::PostRound;
}

void reset_for_testing() noexcept {
    write_snapshot(storage(), ScreenStateSnapshot{});
}

}  // namespace poker_trainer::backbone
