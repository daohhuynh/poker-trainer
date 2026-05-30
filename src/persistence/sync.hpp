#pragma once

#include "persistence/clock.hpp"
#include "persistence/persistence_schema.hpp"
#include "persistence/sync_state.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace poker_trainer::persistence {

// Forward-declared so the seam's upload_initial signature can name the
// migration payload without sync.hpp depending on migration.hpp (the
// dependency runs the other way: Migrator uses SyncBackend).
struct AccountMigrationState;

// Outcome of fetching server-side account state at session start / sign-in.
enum class FetchOutcome : std::uint8_t {
    // The server holds account state for this user. Adopt it (server is the
    // source of truth).
    Found = 0,

    // The account exists but the server holds no state yet (a brand-new
    // account that has never been seeded). The guest's local state migrates
    // up as the initial account state.
    NotFound = 1,

    // The fetch failed (offline / server error). Keep local state; the sync
    // subsystem reflects the failure and retries with backoff.
    Failed = 2,
};

struct FetchResult {
    FetchOutcome outcome{FetchOutcome::Failed};

    // Populated only when outcome == Found.
    AppState state{};
};

// Sync seam — server-side account state.
//
// In deployment this talks to the leaderboard backend over authenticated
// HTTP (the Auth0 access token gates the request); native tests inject a mock
// returning success / failure and recording call order. Z04 never branches on
// which is installed.
class SyncBackend {
public:
    virtual ~SyncBackend() = default;

    // Fetch the authoritative server-side state for the given Auth0 user.
    [[nodiscard]] virtual FetchResult fetch(std::string_view auth0_user_id) = 0;

    // Push pending writes to the server, oldest first. Returns true only when
    // the whole ordered batch is durably accepted; false on any failure (the
    // caller keeps the batch queued and retries with backoff).
    [[nodiscard]] virtual bool push(
        std::string_view auth0_user_id,
        std::span<const AppState> ordered_writes) = 0;

    // Seed a brand-new account with the migration payload (exactly Spendable
    // Tomatoes, Lifetime Tomatoes, and the unlocked-tracks list). Returns true
    // on success. After this the server is authoritative for those values.
    [[nodiscard]] virtual bool upload_initial(
        std::string_view auth0_user_id,
        const AccountMigrationState& initial) = 0;
};

// Server-sync orchestrator.
//
// Owns the pending-write queue and the exponential-backoff retry schedule,
// and is the sole writer of the sync_state Phase 0 primitive (read by Z11's
// offline indicator). All testable sync logic lives here: the backoff
// schedule, the sync_state transitions, and the ordered flush of pending
// writes on a successful sync.
class SyncEngine {
public:
    SyncEngine(SyncBackend& server, Clock& clock) noexcept;

    // Record a state change for the logged-in user. Enqueues a snapshot and,
    // when the engine is online or idle (not mid-backoff), immediately
    // attempts to flush. While in backoff the snapshot simply joins the queue
    // and is flushed when the next retry fires.
    void record_state_change(std::string_view auth0_user_id,
                             const AppState& state);

    // Attempt a sync now: push the entire pending queue (ordered, oldest
    // first). On success the queue is flushed, consecutive_failures resets,
    // and sync_state goes SyncOk. On failure sync_state goes SyncFailing,
    // consecutive_failures increments, and the next retry is scheduled per the
    // backoff table. A no-op when there is nothing pending.
    void attempt_sync(std::string_view auth0_user_id);

    // Drive the retry schedule. Z05's main loop calls this each frame; when
    // the engine is in backoff and the scheduled retry time has arrived, it
    // performs another attempt. No real waits anywhere — the schedule is
    // evaluated against the injected clock.
    void pump(std::string_view auth0_user_id);

    // --- Session-start reconcile gate ---
    //
    // The server is the source of truth on reconciliation (Module 7), so the
    // engine withholds every push until a session-start reconcile has
    // succeeded at least once this session. Pushing local writes before the
    // server's authoritative state has been fetched would risk clobbering it
    // (and those writes are superseded by the wholesale reconcile anyway).
    // Local IDBFS persistence is unaffected — it lives upstream of the engine,
    // so writes stay durable locally while the gate is closed.

    // True once a session-start reconcile has succeeded this session. While
    // false, record_state_change and attempt_sync never contact the server.
    [[nodiscard]] bool session_reconciled() const noexcept {
        return session_reconciled_;
    }

    // Open the gate after a successful session-start reconcile (server state
    // adopted, or a brand-new account seeded). Marks the subsystem online and
    // releases subsequent pushes. The reconcile path (AuthManager) calls this.
    void note_session_reconciled() noexcept;

    // Record that a session-start reconcile failed (offline / server error).
    // Surfaces the offline indicator (SyncFailing) and schedules the reconcile
    // retry per the backoff table; the gate stays closed so no push is sent.
    void note_session_reconcile_failed() noexcept;

    // Reset the gate to closed and discard the pending queue (sign-out /
    // delete-account). The next account's session-start reconcile must reopen
    // the gate before that account's writes are pushed.
    void reset_session_gate() noexcept;

    // True when the gate is closed, a reconcile attempt has failed, and the
    // backoff delay has elapsed — i.e. pump_sync should retry the reconcile.
    [[nodiscard]] bool reconcile_retry_due() const noexcept;

    // The steady-clock time the next retry is due. Meaningful only while the
    // most recent attempt failed (sync_state == SyncFailing).
    [[nodiscard]] std::chrono::steady_clock::time_point next_retry_at()
        const noexcept {
        return next_retry_at_;
    }

    // Number of writes currently queued for the next flush.
    [[nodiscard]] std::size_t pending_count() const noexcept {
        return pending_.size();
    }

    // Exponential backoff delay for a given consecutive-failure count:
    // 1 -> 5s, 2 -> 15s, 3 -> 30s, 4+ -> 60s (capped). 0 failures -> 0s.
    [[nodiscard]] static std::chrono::seconds backoff_delay(
        std::uint32_t consecutive_failures) noexcept;

private:
    // Read-modify-write the sync_state primitive after an attempt.
    void mark_success(std::chrono::steady_clock::time_point now) noexcept;
    void mark_failure(std::chrono::steady_clock::time_point now) noexcept;

    SyncBackend& server_;
    Clock& clock_;
    std::vector<AppState> pending_;
    std::chrono::steady_clock::time_point next_retry_at_{};

    // Session-start reconcile gate. Closed (false) at construction: no push
    // reaches the server until the first successful reconcile opens it.
    bool session_reconciled_{false};
};

}  // namespace poker_trainer::persistence
