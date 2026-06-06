// Zone 03 shuffle-pool semantics: add/remove, the reshuffle-at-cycle-end ordering
// contract, empty-pool silence, remove-to-empty, and that a removed track never
// plays again. These are the deterministic, browser-independent contracts.

#include "audio/shuffle_pool.hpp"

#include "audio/audio_paths.hpp"

#include <gtest/gtest.h>

#include <set>
#include <vector>

namespace {

using poker_trainer::audio::MusicTrackId;
using poker_trainer::audio::ShufflePool;

// A spread of track ids (membership is unordered, so the exact ids are arbitrary).
constexpr MusicTrackId kA = MusicTrackId::LoungeJazz_Starter;
constexpr MusicTrackId kB = MusicTrackId::Classical_Starter;
constexpr MusicTrackId kC = MusicTrackId::BossaNova_Starter;
constexpr MusicTrackId kD = MusicTrackId::Ambient_Starter;
constexpr MusicTrackId kE = MusicTrackId::LoungeJazz_Track2;

TEST(ShufflePool, EmptyPoolReportsSilence) {
    ShufflePool pool{1};
    EXPECT_TRUE(pool.empty());
    EXPECT_EQ(pool.size(), 0u);
    EXPECT_FALSE(pool.next().has_value());  // explicit silence, no fallback track
}

TEST(ShufflePool, AddMakesNonEmptyAndContains) {
    ShufflePool pool{1};
    pool.add(kA);
    EXPECT_FALSE(pool.empty());
    EXPECT_TRUE(pool.contains(kA));
    EXPECT_FALSE(pool.contains(kB));
    pool.add(kA);  // duplicate add is a no-op
    EXPECT_EQ(pool.size(), 1u);
}

TEST(ShufflePool, SingleTrackPoolAlwaysReturnsThatTrack) {
    ShufflePool pool{7};
    pool.add(kA);
    for (int i = 0; i < 5; ++i) {
        const auto track = pool.next();
        ASSERT_TRUE(track.has_value());
        EXPECT_EQ(*track, kA);
    }
}

TEST(ShufflePool, FullCycleIsPermutationOfMembers) {
    ShufflePool pool{42};
    pool.add(kA);
    pool.add(kB);
    pool.add(kC);
    std::multiset<MusicTrackId> seen;
    for (int i = 0; i < 3; ++i) {
        const auto track = pool.next();
        ASSERT_TRUE(track.has_value());
        seen.insert(*track);
    }
    EXPECT_EQ(seen.count(kA), 1u);
    EXPECT_EQ(seen.count(kB), 1u);
    EXPECT_EQ(seen.count(kC), 1u);
}

TEST(ShufflePool, ReshufflesAndContinuesAfterCycleEnd) {
    ShufflePool pool{42};
    pool.add(kA);
    pool.add(kB);
    pool.add(kC);
    std::multiset<MusicTrackId> seen;
    for (int i = 0; i < 6; ++i) {  // two full cycles
        const auto track = pool.next();
        ASSERT_TRUE(track.has_value());
        seen.insert(*track);
    }
    // Each member appears exactly twice — the pool reshuffles and continues rather
    // than looping a single track or stalling.
    EXPECT_EQ(seen.count(kA), 2u);
    EXPECT_EQ(seen.count(kB), 2u);
    EXPECT_EQ(seen.count(kC), 2u);
}

TEST(ShufflePool, NoImmediateRepeatAcrossCycleBoundary) {
    // The last track of one cycle is never the first of the next (when >1 member),
    // for every seed we try.
    for (std::uint64_t seed = 0; seed < 16; ++seed) {
        ShufflePool pool{seed};
        pool.add(kA);
        pool.add(kB);
        pool.add(kC);
        MusicTrackId last{};
        for (int i = 0; i < 3; ++i) {
            const auto track = pool.next();
            ASSERT_TRUE(track.has_value());
            last = *track;
        }
        const auto boundary = pool.next();  // first of the next cycle
        ASSERT_TRUE(boundary.has_value());
        EXPECT_NE(*boundary, last) << "seed=" << seed;
    }
}

TEST(ShufflePool, AddToEmptyPoolResumesPlayback) {
    ShufflePool pool{3};
    EXPECT_FALSE(pool.next().has_value());  // empty -> silence
    pool.add(kB);
    const auto track = pool.next();
    ASSERT_TRUE(track.has_value());
    EXPECT_EQ(*track, kB);  // playback resumes from the newly added track
}

TEST(ShufflePool, RemoveDownToEmptyReportsSilence) {
    ShufflePool pool{5};
    pool.add(kA);
    pool.add(kB);
    pool.remove(kA);
    pool.remove(kB);
    EXPECT_TRUE(pool.empty());
    EXPECT_FALSE(pool.next().has_value());
}

TEST(ShufflePool, RemoveLastTrackEmptiesPool) {
    ShufflePool pool{5};
    pool.add(kA);
    (void)pool.next();   // it is playing
    pool.remove(kA);     // remove the last track
    EXPECT_TRUE(pool.empty());
    EXPECT_FALSE(pool.next().has_value());
}

TEST(ShufflePool, RemovedTrackNeverPlaysAgain) {
    ShufflePool pool{11};
    pool.add(kA);
    pool.add(kB);
    pool.add(kC);
    pool.add(kD);
    pool.add(kE);
    const auto first = pool.next();
    ASSERT_TRUE(first.has_value());
    // Remove a member that is not the one currently playing.
    const MusicTrackId removed = (*first == kC) ? kD : kC;
    pool.remove(removed);
    for (int i = 0; i < 30; ++i) {  // across this cycle and several reshuffles
        const auto track = pool.next();
        ASSERT_TRUE(track.has_value());
        EXPECT_NE(*track, removed);
    }
}

}  // namespace
