// Zone 03 music transport gate + MusicEngine pool routing: the pure halt/resume
// decision (which encodes the spec's empty-pool silence, remove-last halt, and
// add-to-empty resume) and the per-genre pool seeding / routing. The crossfade
// ramp itself is backend glue, browser-verified, not exercised here.

#include "audio/music.hpp"

#include "audio/audio_paths.hpp"
#include "audio/shuffle_pool.hpp"

#include <gtest/gtest.h>

namespace {

using poker_trainer::audio::MusicEngine;
using poker_trainer::audio::MusicGate;
using poker_trainer::audio::music_gate;
using poker_trainer::audio::MusicGenre;
using poker_trainer::audio::MusicTrackId;

TEST(MusicGate, ClosedBeforeGesture) {
    // Nothing plays before the autoplay gesture, regardless of pool / sounding.
    EXPECT_EQ(music_gate(/*started=*/false, /*empty=*/false, /*sounding=*/false),
              MusicGate::Silent);
    EXPECT_EQ(music_gate(/*started=*/false, /*empty=*/true, /*sounding=*/true),
              MusicGate::Silent);
}

TEST(MusicGate, EmptyActivePoolIsSilentOrHalts) {
    // Empty pool with nothing sounding -> silence (the explicit state).
    EXPECT_EQ(music_gate(/*started=*/true, /*empty=*/true, /*sounding=*/false),
              MusicGate::Silent);
    // Empty pool while a track is sounding (the last track was just removed) ->
    // halt immediately.
    EXPECT_EQ(music_gate(/*started=*/true, /*empty=*/true, /*sounding=*/true),
              MusicGate::Halt);
}

TEST(MusicGate, NonEmptyPoolStartsThenContinues) {
    // Non-empty pool, nothing sounding (fresh gesture, or a track was added to a
    // previously empty pool) -> start.
    EXPECT_EQ(music_gate(/*started=*/true, /*empty=*/false, /*sounding=*/false),
              MusicGate::Start);
    // Already sounding -> continue (advance / crossfade).
    EXPECT_EQ(music_gate(/*started=*/true, /*empty=*/false, /*sounding=*/true),
              MusicGate::Continue);
}

TEST(MusicEngine, DefaultsToLoungeJazz) {
    MusicEngine engine{1};
    EXPECT_EQ(engine.active_genre(), MusicGenre::LoungeJazz);
}

TEST(MusicEngine, SeedsEachGenreWithItsStarterTrack) {
    MusicEngine engine{1};
    engine.seed_starter_tracks();
    EXPECT_EQ(engine.pool(MusicGenre::LoungeJazz).size(), 1u);
    EXPECT_TRUE(engine.pool(MusicGenre::LoungeJazz).contains(MusicTrackId::LoungeJazz_Starter));
    EXPECT_TRUE(engine.pool(MusicGenre::Classical).contains(MusicTrackId::Classical_Starter));
    EXPECT_TRUE(engine.pool(MusicGenre::BossaNova).contains(MusicTrackId::BossaNova_Starter));
    EXPECT_TRUE(engine.pool(MusicGenre::Ambient).contains(MusicTrackId::Ambient_Starter));
    // Paid tracks are not seeded into the in-memory pool.
    EXPECT_FALSE(engine.pool(MusicGenre::LoungeJazz).contains(MusicTrackId::LoungeJazz_Track2));
}

TEST(MusicEngine, ActivePoolFollowsActiveGenre) {
    MusicEngine engine{1};
    engine.seed_starter_tracks();
    EXPECT_TRUE(engine.active_pool().contains(MusicTrackId::LoungeJazz_Starter));
    engine.set_active_genre(MusicGenre::BossaNova);
    EXPECT_EQ(engine.active_genre(), MusicGenre::BossaNova);
    EXPECT_TRUE(engine.active_pool().contains(MusicTrackId::BossaNova_Starter));
    EXPECT_FALSE(engine.active_pool().contains(MusicTrackId::LoungeJazz_Starter));
}

TEST(MusicEngine, AddAndRemoveRouteToTheNamedGenre) {
    MusicEngine engine{1};
    engine.add(MusicGenre::Classical, MusicTrackId::Classical_Track2);
    EXPECT_TRUE(engine.pool(MusicGenre::Classical).contains(MusicTrackId::Classical_Track2));
    EXPECT_FALSE(engine.pool(MusicGenre::Ambient).contains(MusicTrackId::Classical_Track2));
    engine.remove(MusicGenre::Classical, MusicTrackId::Classical_Track2);
    EXPECT_FALSE(engine.pool(MusicGenre::Classical).contains(MusicTrackId::Classical_Track2));
}

}  // namespace
