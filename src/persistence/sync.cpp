#include "persistence/sync.hpp"

#include "persistence/clock.hpp"
#include "persistence/persistence_schema.hpp"
#include "persistence/sync_state.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace poker_trainer::persistence {

SyncEngine::SyncEngine(SyncBackend& server, Clock& clock) noexcept
    : server_(server), clock_(clock) {}

std::chrono::seconds SyncEngine::backoff_delay(
    std::uint32_t consecutive_failures) noexcept {
    if (consecutive_failures == 0) {
        return std::chrono::seconds{0};
    }
    // 1 -> 5s, 2 -> 15s, 3 -> 30s, 4+ -> 60s. The final entry is the cap.
    constexpr std::array<std::chrono::seconds, 4> kSchedule{
        std::chrono::seconds{5}, std::chrono::seconds{15},
        std::chrono::seconds{30}, std::chrono::seconds{60}};
    const std::size_t index = std::min<std::size_t>(
        static_cast<std::size_t>(consecutive_failures - 1u),
        kSchedule.size() - 1);
    return kSchedule[index];
}

void SyncEngine::record_state_change(std::string_view auth0_user_id,
                                     const AppState& state) {
    // Session-start gate: before the first successful reconcile this session,
    // the write is already durable in IDBFS (upstream of this engine) but is
    // not queued for the server — the wholesale server-authoritative reconcile
    // would supersede it, and pushing it could clobber authoritative state.
    if (!session_reconciled_) {
        return;
    }

    pending_.push_back(state);

    // While in backoff (the most recent attempt failed) the new write simply
    // joins the queue; pump() flushes the whole queue when the scheduled
    // retry fires. Otherwise sync promptly so the server stays current.
    const SyncStateSnapshot snapshot = read_sync_state();
    if (snapshot.status != SyncStatus::SyncFailing) {
        attempt_sync(auth0_user_id);
    }
}

void SyncEngine::attempt_sync(std::string_view auth0_user_id) {
    if (pending_.empty()) {
        return;
    }

    // Session-start gate (hard enforcement point): no push reaches the server
    // until a reconcile has succeeded this session. record_state_change avoids
    // queuing while gated, so this guard is the belt-and-suspenders backstop
    // for any direct caller.
    if (!session_reconciled_) {
        return;
    }

    // Mark in-flight (indicator hidden) before contacting the server.
    SyncStateSnapshot in_progress = read_sync_state();
    in_progress.status = SyncStatus::SyncInProgress;
    write_sync_state(in_progress);

    const std::chrono::steady_clock::time_point now = clock_.now();
    const bool accepted =
        server_.push(auth0_user_id, std::span<const AppState>(pending_));
    if (accepted) {
        // Ordered batch durably accepted: flush the queue.
        pending_.clear();
        mark_success(now);
    } else {
        mark_failure(now);
    }
}

void SyncEngine::pump(std::string_view auth0_user_id) {
    const SyncStateSnapshot snapshot = read_sync_state();
    if (snapshot.status == SyncStatus::SyncFailing && !pending_.empty() &&
        clock_.now() >= next_retry_at_) {
        attempt_sync(auth0_user_id);
    }
}

void SyncEngine::mark_success(
    std::chrono::steady_clock::time_point now) noexcept {
    SyncStateSnapshot snapshot = read_sync_state();
    snapshot.status = SyncStatus::SyncOk;
    snapshot.last_success = now;
    snapshot.consecutive_failures = 0;
    write_sync_state(snapshot);
    next_retry_at_ = std::chrono::steady_clock::time_point{};
}

void SyncEngine::mark_failure(
    std::chrono::steady_clock::time_point now) noexcept {
    SyncStateSnapshot snapshot = read_sync_state();
    snapshot.status = SyncStatus::SyncFailing;
    snapshot.last_failure = now;
    snapshot.consecutive_failures += 1;
    write_sync_state(snapshot);
    next_retry_at_ = now + backoff_delay(snapshot.consecutive_failures);
}

void SyncEngine::note_session_reconciled() noexcept {
    // A successful session-start reconcile establishes the server-authoritative
    // baseline. Open the gate and mark the subsystem online; this also clears a
    // prior SyncFailing left by failed reconcile attempts (offline indicator
    // off). Nothing is queued while gated, so there is no pre-reconcile batch
    // to flush — and flushing it would be wrong: the wholesale reconcile has
    // already superseded those writes.
    session_reconciled_ = true;
    mark_success(clock_.now());
}

void SyncEngine::note_session_reconcile_failed() noexcept {
    // Offline / server error at session start: keep the gate closed (no push)
    // and drive the backoff so pump_sync retries the reconcile. SyncFailing
    // surfaces the offline indicator.
    mark_failure(clock_.now());
}

void SyncEngine::reset_session_gate() noexcept {
    // Sign-out / delete-account: re-close the gate and discard the pending
    // queue so a different account cannot inherit either. The next account's
    // session-start reconcile must reopen the gate before its writes push.
    session_reconciled_ = false;
    pending_.clear();
    next_retry_at_ = std::chrono::steady_clock::time_point{};
    write_sync_state(SyncStateSnapshot{});
}

bool SyncEngine::reconcile_retry_due() const noexcept {
    if (session_reconciled_) {
        return false;
    }
    const SyncStateSnapshot snapshot = read_sync_state();
    return snapshot.status == SyncStatus::SyncFailing &&
           clock_.now() >= next_retry_at_;
}

}  // namespace poker_trainer::persistence
