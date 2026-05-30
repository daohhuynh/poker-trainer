// SyncEngine tests: exponential-backoff schedule, sync_state transitions on
// failure/success, and ordered pending-write flush. All timing is driven by
// the injected ManualClock — no real waits.

#include "persistence/sync.hpp"

#include "persistence/persistence_schema.hpp"
#include "persistence/sync_state.hpp"

#include "persistence_mocks.hpp"

#include <chrono>
#include <cstdint>

#include <gtest/gtest.h>

namespace pt = poker_trainer::persistence;

namespace {

constexpr std::string_view kUser = "auth0|user";

pt::AppState state_with_spendable(std::uint64_t spendable) {
    pt::AppState state{};
    state.account.is_authenticated = true;
    state.account.auth0_user_id = std::string(kUser);
    state.tomatoes.spendable = spendable;
    return state;
}

class SyncEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // The sync_state primitive is a process-global seqlock; reset it so
        // each test starts from a clean Idle snapshot.
        pt::write_sync_state(pt::SyncStateSnapshot{});
    }

    pt::test::MockSyncBackend server_;
    pt::test::ManualClock clock_;
};

}  // namespace

TEST_F(SyncEngineTest, BackoffScheduleMatchesSpec) {
    using namespace std::chrono_literals;
    EXPECT_EQ(pt::SyncEngine::backoff_delay(0), 0s);
    EXPECT_EQ(pt::SyncEngine::backoff_delay(1), 5s);
    EXPECT_EQ(pt::SyncEngine::backoff_delay(2), 15s);
    EXPECT_EQ(pt::SyncEngine::backoff_delay(3), 30s);
    EXPECT_EQ(pt::SyncEngine::backoff_delay(4), 60s);
    EXPECT_EQ(pt::SyncEngine::backoff_delay(5), 60s);   // capped
    EXPECT_EQ(pt::SyncEngine::backoff_delay(100), 60s);  // capped
}

TEST_F(SyncEngineTest, FailureMarksOfflineAndSchedulesFirstRetry) {
    using namespace std::chrono_literals;
    server_.push_ok = false;
    pt::SyncEngine engine(server_, clock_);

    engine.record_state_change(kUser, state_with_spendable(1));

    const pt::SyncStateSnapshot snap = pt::read_sync_state();
    EXPECT_EQ(snap.status, pt::SyncStatus::SyncFailing);
    EXPECT_EQ(snap.consecutive_failures, 1u);
    EXPECT_EQ(engine.next_retry_at() - clock_.now(), 5s);
    EXPECT_EQ(engine.pending_count(), 1u);  // failed write stays queued
}

TEST_F(SyncEngineTest, SuccessMarksOnlineAndClearsQueue) {
    server_.push_ok = true;
    pt::SyncEngine engine(server_, clock_);

    engine.record_state_change(kUser, state_with_spendable(1));

    const pt::SyncStateSnapshot snap = pt::read_sync_state();
    EXPECT_EQ(snap.status, pt::SyncStatus::SyncOk);
    EXPECT_EQ(snap.consecutive_failures, 0u);
    EXPECT_EQ(engine.pending_count(), 0u);
}

TEST_F(SyncEngineTest, BackoffProgressesAcrossRepeatedFailures) {
    using namespace std::chrono_literals;
    server_.push_ok = false;
    pt::SyncEngine engine(server_, clock_);

    // First failure via record; subsequent failures via direct attempts (the
    // write stays queued, so attempt_sync keeps retrying the same batch).
    engine.record_state_change(kUser, state_with_spendable(1));
    EXPECT_EQ(engine.next_retry_at() - clock_.now(), 5s);

    engine.attempt_sync(kUser);
    EXPECT_EQ(engine.next_retry_at() - clock_.now(), 15s);

    engine.attempt_sync(kUser);
    EXPECT_EQ(engine.next_retry_at() - clock_.now(), 30s);

    engine.attempt_sync(kUser);
    EXPECT_EQ(engine.next_retry_at() - clock_.now(), 60s);

    engine.attempt_sync(kUser);
    EXPECT_EQ(engine.next_retry_at() - clock_.now(), 60s);  // capped

    EXPECT_EQ(pt::read_sync_state().consecutive_failures, 5u);
}

TEST_F(SyncEngineTest, PendingWritesFlushInOrderOnRecovery) {
    using namespace std::chrono_literals;
    server_.push_ok = false;
    pt::SyncEngine engine(server_, clock_);

    // Three changes while offline accumulate in order.
    engine.record_state_change(kUser, state_with_spendable(1));  // attempts+fails
    engine.record_state_change(kUser, state_with_spendable(2));  // queued only
    engine.record_state_change(kUser, state_with_spendable(3));  // queued only
    EXPECT_EQ(engine.pending_count(), 3u);
    EXPECT_EQ(pt::read_sync_state().status, pt::SyncStatus::SyncFailing);

    // Server recovers; the scheduled retry fires.
    server_.push_ok = true;
    clock_.advance(6s);  // past the 5s first-retry delay
    engine.pump(kUser);

    EXPECT_EQ(engine.pending_count(), 0u);
    EXPECT_EQ(pt::read_sync_state().status, pt::SyncStatus::SyncOk);

    // The final (accepted) push carried all three writes oldest-first.
    ASSERT_FALSE(server_.pushes.empty());
    const pt::test::MockSyncBackend::PushRecord& flushed = server_.pushes.back();
    EXPECT_TRUE(flushed.accepted);
    ASSERT_EQ(flushed.writes.size(), 3u);
    EXPECT_EQ(flushed.writes[0].tomatoes.spendable, 1u);
    EXPECT_EQ(flushed.writes[1].tomatoes.spendable, 2u);
    EXPECT_EQ(flushed.writes[2].tomatoes.spendable, 3u);
}

TEST_F(SyncEngineTest, RecordWhileFailingDoesNotAttemptUntilPump) {
    server_.push_ok = false;
    pt::SyncEngine engine(server_, clock_);

    engine.record_state_change(kUser, state_with_spendable(1));  // 1 attempt
    const std::size_t after_first = server_.pushes.size();
    engine.record_state_change(kUser, state_with_spendable(2));  // no attempt
    engine.record_state_change(kUser, state_with_spendable(3));  // no attempt

    EXPECT_EQ(server_.pushes.size(), after_first);
    EXPECT_EQ(engine.pending_count(), 3u);
}

TEST_F(SyncEngineTest, PumpIsNoopBeforeScheduledRetry) {
    using namespace std::chrono_literals;
    server_.push_ok = false;
    pt::SyncEngine engine(server_, clock_);
    engine.record_state_change(kUser, state_with_spendable(1));

    const std::size_t before = server_.pushes.size();
    clock_.advance(1s);  // well before the 5s retry
    engine.pump(kUser);
    EXPECT_EQ(server_.pushes.size(), before);  // no extra attempt
}

TEST_F(SyncEngineTest, AttemptIsNoopWhenNothingPending) {
    pt::SyncEngine engine(server_, clock_);
    engine.attempt_sync(kUser);

    EXPECT_TRUE(server_.pushes.empty());
    EXPECT_EQ(pt::read_sync_state().status, pt::SyncStatus::Idle);
}
