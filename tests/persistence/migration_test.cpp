// Migrator tests: the guest->account payload contains exactly the three
// Module 7 fields, uploads through the sync seam, and never mutates local
// state (wallet + lifetime continue from pre-account values).

#include "persistence/migration.hpp"

#include "persistence/persistence_schema.hpp"

#include "persistence_mocks.hpp"

#include <cstdint>

#include <gtest/gtest.h>

namespace pt = poker_trainer::persistence;

namespace {

pt::AppState rich_guest_state() {
    pt::AppState state = pt::test::make_populated_state();
    state.account.is_authenticated = false;  // guest about to create an account
    state.tomatoes.spendable = 120;
    state.tomatoes.lifetime = 500;
    state.music_library.unlocked_track_ids = {1, 3, 5};
    state.music_library.active_pool_track_ids = {0, 9};  // must NOT migrate
    return state;
}

}  // namespace

TEST(Migration, BuildExtractsTheMigrationFields) {
    const pt::AppState state = rich_guest_state();
    const pt::AccountMigrationState payload =
        pt::Migrator::build_migration_state(state);

    // The three value fields plus the account display name (make_populated_state's
    // "TestUser") — the username, not the email, seeds the server row.
    const pt::AccountMigrationState expected{120, 500, {1, 3, 5}, "TestUser"};
    EXPECT_EQ(payload, expected);
}

TEST(Migration, CapturesChosenUsernameAsDisplayName) {
    pt::AppState state = rich_guest_state();
    state.account.display_name = "RyanTheGrinder";  // the user's chosen signup username
    const pt::AccountMigrationState payload =
        pt::Migrator::build_migration_state(state);
    EXPECT_EQ(payload.display_name, "RyanTheGrinder");
}

TEST(Migration, UploadsInitialPayloadThroughSeam) {
    pt::test::MockSyncBackend server;
    server.upload_ok = true;
    pt::Migrator migrator(server);

    const bool ok = migrator.migrate(rich_guest_state(), "auth0|new");

    EXPECT_TRUE(ok);
    ASSERT_EQ(server.uploads.size(), 1u);
    EXPECT_EQ(server.uploads[0].user, "auth0|new");
    EXPECT_EQ(server.uploads[0].payload,
              (pt::AccountMigrationState{120, 500, {1, 3, 5}, "TestUser"}));
}

TEST(Migration, ReturnsFalseWhenUploadFails) {
    pt::test::MockSyncBackend server;
    server.upload_ok = false;
    pt::Migrator migrator(server);

    EXPECT_FALSE(migrator.migrate(rich_guest_state(), "auth0|new"));
}

TEST(Migration, DoesNotMutateLocalWalletOrLifetime) {
    pt::test::MockSyncBackend server;
    pt::Migrator migrator(server);
    const pt::AppState before = rich_guest_state();

    pt::AppState state = before;
    static_cast<void>(migrator.migrate(state, "auth0|new"));

    // Post-migration the server is authoritative, but the local values are
    // untouched so they continue accumulating from pre-account values.
    EXPECT_EQ(state.tomatoes.spendable, before.tomatoes.spendable);
    EXPECT_EQ(state.tomatoes.lifetime, before.tomatoes.lifetime);
    EXPECT_EQ(state.music_library.unlocked_track_ids,
              before.music_library.unlocked_track_ids);
}
