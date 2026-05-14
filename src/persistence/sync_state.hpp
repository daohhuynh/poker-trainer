#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace poker_trainer::persistence {

// The state of the sync subsystem at any given moment.
enum class SyncStatus : std::uint8_t {
    // No sync attempts have been made yet this session. Default at
    // app boot before the first sync is initiated. Indicator is hidden.
    Idle = 0,

    // A sync is currently in flight. Indicator is hidden (treating
    // in-flight as "not yet a failure"). Transitioning to SyncOk or
    // SyncFailing depending on the outcome.
    SyncInProgress = 1,

    // The most recent sync completed successfully. Indicator is
    // hidden. Z04 transitions to SyncInProgress when initiating the
    // next sync.
    SyncOk = 2,

    // The most recent sync failed, or the system is in retry backoff
    // between sync attempts. Indicator is visible. Z04 transitions
    // back to SyncInProgress when initiating a retry; transitions to
    // SyncOk if the retry succeeds.
    SyncFailing = 3,
};

// Snapshot of the sync state at the moment of read. Returned by value
// from the read API so the consumer gets a coherent snapshot, not a
// torn read across atomic fields.
struct SyncStateSnapshot {
    SyncStatus status{SyncStatus::Idle};

    // Steady-clock timestamp of the last successful sync, or
    // std::chrono::steady_clock::time_point{} (epoch) if no sync has
    // succeeded this session.
    std::chrono::steady_clock::time_point last_success{};

    // Steady-clock timestamp of the last failed sync, or epoch if no
    // sync has failed this session.
    std::chrono::steady_clock::time_point last_failure{};

    // Number of consecutive failures since the last success. Reset
    // to 0 on every successful sync. Used by Z04 to compute exponential
    // backoff between retry attempts.
    std::uint32_t consecutive_failures{0};
};

// Read the current sync state. Safe to call from any thread. Returns
// a coherent snapshot — fields are not torn across reads.
[[nodiscard]] SyncStateSnapshot read_sync_state() noexcept;

// Write the sync state. Z04 is the only legitimate writer. Calling
// from any other zone is a contract violation. The write is atomic
// from the reader's perspective: a single call to read_sync_state
// after this returns will observe all fields from this write
// atomically (no torn reads).
void write_sync_state(const SyncStateSnapshot& snapshot) noexcept;

}  // namespace poker_trainer::persistence
