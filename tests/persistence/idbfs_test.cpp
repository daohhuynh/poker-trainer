// IdbfsStore tests: load/save round-trip, immediate write, fresh-state
// fallback, server adoption, wipe, and the tutorial-flag lifecycle.

#include "persistence/idbfs.hpp"

#include "persistence/persistence_schema.hpp"

#include "persistence_mocks.hpp"

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace pt = poker_trainer::persistence;

TEST(Idbfs, LoadReturnsFreshStateWhenStoreEmpty) {
    pt::test::MemoryStorage storage;
    pt::IdbfsStore store(storage);

    const pt::AppState loaded = store.load_state();
    EXPECT_TRUE(pt::test::app_states_equal(loaded, pt::AppState{}));
    EXPECT_FALSE(loaded.account.is_authenticated);
    EXPECT_FALSE(loaded.tutorial.has_seen_tutorial_prompt);
}

TEST(Idbfs, SaveThenReloadIsLossless) {
    pt::test::MemoryStorage storage;
    const pt::AppState original = pt::test::make_populated_state();

    {
        pt::IdbfsStore writer(storage);
        writer.save_state(original);
    }
    // A separate store reading the same backing storage must see it intact.
    pt::IdbfsStore reader(storage);
    const pt::AppState reloaded = reader.load_state();

    EXPECT_TRUE(pt::test::app_states_equal(reloaded, original));
}

TEST(Idbfs, SaveWritesThroughImmediately) {
    pt::test::MemoryStorage storage;
    pt::IdbfsStore store(storage);

    EXPECT_FALSE(storage.blob().has_value());
    store.save_state(pt::test::make_populated_state());
    EXPECT_TRUE(storage.blob().has_value());
    EXPECT_EQ(storage.writes().size(), 1u);
}

TEST(Idbfs, LoadFallsBackToFreshOnCorruptBlob) {
    pt::test::MemoryStorage storage;
    storage.set_raw({0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03, 0x04,
                     0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B});
    pt::IdbfsStore store(storage);

    const pt::AppState loaded = store.load_state();
    EXPECT_TRUE(pt::test::app_states_equal(loaded, pt::AppState{}));
}

TEST(Idbfs, AdoptServerStatePreservesLocalAccountIdentity) {
    pt::test::MemoryStorage storage;
    pt::IdbfsStore store(storage);

    // Local identity: authenticated as sub "local|me".
    pt::AccountState identity{};
    identity.is_authenticated = true;
    identity.auth0_user_id = "local|me";
    identity.display_name = "Me";
    identity.email = "me@example.com";
    store.update_account(identity);

    // Server's authoritative wallet differs, and its blob carries a *different*
    // account that must NOT override the live local identity.
    pt::AppState server_state = pt::test::make_populated_state();
    server_state.tomatoes.spendable = 999;
    server_state.account.auth0_user_id = "server|other";
    store.adopt_server_state(server_state);

    EXPECT_EQ(store.state().tomatoes.spendable, 999u);
    EXPECT_EQ(store.state().account.auth0_user_id, "local|me");
    EXPECT_EQ(store.state().account.display_name, "Me");
    EXPECT_EQ(store.state().schema_version, pt::kCurrentSchemaVersion);
}

TEST(Idbfs, WipeClearsStorageAndResetsToGuest) {
    pt::test::MemoryStorage storage;
    pt::IdbfsStore store(storage);
    store.save_state(pt::test::make_populated_state());

    store.wipe();

    EXPECT_TRUE(storage.cleared());
    EXPECT_FALSE(storage.blob().has_value());
    EXPECT_FALSE(store.state().account.is_authenticated);
    EXPECT_TRUE(pt::test::app_states_equal(store.state(), pt::AppState{}));
}

TEST(Idbfs, TutorialPromptSeenLifecyclePersists) {
    pt::test::MemoryStorage storage;
    pt::IdbfsStore store(storage);

    EXPECT_FALSE(store.has_seen_tutorial_prompt());
    store.mark_tutorial_prompt_seen();
    EXPECT_TRUE(store.has_seen_tutorial_prompt());

    // The flag survives a reload from the same backing storage.
    pt::IdbfsStore reloaded(storage);
    EXPECT_TRUE(reloaded.load_state().tutorial.has_seen_tutorial_prompt);
    EXPECT_TRUE(reloaded.has_seen_tutorial_prompt());
}

TEST(Idbfs, TutorialCompletedLifecyclePersists) {
    pt::test::MemoryStorage storage;
    pt::IdbfsStore store(storage);

    EXPECT_FALSE(store.has_completed_tutorial());
    store.mark_tutorial_completed();
    EXPECT_TRUE(store.has_completed_tutorial());

    pt::IdbfsStore reloaded(storage);
    EXPECT_TRUE(reloaded.load_state().tutorial.has_completed_tutorial);
}

TEST(Idbfs, UpdateAccountPersistsIdentityWithoutTouchingWallet) {
    pt::test::MemoryStorage storage;
    pt::IdbfsStore store(storage);

    pt::AppState wallet_state{};
    wallet_state.tomatoes.spendable = 77;
    wallet_state.tomatoes.lifetime = 88;
    store.save_state(wallet_state);

    pt::AccountState account{};
    account.is_authenticated = true;
    account.auth0_user_id = "auth0|xyz";
    store.update_account(account);

    pt::IdbfsStore reloaded(storage);
    const pt::AppState loaded = reloaded.load_state();
    EXPECT_TRUE(loaded.account.is_authenticated);
    EXPECT_EQ(loaded.account.auth0_user_id, "auth0|xyz");
    EXPECT_EQ(loaded.tomatoes.spendable, 77u);
    EXPECT_EQ(loaded.tomatoes.lifetime, 88u);
}
