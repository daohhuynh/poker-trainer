#include "persistence/economy.hpp"

#include "audio/audio_paths.hpp"
#include "persistence/persistence_schema.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

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

// ----- Track pricing (position-based: free / 5 / 10 per genre) -----

TEST(TrackPrice, StarterIsFree) {
    EXPECT_EQ(pt::track_price(audio::MusicTrackId::LoungeJazz_Starter), 0u);
    EXPECT_EQ(pt::track_price(audio::MusicTrackId::Classical_Starter), 0u);
    EXPECT_EQ(pt::track_price(audio::MusicTrackId::BossaNova_Starter), 0u);
    EXPECT_EQ(pt::track_price(audio::MusicTrackId::Ambient_Starter), 0u);
}

TEST(TrackPrice, SecondTrackOfEachGenreIsFive) {
    EXPECT_EQ(pt::track_price(audio::MusicTrackId::LoungeJazz_Track2), 5u);
    EXPECT_EQ(pt::track_price(audio::MusicTrackId::Classical_Track2), 5u);
    EXPECT_EQ(pt::track_price(audio::MusicTrackId::BossaNova_Track2), 5u);
    EXPECT_EQ(pt::track_price(audio::MusicTrackId::Ambient_Track2), 5u);
}

TEST(TrackPrice, ThirdTrackOfEachGenreIsTen) {
    EXPECT_EQ(pt::track_price(audio::MusicTrackId::LoungeJazz_Track3), 10u);
    EXPECT_EQ(pt::track_price(audio::MusicTrackId::Classical_Track3), 10u);
    EXPECT_EQ(pt::track_price(audio::MusicTrackId::BossaNova_Track3), 10u);
    EXPECT_EQ(pt::track_price(audio::MusicTrackId::Ambient_Track3), 10u);
}

TEST(TrackPrice, PaidCatalogTotalsSixty) {
    std::uint64_t total = 0;
    for (std::size_t i = 0; i < audio::kMusicTrackCount; ++i) {
        total += pt::track_price(static_cast<audio::MusicTrackId>(i));
    }
    EXPECT_EQ(total, 60u);  // 4 genres x (5 + 10)
}

// ----- Affordability -----

TEST(CanAfford, ExactBalanceIsAffordable) {
    EXPECT_TRUE(pt::can_afford(pt::TomatoesState{10, 0}, 10));
}

TEST(CanAfford, OneShortIsNotAffordable) {
    EXPECT_FALSE(pt::can_afford(pt::TomatoesState{9, 0}, 10));
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
    // Classical_Track2 is the genre's second track (price 5): 25 - 5 = 20 spendable.
    EXPECT_TRUE(pt::purchase_track(s, audio::MusicTrackId::Classical_Track2));
    EXPECT_EQ(s.tomatoes.spendable, 20u);
    EXPECT_EQ(s.tomatoes.lifetime, 100u);  // spending never reduces the leaderboard metric
    EXPECT_TRUE(pt::is_track_owned(s.music_library, audio::MusicTrackId::Classical_Track2));
    // A freshly bought track is Owned-not-in-shuffle (not yet in rotation).
    EXPECT_FALSE(pt::is_track_in_pool(s.music_library, audio::MusicTrackId::Classical_Track2));
}

