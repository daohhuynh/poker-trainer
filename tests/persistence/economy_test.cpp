#include "persistence/economy.hpp"

#include "audio/audio_paths.hpp"
#include "persistence/persistence_schema.hpp"

#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

namespace pt = poker_trainer::persistence;
namespace audio = poker_trainer::audio;

// ----- Dual-track award math -----

TEST(Awarded, AddsToBothTracks) {
    const pt::TomatoesState w = pt::awarded(pt::TomatoesState{5, 10}, 3);
    EXPECT_EQ(w.spendable, 8u);
    EXPECT_EQ(w.lifetime, 13u);
}

TEST(Awarded, SaturatesInsteadOfOverflowing) {
    constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();
    const pt::TomatoesState w = pt::awarded(pt::TomatoesState{kMax, kMax}, 5);
    EXPECT_EQ(w.spendable, kMax);
    EXPECT_EQ(w.lifetime, kMax);
}

TEST(ApplyPassAward, IncrementsBothByPerPassAmount) {
    pt::AppState s{};
    pt::apply_pass_award(s);
    EXPECT_EQ(s.tomatoes.spendable, pt::kTomatoesPerPass);
    EXPECT_EQ(s.tomatoes.lifetime, pt::kTomatoesPerPass);
}

TEST(ApplyPassAward, AccumulatesAcrossPasses) {
    pt::AppState s{};
    for (int i = 0; i < 30; ++i) {
        pt::apply_pass_award(s);
    }
    EXPECT_EQ(s.tomatoes.spendable, 30u * pt::kTomatoesPerPass);
    EXPECT_EQ(s.tomatoes.lifetime, 30u * pt::kTomatoesPerPass);
}

// ----- Affordability -----

TEST(CanAfford, ExactBalanceIsAffordable) {
    EXPECT_TRUE(pt::can_afford(pt::TomatoesState{25, 0}, 25));
}

TEST(CanAfford, OneShortIsNotAffordable) {
    EXPECT_FALSE(pt::can_afford(pt::TomatoesState{24, 0}, 25));
}

TEST(CanAfford, FreeIsAlwaysAffordable) {
    EXPECT_TRUE(pt::can_afford(pt::TomatoesState{0, 0}, 0));
}

// ----- Ownership predicates -----

TEST(IsTrackOwned, StarterOwnedWithEmptyUnlockList) {
    pt::MusicLibraryState lib{};
    EXPECT_TRUE(pt::is_track_owned(lib, audio::MusicTrackId::LoungeJazz_Starter));
    EXPECT_TRUE(pt::is_track_owned(lib, audio::MusicTrackId::Ambient_Starter));
}

TEST(IsTrackOwned, PaidTrackNotOwnedUntilUnlocked) {
    pt::MusicLibraryState lib{};
    EXPECT_FALSE(pt::is_track_owned(lib, audio::MusicTrackId::LoungeJazz_Track2));
    lib.unlocked_track_ids = {static_cast<std::uint8_t>(audio::MusicTrackId::LoungeJazz_Track2)};
    EXPECT_TRUE(pt::is_track_owned(lib, audio::MusicTrackId::LoungeJazz_Track2));
}

// ----- Purchase: spend / insufficient funds / dual-track invariants -----

TEST(PurchaseTrack, CommitsDecrementsSpendableAndUnlocks) {
    pt::AppState s{};
    s.tomatoes = pt::TomatoesState{25, 100};
    EXPECT_TRUE(pt::purchase_track(s, audio::MusicTrackId::Classical_Track2));
    EXPECT_EQ(s.tomatoes.spendable, 0u);
    EXPECT_EQ(s.tomatoes.lifetime, 100u);  // spending never reduces the leaderboard metric
    EXPECT_TRUE(pt::is_track_owned(s.music_library, audio::MusicTrackId::Classical_Track2));
    // A freshly bought track is Owned-not-in-shuffle (not yet in rotation).
    EXPECT_FALSE(pt::is_track_in_pool(s.music_library, audio::MusicTrackId::Classical_Track2));
}

TEST(PurchaseTrack, InsufficientFundsLeavesStateUnchanged) {
    pt::AppState s{};
    s.tomatoes = pt::TomatoesState{24, 24};  // one short of the 25-tomato price
    EXPECT_FALSE(pt::purchase_track(s, audio::MusicTrackId::BossaNova_Track3));
    EXPECT_EQ(s.tomatoes.spendable, 24u);
    EXPECT_TRUE(s.music_library.unlocked_track_ids.empty());
}

TEST(PurchaseTrack, AlreadyOwnedIsRejected) {
    pt::AppState s{};
    s.tomatoes = pt::TomatoesState{1000, 1000};
    // Starters are owned by default; buying one is a no-op and never costs tomatoes.
    EXPECT_FALSE(pt::purchase_track(s, audio::MusicTrackId::BossaNova_Starter));
    EXPECT_EQ(s.tomatoes.spendable, 1000u);
    // Buying a paid track twice: the second attempt is rejected.
    EXPECT_TRUE(pt::purchase_track(s, audio::MusicTrackId::Ambient_Track2));
    const std::uint64_t after_first = s.tomatoes.spendable;
    EXPECT_FALSE(pt::purchase_track(s, audio::MusicTrackId::Ambient_Track2));
    EXPECT_EQ(s.tomatoes.spendable, after_first);
}

// ----- Shuffle-pool composition -----

TEST(PoolMutation, AddThenRemoveTogglesRotation) {
    pt::AppState s{};
    s.tomatoes = pt::TomatoesState{25, 25};
    ASSERT_TRUE(pt::purchase_track(s, audio::MusicTrackId::LoungeJazz_Track2));

    pt::add_track_to_pool(s.music_library, audio::MusicTrackId::LoungeJazz_Track2);
    EXPECT_TRUE(pt::is_track_in_pool(s.music_library, audio::MusicTrackId::LoungeJazz_Track2));

    pt::remove_track_from_pool(s.music_library, audio::MusicTrackId::LoungeJazz_Track2);
    EXPECT_FALSE(pt::is_track_in_pool(s.music_library, audio::MusicTrackId::LoungeJazz_Track2));
}

TEST(PoolMutation, AddIsIdempotentAndSorted) {
    pt::MusicLibraryState lib{};
    // Starters are owned, so they can join the pool.
    pt::add_track_to_pool(lib, audio::MusicTrackId::Ambient_Starter);   // id 9
    pt::add_track_to_pool(lib, audio::MusicTrackId::LoungeJazz_Starter); // id 0
    pt::add_track_to_pool(lib, audio::MusicTrackId::Ambient_Starter);   // duplicate
    ASSERT_EQ(lib.active_pool_track_ids.size(), 2u);
    EXPECT_EQ(lib.active_pool_track_ids[0], 0u);  // kept sorted
    EXPECT_EQ(lib.active_pool_track_ids[1], 9u);
}

TEST(PoolMutation, AddUnownedTrackIsNoOp) {
    pt::MusicLibraryState lib{};
    pt::add_track_to_pool(lib, audio::MusicTrackId::Classical_Track3);  // not owned
    EXPECT_TRUE(lib.active_pool_track_ids.empty());
}
