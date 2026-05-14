#include "persistence/sync_state.hpp"

#include <atomic>

namespace poker_trainer::persistence {

namespace {

// Single instance of the snapshot, guarded by a mutex-free protocol:
// reads and writes are serialized by std::atomic ordering on the
// `version` counter. Reader spins reading the version, the snapshot,
// and the version again; if the version matches, the read is coherent.
// Writer increments the version twice — once to mark the write as
// in-progress, once to mark it as complete.
//
// This is a seqlock pattern. Appropriate because writes are rare
// (one per sync attempt, which is on the order of seconds) and reads
// are frequent (every frame from Z11).

struct SyncStateStorage {
    std::atomic<std::uint64_t> version{0};
    SyncStateSnapshot snapshot{};
};

SyncStateStorage& storage() {
    static SyncStateStorage s;
    return s;
}

}  // namespace

SyncStateSnapshot read_sync_state() noexcept {
    auto& s = storage();
    while (true) {
        const std::uint64_t v1 = s.version.load(std::memory_order_acquire);
        if (v1 & 1) {
            // Write in progress, retry.
            continue;
        }
        const SyncStateSnapshot result = s.snapshot;
        const std::uint64_t v2 = s.version.load(std::memory_order_acquire);
        if (v1 == v2) {
            return result;
        }
        // Write happened during read, retry.
    }
}

void write_sync_state(const SyncStateSnapshot& snapshot) noexcept {
    auto& s = storage();
    const std::uint64_t v = s.version.load(std::memory_order_relaxed);
    s.version.store(v + 1, std::memory_order_release);  // mark in-progress
    s.snapshot = snapshot;
    s.version.store(v + 2, std::memory_order_release);  // mark complete
}

}  // namespace poker_trainer::persistence
