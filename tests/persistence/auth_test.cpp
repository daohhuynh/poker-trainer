// Auth-flow tests through the PersistenceService facade: guest<->logged-in
// transitions, sign-in/sign-up migration vs. reconciliation, sign-out keeping
// IDBFS intact, delete wiping IDBFS, change-password, and the cached
// auth0_health_check.

#include "persistence/persistence_service.hpp"

#include "persistence/auth.hpp"
#include "persistence/auth0_config.hpp"
#include "persistence/persistence_schema.hpp"
#include "persistence/sync.hpp"
#include "persistence/sync_state.hpp"

#include "persistence_mocks.hpp"

#include <chrono>
#include <cstdint>
#include <expected>

#include <gtest/gtest.h>

namespace pt = poker_trainer::persistence;

namespace {

const pt::AuthCredentials kCreds{"user@example.com", "hunter2-Sample!"};

class AuthFlowTest : public ::testing::Test {
protected:
    void SetUp() override { pt::write_sync_state(pt::SyncStateSnapshot{}); }

    pt::PersistenceService make_service() {
        return pt::PersistenceService(storage_, auth_, server_, clock_);
    }

    pt::test::MemoryStorage storage_;
    pt::test::MockAuthBackend auth_;
    pt::test::MockSyncBackend server_;
    pt::test::ManualClock clock_;
};

}  // namespace

TEST_F(AuthFlowTest, SignInExistingAccountReconcilesServerAsAuthoritative) {
    // Server already holds account state -> adopt it (server is source truth).
    pt::AppState server_state{};
    server_state.tomatoes.spendable = 999;
    server_state.tomatoes.lifetime = 4242;
    server_.fetch_result = pt::FetchResult{pt::FetchOutcome::Found, server_state};

    pt::PersistenceService svc = make_service();
    svc.load_state();  // start as guest

    const std::expected<void, pt::AuthError> result = svc.sign_in(kCreds);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(svc.state().account.is_authenticated);
    EXPECT_EQ(svc.state().account.auth0_user_id, "sub-123");
    EXPECT_EQ(svc.state().tomatoes.spendable, 999u);
    EXPECT_EQ(svc.state().tomatoes.lifetime, 4242u);
    EXPECT_TRUE(server_.uploads.empty());  // adopted, not migrated
}

TEST_F(AuthFlowTest, SignInFromGuestWithNoServerStateTriggersMigration) {
    server_.fetch_result = pt::FetchResult{pt::FetchOutcome::NotFound, {}};

    pt::PersistenceService svc = make_service();
    svc.load_state();
    pt::AppState guest{};
    guest.tomatoes.spendable = 40;
    guest.tomatoes.lifetime = 90;
    guest.music_library.unlocked_track_ids = {2, 4};
    svc.save_state(guest);  // guest: persisted locally, no sync

    const std::expected<void, pt::AuthError> result = svc.sign_in(kCreds);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(server_.uploads.size(), 1u);
    EXPECT_EQ(server_.uploads[0].payload,
              (pt::AccountMigrationState{40, 90, {2, 4}}));
    // Wallet continues from pre-account values.
    EXPECT_EQ(svc.state().tomatoes.spendable, 40u);
}

TEST_F(AuthFlowTest, SignUpFromGuestSeedsServerViaMigration) {
    server_.fetch_result = pt::FetchResult{pt::FetchOutcome::NotFound, {}};

    pt::PersistenceService svc = make_service();
    svc.load_state();
    pt::AppState guest{};
    guest.tomatoes.spendable = 50;
    guest.tomatoes.lifetime = 200;
    guest.music_library.unlocked_track_ids = {1, 9};
    svc.save_state(guest);

    const std::expected<void, pt::AuthError> result =
        svc.sign_up(kCreds, "Alice");

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(svc.state().account.is_authenticated);
    ASSERT_EQ(server_.uploads.size(), 1u);
    EXPECT_EQ(server_.uploads[0].payload,
              (pt::AccountMigrationState{50, 200, {1, 9}}));
    EXPECT_EQ(auth_.display_names_seen.size(), 1u);
    EXPECT_EQ(auth_.display_names_seen[0], "Alice");
}

TEST_F(AuthFlowTest, SignInFailurePropagatesErrorAndStaysGuest) {
    auth_.sign_in_ok = false;
    auth_.error = pt::AuthError::InvalidCredentials;

    pt::PersistenceService svc = make_service();
    svc.load_state();
    const std::expected<void, pt::AuthError> result = svc.sign_in(kCreds);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), pt::AuthError::InvalidCredentials);
    EXPECT_FALSE(svc.state().account.is_authenticated);
    EXPECT_TRUE(server_.uploads.empty());
}

TEST_F(AuthFlowTest, SignOutKeepsIdbfsIntactAsGuest) {
    server_.fetch_result = pt::FetchResult{pt::FetchOutcome::NotFound, {}};
    pt::PersistenceService svc = make_service();
    svc.load_state();
    pt::AppState guest{};
    guest.tomatoes.spendable = 314;
    svc.save_state(guest);
    ASSERT_TRUE(svc.sign_in(kCreds).has_value());

    const std::expected<void, pt::AuthError> result = svc.sign_out();

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(svc.state().account.is_authenticated);
    EXPECT_FALSE(storage_.cleared());          // IDBFS intact
    EXPECT_TRUE(storage_.blob().has_value());
    EXPECT_EQ(svc.state().tomatoes.spendable, 314u);  // wallet persists locally
}