TEST(PurchaseTrack, InsufficientFundsLeavesStateUnchanged) {
    pt::AppState s{};
    s.tomatoes = pt::TomatoesState{9, 9};  // one short of the 10-tomato third-track price
    EXPECT_FALSE(pt::purchase_track(s, audio::MusicTrackId::BossaNova_Track3));
    EXPECT_EQ(s.tomatoes.spendable, 9u);
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

// ----- Rotation composition (one global, cross-genre, ordered by add order) -----

TEST(PoolMutation, AddThenRemoveTogglesRotation) {
    pt::AppState s{};
    s.tomatoes = pt::TomatoesState{25, 25};
    ASSERT_TRUE(pt::purchase_track(s, audio::MusicTrackId::LoungeJazz_Track2));

    pt::add_track_to_pool(s.music_library, audio::MusicTrackId::LoungeJazz_Track2);
    EXPECT_TRUE(pt::is_track_in_pool(s.music_library, audio::MusicTrackId::LoungeJazz_Track2));

    pt::remove_track_from_pool(s.music_library, audio::MusicTrackId::LoungeJazz_Track2);
    EXPECT_FALSE(pt::is_track_in_pool(s.music_library, audio::MusicTrackId::LoungeJazz_Track2));
}

// Supersedes an earlier "kept sorted" assertion: the rotation is a playlist, and Loop
// playback walks it in the order the user built it. Sorting would silently reorder that.
TEST(PoolMutation, AddAppendsInAddOrderAndIsIdempotent) {
    pt::MusicLibraryState lib{};
    // Starters are owned, so they can join the rotation.
    pt::add_track_to_pool(lib, audio::MusicTrackId::Ambient_Starter);    // id 9
    pt::add_track_to_pool(lib, audio::MusicTrackId::LoungeJazz_Starter); // id 0
    pt::add_track_to_pool(lib, audio::MusicTrackId::Ambient_Starter);    // duplicate
    ASSERT_EQ(lib.active_pool_track_ids.size(), 2u);
    EXPECT_EQ(lib.active_pool_track_ids[0], 9u);  // added first, stays first
    EXPECT_EQ(lib.active_pool_track_ids[1], 0u);
}

// Re-adding a member must not shuffle it to the back either — add is a no-op, not a move.
TEST(PoolMutation, ReAddingAMemberDoesNotMoveIt) {
    pt::MusicLibraryState lib{};
    pt::add_track_to_pool(lib, audio::MusicTrackId::LoungeJazz_Starter);
    pt::add_track_to_pool(lib, audio::MusicTrackId::Classical_Starter);
    pt::add_track_to_pool(lib, audio::MusicTrackId::LoungeJazz_Starter);
    ASSERT_EQ(lib.active_pool_track_ids.size(), 2u);
    EXPECT_EQ(lib.active_pool_track_ids[0],
              static_cast<std::uint8_t>(audio::MusicTrackId::LoungeJazz_Starter));
}

TEST(PoolMutation, RemoveKeepsTheOrderOfTheSurvivors) {
    pt::MusicLibraryState lib{};
    pt::add_track_to_pool(lib, audio::MusicTrackId::Ambient_Starter);     // 9
    pt::add_track_to_pool(lib, audio::MusicTrackId::LoungeJazz_Starter);  // 0
    pt::add_track_to_pool(lib, audio::MusicTrackId::BossaNova_Starter);   // 6
    pt::remove_track_from_pool(lib, audio::MusicTrackId::LoungeJazz_Starter);
    ASSERT_EQ(lib.active_pool_track_ids.size(), 2u);
    EXPECT_EQ(lib.active_pool_track_ids[0], 9u);
    EXPECT_EQ(lib.active_pool_track_ids[1], 6u);
}

TEST(PoolMutation, AddUnownedTrackIsNoOp) {
    pt::MusicLibraryState lib{};
    pt::add_track_to_pool(lib, audio::MusicTrackId::Classical_Track3);  // not owned
    EXPECT_TRUE(lib.active_pool_track_ids.empty());
}

TEST(RotationTracks, ReturnsTypedIdsInAddOrder) {
    pt::MusicLibraryState lib{};
    pt::add_track_to_pool(lib, audio::MusicTrackId::BossaNova_Starter);
    pt::add_track_to_pool(lib, audio::MusicTrackId::Classical_Starter);
    const std::vector<audio::MusicTrackId> tracks = pt::rotation_tracks(lib);
    ASSERT_EQ(tracks.size(), 2u);
    EXPECT_EQ(tracks[0], audio::MusicTrackId::BossaNova_Starter);
    EXPECT_EQ(tracks[1], audio::MusicTrackId::Classical_Starter);
}

TEST(RotationTracks, SkipsIdsTheCatalogDoesNotDefine) {
    pt::MusicLibraryState lib{};
    lib.active_pool_track_ids = {0, 200, 3};
    const std::vector<audio::MusicTrackId> tracks = pt::rotation_tracks(lib);
    ASSERT_EQ(tracks.size(), 2u);
    EXPECT_EQ(tracks[0], audio::MusicTrackId::LoungeJazz_Starter);
    EXPECT_EQ(tracks[1], audio::MusicTrackId::Classical_Starter);
}

// ----- Rotation normalization / migration of pre-rotation profiles -----

// The old semantics stored the per-genre pools unioned into one sorted set. Those ids are
// adopted verbatim, so a profile that had tracks keeps every one of them.
TEST(NormalizeRotation, AdoptsALegacySortedSetUnchanged) {
    pt::MusicLibraryState lib{};
    lib.active_pool_track_ids = {0, 3, 4, 6, 9};  // what the per-genre model left behind
    pt::normalize_rotation(lib);
    const std::vector<std::uint8_t> expected{0, 3, 4, 6, 9};
    EXPECT_EQ(lib.active_pool_track_ids, expected);
}

TEST(NormalizeRotation, DropsDuplicatesKeepingTheFirstPosition) {
    pt::MusicLibraryState lib{};
    lib.active_pool_track_ids = {9, 0, 9, 6, 0};
    pt::normalize_rotation(lib);
    const std::vector<std::uint8_t> expected{9, 0, 6};
    EXPECT_EQ(lib.active_pool_track_ids, expected);
}

TEST(NormalizeRotation, DropsIdsOutsideTheCatalog) {
    pt::MusicLibraryState lib{};
    lib.active_pool_track_ids = {0, 12, 255, 3};
    pt::normalize_rotation(lib);
    const std::vector<std::uint8_t> expected{0, 3};
    EXPECT_EQ(lib.active_pool_track_ids, expected);
}

// Boot re-seeds only an EMPTY rotation, so normalization emptying a populated one would
// silently reset the user's playlist to the four starters.
TEST(NormalizeRotation, NeverEmptiesARotationThatHadValidTracks) {
    pt::MusicLibraryState lib{};
    lib.active_pool_track_ids = {7, 7, 7};
    pt::normalize_rotation(lib);
    ASSERT_EQ(lib.active_pool_track_ids.size(), 1u);
    EXPECT_EQ(lib.active_pool_track_ids[0], 7u);
}

// ----- First-session starter seeding (Module 7 "Default tracks") -----

TEST(StarterPoolSeeding, PutsEveryGenresStarterInRotation) {
    pt::MusicLibraryState lib{};
    pt::add_starter_tracks_to_pool(lib);
    EXPECT_TRUE(pt::is_track_in_pool(lib, audio::MusicTrackId::LoungeJazz_Starter));
    EXPECT_TRUE(pt::is_track_in_pool(lib, audio::MusicTrackId::Classical_Starter));
    EXPECT_TRUE(pt::is_track_in_pool(lib, audio::MusicTrackId::BossaNova_Starter));
    EXPECT_TRUE(pt::is_track_in_pool(lib, audio::MusicTrackId::Ambient_Starter));
}

// A fresh profile's Loop order is the genre order the Shop lists, not an arbitrary one.
TEST(StarterPoolSeeding, SeedsInGenreOrder) {
    pt::MusicLibraryState lib{};
    pt::add_starter_tracks_to_pool(lib);
    const std::vector<std::uint8_t> expected{
        static_cast<std::uint8_t>(audio::MusicTrackId::LoungeJazz_Starter),
        static_cast<std::uint8_t>(audio::MusicTrackId::Classical_Starter),
        static_cast<std::uint8_t>(audio::MusicTrackId::BossaNova_Starter),
        static_cast<std::uint8_t>(audio::MusicTrackId::Ambient_Starter)};
    EXPECT_EQ(lib.active_pool_track_ids, expected);
}

TEST(StarterPoolSeeding, LeavesPaidTracksOutOfRotation) {
    pt::MusicLibraryState lib{};
    pt::add_starter_tracks_to_pool(lib);
    ASSERT_EQ(lib.active_pool_track_ids.size(), 4u);
    for (std::size_t i = 0; i < audio::kMusicTrackCount; ++i) {
        const auto track = static_cast<audio::MusicTrackId>(i);
        if (!audio::music_track_info(track).is_starter) {
            EXPECT_FALSE(pt::is_track_in_pool(lib, track));
        }
    }
}

TEST(StarterPoolSeeding, IsIdempotent) {
    pt::MusicLibraryState lib{};
    pt::add_starter_tracks_to_pool(lib);
    pt::add_starter_tracks_to_pool(lib);
    EXPECT_EQ(lib.active_pool_track_ids.size(), 4u);
}

TEST(StarterPoolSeeding, PreservesTracksAlreadyInRotation) {
    pt::AppState s{};
    s.tomatoes = pt::TomatoesState{25, 25};
    ASSERT_TRUE(pt::purchase_track(s, audio::MusicTrackId::Classical_Track2));
    pt::add_track_to_pool(s.music_library, audio::MusicTrackId::Classical_Track2);

    pt::add_starter_tracks_to_pool(s.music_library);
    EXPECT_TRUE(pt::is_track_in_pool(s.music_library, audio::MusicTrackId::Classical_Track2));
    EXPECT_EQ(s.music_library.active_pool_track_ids.size(), 5u);
    // The track already in rotation keeps its position; the starters append behind it.
    EXPECT_EQ(s.music_library.active_pool_track_ids[0],
              static_cast<std::uint8_t>(audio::MusicTrackId::Classical_Track2));
}

// Boot reconciles Z03's in-memory pools to this set both ways, so a removed starter must
// stay out of it — the persisted rotation is what survives a reload, not the Z03 seed.
TEST(StarterPoolSeeding, RemovedStarterStaysOutOnceTheSetIsNonEmpty) {
    pt::MusicLibraryState lib{};
    pt::add_starter_tracks_to_pool(lib);
    pt::remove_track_from_pool(lib, audio::MusicTrackId::LoungeJazz_Starter);
    EXPECT_FALSE(pt::is_track_in_pool(lib, audio::MusicTrackId::LoungeJazz_Starter));
    EXPECT_FALSE(lib.active_pool_track_ids.empty());  // boot re-seeds only an EMPTY set
}
