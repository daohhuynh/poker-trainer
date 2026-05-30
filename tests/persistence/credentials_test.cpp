// Credential-safety tests: no raw password or access token is EVER written
// through the storage seam, and no credential leaks into a server sync
// payload. Auth0 owns credential storage; Z04 persists only non-sensitive
// app state. Credentials are forwarded across the auth seam (proving the flow
// works) but must never reach IDBFS or the server-state blobs.

#include "persistence/persistence_service.hpp"

#include "persistence/auth.hpp"
#include "persistence/idbfs.hpp"
#include "persistence/persistence_schema.hpp"
#include "persistence/sync.hpp"
#include "persistence/sync_state.hpp"

#include "persistence_mocks.hpp"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

namespace pt = poker_trainer::persistence;

namespace {

// Distinctive sentinels that would never occur incidentally in a serialized
// AppState.
constexpr std::string_view kPassword = "P@ssw0rd-SENTINEL-9z";
constexpr std::string_view kToken = "ACCESS-TOKEN-SENTINEL-7y";

}  // namespace

TEST(Credentials, PasswordAndTokenNeverWrittenThroughStorageSeam) {
    pt::write_sync_state(pt::SyncStateSnapshot{});
    pt::test::MemoryStorage storage;
    pt::test::MockAuthBackend auth;
    pt::test::MockSyncBackend server;
    pt::test::ManualClock clock;

    auth.session =
        pt::AuthSession{"sub-xyz", "Bob", "bob@example.com", std::string(kToken)};
    server.fetch_result = pt::FetchResult{pt::FetchOutcome::NotFound, {}};

    pt::PersistenceService svc(storage, auth, server, clock);
    svc.load_state();

    pt::AppState guest{};
    guest.tomatoes.spendable = 10;
    svc.save_state(guest);

    const pt::AuthCredentials creds{"bob@example.com", std::string(kPassword)};
    ASSERT_TRUE(svc.sign_up(creds, "Bob").has_value());

    // A further state change that drives a logged-in server sync.
    pt::AppState updated = svc.state();
    updated.tomatoes.spendable = 20;
    svc.save_state(updated);

    // The password WAS forwarded across the auth seam (the flow works)...
    ASSERT_FALSE(auth.passwords_seen.empty());
    EXPECT_EQ(auth.passwords_seen.back(), kPassword);

    // ...but neither the password nor the token ever reached the storage seam.
    ASSERT_FALSE(storage.writes().empty());
    for (const std::vector<std::uint8_t>& blob : storage.writes()) {
        EXPECT_FALSE(pt::test::bytes_contain(blob, kPassword));
        EXPECT_FALSE(pt::test::bytes_contain(blob, kToken));
    }
    ASSERT_TRUE(storage.blob().has_value());
    EXPECT_FALSE(pt::test::bytes_contain(*storage.blob(), kPassword));
    EXPECT_FALSE(pt::test::bytes_contain(*storage.blob(), kToken));
}

TEST(Credentials, TokenNeverPersistedAfterSignIn) {
    pt::write_sync_state(pt::SyncStateSnapshot{});
    pt::test::MemoryStorage storage;
    pt::test::MockAuthBackend auth;
    pt::test::MockSyncBackend server;
    pt::test::ManualClock clock;

    auth.session = pt::AuthSession{"sub-1", "Carol", "carol@example.com",
                                   std::string(kToken)};
    server.fetch_result = pt::FetchResult{pt::FetchOutcome::Found, pt::AppState{}};

    pt::PersistenceService svc(storage, auth, server, clock);
    svc.load_state();
    ASSERT_TRUE(svc.sign_in({"carol@example.com", std::string(kPassword)})
                    .has_value());

    // Identity (sub/display/email) is persisted; the access token is not.
    const std::vector<std::uint8_t> snapshot =
        pt::serialize_app_state(svc.state());
    EXPECT_FALSE(pt::test::bytes_contain(snapshot, kToken));
    EXPECT_FALSE(pt::test::bytes_contain(snapshot, kPassword));
    EXPECT_TRUE(pt::test::bytes_contain(snapshot, "carol@example.com"));
}

TEST(Credentials, ServerSyncPayloadsCarryNoCredentials) {
    pt::write_sync_state(pt::SyncStateSnapshot{});
    pt::test::MemoryStorage storage;
    pt::test::MockAuthBackend auth;
    pt::test::MockSyncBackend server;
    pt::test::ManualClock clock;

    auth.session = pt::AuthSession{"sub-2", "Dave", "dave@example.com",
                                   std::string(kToken)};
    server.fetch_result = pt::FetchResult{pt::FetchOutcome::NotFound, {}};

    pt::PersistenceService svc(storage, auth, server, clock);
    svc.load_state();
    pt::AppState guest{};
    guest.tomatoes.spendable = 5;
    svc.save_state(guest);
    ASSERT_TRUE(svc.sign_up({"dave@example.com", std::string(kPassword)}, "Dave")
                    .has_value());

    pt::AppState updated = svc.state();
    updated.tomatoes.spendable = 6;
    svc.save_state(updated);

    // Server-bound full-state snapshots carry no credentials.
    for (const pt::test::MockSyncBackend::PushRecord& push : server.pushes) {
        for (const pt::AppState& written : push.writes) {
            const std::vector<std::uint8_t> blob =
                pt::serialize_app_state(written);
            EXPECT_FALSE(pt::test::bytes_contain(blob, kPassword));
            EXPECT_FALSE(pt::test::bytes_contain(blob, kToken));
        }
    }
    // The migration payload structurally cannot hold credentials (only two
    // counters and a track-id list), so the upload path is credential-free.
    EXPECT_FALSE(server.uploads.empty());
}