TEST_F(AuthFlowTest, DeleteAccountWipesIdbfsToGuest) {
    server_.fetch_result = pt::FetchResult{pt::FetchOutcome::Found, pt::AppState{}};
    pt::PersistenceService svc = make_service();
    svc.load_state();
    ASSERT_TRUE(svc.sign_in(kCreds).has_value());

    const std::expected<void, pt::AuthError> result = svc.delete_account();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(auth_.deleted_user_id, "sub-123");
    EXPECT_TRUE(storage_.cleared());
    EXPECT_FALSE(storage_.blob().has_value());
    EXPECT_FALSE(svc.state().account.is_authenticated);
    EXPECT_TRUE(pt::test::app_states_equal(svc.state(), pt::AppState{}));
}

TEST_F(AuthFlowTest, DeleteAccountWhenGuestReturnsNotAuthenticated) {
    pt::PersistenceService svc = make_service();
    svc.load_state();
    const std::expected<void, pt::AuthError> result = svc.delete_account();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), pt::AuthError::NotAuthenticated);
}

TEST_F(AuthFlowTest, ChangePasswordTriggersResetEmailForCurrentUser) {
    server_.fetch_result = pt::FetchResult{pt::FetchOutcome::Found, pt::AppState{}};
    pt::PersistenceService svc = make_service();
    svc.load_state();
    ASSERT_TRUE(svc.sign_in(kCreds).has_value());  // session email = mock's

    const std::expected<void, pt::AuthError> result = svc.change_password();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(auth_.change_password_email, "alice@example.com");
}

TEST_F(AuthFlowTest, ChangePasswordWhenGuestReturnsNotAuthenticated) {
    pt::PersistenceService svc = make_service();
    svc.load_state();
    const std::expected<void, pt::AuthError> result = svc.change_password();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), pt::AuthError::NotAuthenticated);
}

TEST_F(AuthFlowTest, HealthCheckReportsHealthy) {
    auth_.healthy = true;
    pt::PersistenceService svc = make_service();

    EXPECT_TRUE(svc.auth0_health_check());
    EXPECT_EQ(auth_.health_calls, 1);
}

TEST_F(AuthFlowTest, UnhealthyResultIsNotCachedAndReprobesEachCall) {
    // No prior successful probe, so nothing is cached; each call re-probes and
    // a failure is never cached (so a recovering Auth0 is noticed promptly).
    auth_.healthy = false;
    pt::PersistenceService svc = make_service();

    EXPECT_FALSE(svc.auth0_health_check());
    EXPECT_EQ(auth_.health_calls, 1);
    clock_.advance(std::chrono::seconds{1});
    EXPECT_FALSE(svc.auth0_health_check());
    EXPECT_EQ(auth_.health_calls, 2);
}

TEST_F(AuthFlowTest, HealthCheckCachesSuccessWithinTtl) {
    auth_.healthy = true;
    pt::PersistenceService svc = make_service();

    EXPECT_TRUE(svc.auth0_health_check());
    EXPECT_EQ(auth_.health_calls, 1);

    // Within the cache TTL the cached healthy result short-circuits the probe.
    clock_.advance(pt::kAuth0HealthCheckCacheTtl - std::chrono::seconds{1});
    EXPECT_TRUE(svc.auth0_health_check());
    EXPECT_EQ(auth_.health_calls, 1);

    // Past the TTL it re-probes.
    clock_.advance(std::chrono::seconds{2});
    EXPECT_TRUE(svc.auth0_health_check());
    EXPECT_EQ(auth_.health_calls, 2);
}

TEST_F(AuthFlowTest, OfflineAtSessionStartWithholdsPushesUntilReconcileSucceeds) {
    using namespace std::chrono_literals;

    // Sign-in succeeds at Auth0 but the state fetch fails (offline at session
    // start). The user is authenticated and proceeds on local state; the
    // offline indicator shows (SyncFailing) and the push gate stays closed.
    server_.fetch_result = pt::FetchResult{pt::FetchOutcome::Failed, {}};
    pt::PersistenceService svc = make_service();
    svc.load_state();
    ASSERT_TRUE(svc.sign_in(kCreds).has_value());
    EXPECT_TRUE(svc.state().account.is_authenticated);
    EXPECT_EQ(pt::read_sync_state().status, pt::SyncStatus::SyncFailing);

    // The user trains: the write is durable in IDBFS but nothing is pushed.
    pt::AppState s1 = svc.state();
    s1.tomatoes.spendable = 7;
    svc.save_state(s1);
    EXPECT_TRUE(server_.pushes.empty());
    EXPECT_TRUE(storage_.blob().has_value());  // durable locally

    // pump_sync before the backoff elapses does not retry the reconcile.
    svc.pump_sync();
    EXPECT_EQ(server_.fetch_calls, 1);  // only the original sign-in fetch
    EXPECT_TRUE(server_.pushes.empty());

    // Connectivity returns. After the backoff, pump_sync retries the reconcile;
    // the server is reachable and authoritative, so the gate opens and its
    // state is adopted wholesale.
    pt::AppState server_state{};
    server_state.tomatoes.spendable = 500;
    server_state.tomatoes.lifetime = 900;
    server_.fetch_result = pt::FetchResult{pt::FetchOutcome::Found, server_state};
    clock_.advance(6s);  // past the 5s first-retry delay
    svc.pump_sync();
    EXPECT_EQ(server_.fetch_calls, 2);  // reconcile retried
    EXPECT_EQ(pt::read_sync_state().status, pt::SyncStatus::SyncOk);
    EXPECT_EQ(svc.state().tomatoes.spendable, 500u);  // server adopted

    // Thereafter pushes flow as before.
    pt::AppState s2 = svc.state();
    s2.tomatoes.spendable = 501;
    svc.save_state(s2);
    ASSERT_FALSE(server_.pushes.empty());
    EXPECT_EQ(server_.pushes.back().writes.back().tomatoes.spendable, 501u);
}
