// Zone 03 music rotation + transport gate. The rotation is ONE ordered,
// cross-genre playlist; the genre selection is a filter over it, never an editor;
// and the playback order decides how the filtered scope is walked. All of that is
// pure logic and is exercised here, together with the transport reactions that must
// happen the instant the rotation or the filter changes (skip the playing track,
// halt when the scope empties, never restart a track that is still in scope).
//
// The crossfade ramp itself is backend glue, browser-verified, not exercised here:
// the native backend is a no-op, so a started track simply keeps sounding.

#include "audio/music.hpp"

#include "audio/audio_engine.hpp"
#include "audio/audio_paths.hpp"

#include "settings/settings.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace {

using poker_trainer::audio::AudioEngine;
using poker_trainer::audio::MusicGate;
using poker_trainer::audio::music_apply_genre_filter;
using poker_trainer::audio::music_gate;
using poker_trainer::audio::MusicGenre;
using poker_trainer::audio::music_remove_track;
using poker_trainer::audio::MusicRotation;
using poker_trainer::audio::MusicTrackId;
using poker_trainer::audio::music_update;
using poker_trainer::settings::MusicPlaybackOrder;

// Two tracks per genre, so filter scoping has something to include and exclude.
constexpr MusicTrackId kJazzA = MusicTrackId::LoungeJazz_Starter;
constexpr MusicTrackId kJazzB = MusicTrackId::LoungeJazz_Track2;
constexpr MusicTrackId kClassicalA = MusicTrackId::Classical_Starter;
constexpr MusicTrackId kClassicalB = MusicTrackId::Classical_Track2;
constexpr MusicTrackId kBossaA = MusicTrackId::BossaNova_Starter;
constexpr MusicTrackId kAmbientA = MusicTrackId::Ambient_Starter;

// Draw `count` tracks in the current order, failing the test if the scope runs dry.
std::vector<MusicTrackId> draw(MusicRotation& rotation, int count) {
    std::vector<MusicTrackId> drawn;
    for (int i = 0; i < count; ++i) {
        const std::optional<MusicTrackId> track = rotation.next();
        EXPECT_TRUE(track.has_value()) << "scope ran dry at draw " << i;
        if (!track.has_value()) {
            break;
        }
        drawn.push_back(*track);
    }
    return drawn;
}

// An engine past the autoplay gate with `tracks` in the rotation, playing the first
// one — the state every transport test starts from.
AudioEngine started_engine(const std::vector<MusicTrackId>& tracks) {
    AudioEngine eng{1};
    for (const MusicTrackId track : tracks) {
        eng.music.add(track);
    }
    eng.gesture_started = true;
    music_update(eng, 0.0f);  // gate: Start
    return eng;
}

// ---- Rotation membership: order is the product, so it is asserted everywhere ----

TEST(MusicRotation, EmptyRotationIsSilent) {
    MusicRotation rotation{1};
    EXPECT_TRUE(rotation.empty());
    EXPECT_TRUE(rotation.scope_empty());
    EXPECT_FALSE(rotation.next().has_value());  // explicit silence, no fallback track
}

TEST(MusicRotation, AddPreservesAddOrderAcrossGenres) {
    MusicRotation rotation{1};
    rotation.add(kBossaA);
    rotation.add(kJazzA);
    rotation.add(kClassicalA);
    // Add order, NOT track-id order — the rotation is a queue the user built.
    const std::vector<MusicTrackId> expected{kBossaA, kJazzA, kClassicalA};
    EXPECT_EQ(rotation.tracks(), expected);
}

TEST(MusicRotation, DuplicateAddIsANoOpAndDoesNotReorder) {
    MusicRotation rotation{1};
    rotation.add(kJazzA);
    rotation.add(kClassicalA);
    rotation.add(kJazzA);  // already queued
    const std::vector<MusicTrackId> expected{kJazzA, kClassicalA};
    EXPECT_EQ(rotation.tracks(), expected);
    EXPECT_EQ(rotation.size(), 2u);
}

TEST(MusicRotation, RemovePreservesTheOrderOfTheRest) {
    MusicRotation rotation{1};
    rotation.add(kJazzA);
    rotation.add(kClassicalA);
    rotation.add(kBossaA);
    rotation.add(kAmbientA);
    rotation.remove(kClassicalA);
    const std::vector<MusicTrackId> expected{kJazzA, kBossaA, kAmbientA};
    EXPECT_EQ(rotation.tracks(), expected);
    EXPECT_FALSE(rotation.contains(kClassicalA));
}

TEST(MusicRotation, RemovingATrackThatIsNotInRotationIsANoOp) {
    MusicRotation rotation{1};
    rotation.add(kJazzA);
    rotation.remove(kBossaA);
    const std::vector<MusicTrackId> expected{kJazzA};
    EXPECT_EQ(rotation.tracks(), expected);
}

TEST(MusicRotation, FreshProfileStartsWithAllFourStarterTracks) {
    MusicRotation rotation{1};
    rotation.seed_starter_tracks();
    const std::vector<MusicTrackId> expected{
        MusicTrackId::LoungeJazz_Starter, MusicTrackId::Classical_Starter,
        MusicTrackId::BossaNova_Starter, MusicTrackId::Ambient_Starter};
    EXPECT_EQ(rotation.tracks(), expected);
    EXPECT_FALSE(rotation.contains(MusicTrackId::LoungeJazz_Track2));  // paid
}

// ---- The genre filter scopes playback; it never edits the rotation ----

TEST(MusicRotation, DefaultFilterIsAllGenres) {
    MusicRotation rotation{1};
    EXPECT_FALSE(rotation.genre_filter().has_value());  // nullopt == All genres
}

TEST(MusicRotation, FilterScopesPlaybackWithoutEditingTheRotation) {
    MusicRotation rotation{1};
    rotation.add(kJazzA);
    rotation.add(kClassicalA);
    rotation.add(kJazzB);
    const std::vector<MusicTrackId> before = rotation.tracks();

    rotation.set_genre_filter(MusicGenre::LoungeJazz);

    EXPECT_EQ(rotation.tracks(), before);  // membership and order untouched
    EXPECT_EQ(rotation.size(), 3u);
    EXPECT_EQ(rotation.scope_size(), 2u);
    EXPECT_TRUE(rotation.in_scope(kJazzA));
    EXPECT_FALSE(rotation.in_scope(kClassicalA));
    EXPECT_TRUE(rotation.contains(kClassicalA));  // still IN the rotation
}

TEST(MusicRotation, FilterSelectingNoTrackIsSilentEvenThoughRotationIsNotEmpty) {
    MusicRotation rotation{1};
    rotation.add(kJazzA);
    rotation.add(kJazzB);
    rotation.set_genre_filter(MusicGenre::Classical);
    EXPECT_FALSE(rotation.empty());
    EXPECT_TRUE(rotation.scope_empty());
    EXPECT_EQ(rotation.scope_size(), 0u);
    EXPECT_FALSE(rotation.next().has_value());
}

TEST(MusicRotation, AllGenresFilterPlaysTheWholeRotation) {
    MusicRotation rotation{1};
    rotation.add(kJazzA);
    rotation.add(kClassicalA);
    rotation.set_genre_filter(MusicGenre::Classical);
    EXPECT_EQ(rotation.scope_size(), 1u);
    rotation.set_genre_filter(std::nullopt);
    EXPECT_EQ(rotation.scope_size(), 2u);
    EXPECT_TRUE(rotation.in_scope(kJazzA));
    EXPECT_TRUE(rotation.in_scope(kClassicalA));
}

// ---- Loop order: walk the filtered scope in add order and wrap ----

TEST(MusicRotation, PlaybackOrderDefaultsToLoop) {
    MusicRotation rotation{1};
    EXPECT_EQ(rotation.playback_order(), MusicPlaybackOrder::Loop);
}

TEST(MusicRotation, LoopWalksAddOrderAndWrapsAround) {
    MusicRotation rotation{1};
    rotation.add(kBossaA);
    rotation.add(kJazzA);
    rotation.add(kClassicalA);
    const std::vector<MusicTrackId> expected{kBossaA, kJazzA, kClassicalA, kBossaA,
                                             kJazzA,  kClassicalA, kBossaA};
    EXPECT_EQ(draw(rotation, 7), expected);
}

TEST(MusicRotation, LoopWalksOnlyTheFilteredSubsequence) {
    MusicRotation rotation{1};
    rotation.add(kJazzA);
    rotation.add(kClassicalA);
    rotation.add(kJazzB);
    rotation.add(kAmbientA);
    rotation.set_genre_filter(MusicGenre::LoungeJazz);
    const std::vector<MusicTrackId> expected{kJazzA, kJazzB, kJazzA, kJazzB, kJazzA};
    EXPECT_EQ(draw(rotation, 5), expected);
}

TEST(MusicRotation, LoopContinuesWithTheSuccessorAfterAnEarlierEntryIsRemoved) {
    // Removing an entry the walk has already passed must not make the walk skip the
    // entry it was about to play next.
    MusicRotation rotation{1};
    rotation.add(kJazzA);
    rotation.add(kClassicalA);
    rotation.add(kBossaA);
    EXPECT_EQ(rotation.next(), kJazzA);  // the cursor now sits on kClassicalA
    rotation.remove(kJazzA);
    EXPECT_EQ(rotation.next(), kClassicalA);
    EXPECT_EQ(rotation.next(), kBossaA);
    EXPECT_EQ(rotation.next(), kClassicalA);  // wraps within what is left
}

TEST(MusicRotation, LoopWrapsToTheFrontWhenTheLastEntryIsRemoved) {
    MusicRotation rotation{1};
    rotation.add(kJazzA);
    rotation.add(kClassicalA);
    rotation.add(kBossaA);
    (void)draw(rotation, 3);  // the cursor has wrapped back to the front
    rotation.remove(kBossaA);
    EXPECT_EQ(rotation.next(), kJazzA);
}

TEST(MusicRotation, ATrackAddedMidWalkPlaysWhenTheWalkReachesTheBack) {
    MusicRotation rotation{1};
    rotation.add(kJazzA);
    rotation.add(kClassicalA);
    EXPECT_EQ(rotation.next(), kJazzA);
    rotation.add(kBossaA);  // appended behind kClassicalA
    EXPECT_EQ(rotation.next(), kClassicalA);
    EXPECT_EQ(rotation.next(), kBossaA);
    EXPECT_EQ(rotation.next(), kJazzA);
}

// ---- Shuffle order: draw from the filtered scope, never repeating immediately ----

TEST(MusicRotation, ShuffleNeverRepeatsATrackImmediately) {
    for (std::uint64_t seed = 0; seed < 16; ++seed) {
        MusicRotation rotation{seed};
        rotation.add(kJazzA);
        rotation.add(kClassicalA);
        rotation.add(kBossaA);
        rotation.set_playback_order(MusicPlaybackOrder::Shuffle);
        const std::vector<MusicTrackId> drawn = draw(rotation, 12);
        ASSERT_EQ(drawn.size(), 12u);
        for (std::size_t i = 1; i < drawn.size(); ++i) {
            EXPECT_NE(drawn[i], drawn[i - 1]) << "seed=" << seed << " at draw " << i;
        }
    }
}

TEST(MusicRotation, ShuffleStaysInsideTheFilteredScope) {
    MusicRotation rotation{7};
    rotation.add(kJazzA);
    rotation.add(kClassicalA);
    rotation.add(kJazzB);
    rotation.add(kAmbientA);
    rotation.set_genre_filter(MusicGenre::LoungeJazz);
    rotation.set_playback_order(MusicPlaybackOrder::Shuffle);
    for (const MusicTrackId track : draw(rotation, 20)) {
        EXPECT_TRUE(track == kJazzA || track == kJazzB);
    }
}

TEST(MusicRotation, ShuffleFollowsTheFilterBackToAWiderScope) {
    MusicRotation rotation{7};
    rotation.add(kJazzA);
    rotation.add(kClassicalA);
    rotation.set_genre_filter(MusicGenre::LoungeJazz);
    rotation.set_playback_order(MusicPlaybackOrder::Shuffle);
    EXPECT_EQ(rotation.next(), kJazzA);  // only one track in scope
    rotation.set_genre_filter(std::nullopt);
    bool saw_classical = false;
    for (const MusicTrackId track : draw(rotation, 8)) {
        saw_classical = saw_classical || track == kClassicalA;
    }
    EXPECT_TRUE(saw_classical);
}

TEST(MusicRotation, ShuffleOnAnEmptyScopeIsSilent) {
    MusicRotation rotation{7};
    rotation.add(kJazzA);
    rotation.set_playback_order(MusicPlaybackOrder::Shuffle);
    rotation.set_genre_filter(MusicGenre::Classical);
    EXPECT_FALSE(rotation.next().has_value());
}

TEST(MusicRotation, ChangingPlaybackOrderKeepsMembershipAndOrder) {
    MusicRotation rotation{1};
    rotation.add(kJazzA);
    rotation.add(kClassicalA);
    rotation.add(kBossaA);
    const std::vector<MusicTrackId> before = rotation.tracks();
    rotation.set_playback_order(MusicPlaybackOrder::Shuffle);
    EXPECT_EQ(rotation.tracks(), before);
    rotation.set_playback_order(MusicPlaybackOrder::Loop);
    EXPECT_EQ(rotation.tracks(), before);
}

// ---- The pure transport gate ----

TEST(MusicGate, ClosedBeforeGesture) {
    // Nothing plays before the autoplay gesture, regardless of scope / sounding.
    EXPECT_EQ(music_gate(/*started=*/false, /*scope_empty=*/false, /*sounding=*/false),
              MusicGate::Silent);
    EXPECT_EQ(music_gate(/*started=*/false, /*scope_empty=*/true, /*sounding=*/true),
              MusicGate::Silent);
}

TEST(MusicGate, EmptyScopeIsSilentOrHalts) {
    // Empty scope with nothing sounding -> silence (the explicit state).
    EXPECT_EQ(music_gate(/*started=*/true, /*scope_empty=*/true, /*sounding=*/false),
              MusicGate::Silent);
    // Empty scope while a track is sounding (the last in-scope track was just
    // removed, or the filter moved off it) -> halt immediately.
    EXPECT_EQ(music_gate(/*started=*/true, /*scope_empty=*/true, /*sounding=*/true),
              MusicGate::Halt);
}

TEST(MusicGate, NonEmptyScopeStartsThenContinues) {
    // Non-empty scope, nothing sounding (fresh gesture, or a track was added to an
    // empty rotation) -> start.
    EXPECT_EQ(music_gate(/*started=*/true, /*scope_empty=*/false, /*sounding=*/false),
              MusicGate::Start);
    // Already sounding -> continue (advance / crossfade).
    EXPECT_EQ(music_gate(/*started=*/true, /*scope_empty=*/false, /*sounding=*/true),
              MusicGate::Continue);
}

// ---- Transport reactions to rotation and filter edits ----

TEST(MusicTransport, UpdateStartsTheFirstTrackInAddOrder) {
    const AudioEngine eng = started_engine({kBossaA, kJazzA});
    EXPECT_TRUE(eng.transport.sounding);
    EXPECT_EQ(eng.transport.playing, kBossaA);
}

TEST(MusicTransport, RemovingThePlayingTrackSkipsToTheNextImmediately) {
    // The bug this feature fixes: removing the sounding track used to leave it
    // playing for up to four more minutes.
    AudioEngine eng = started_engine({kJazzA, kClassicalA, kBossaA});
    ASSERT_EQ(eng.transport.playing, kJazzA);
    music_remove_track(eng, kJazzA);
    EXPECT_TRUE(eng.transport.sounding);
    EXPECT_EQ(eng.transport.playing, kClassicalA);  // the successor in add order
    EXPECT_FALSE(eng.music.contains(kJazzA));
}

TEST(MusicTransport, RemovingTheLastInScopeTrackHalts) {
    AudioEngine eng = started_engine({kJazzA});
    ASSERT_TRUE(eng.transport.sounding);
    music_remove_track(eng, kJazzA);
    EXPECT_FALSE(eng.transport.sounding);
    EXPECT_FALSE(eng.transport.playing.has_value());
    // And it stays silent: the gate reports Silent, not Start.
    music_update(eng, 16.0f);
    EXPECT_FALSE(eng.transport.sounding);
}

TEST(MusicTransport, RemovingTheLastTrackOfTheFilteredGenreHalts) {
    // The rotation is still non-empty; the FILTERED scope is what emptied.
    AudioEngine eng{1};
    eng.music.add(kJazzA);
    eng.music.add(kClassicalA);
    music_apply_genre_filter(eng, MusicGenre::LoungeJazz);
    eng.gesture_started = true;
    music_update(eng, 0.0f);
    ASSERT_EQ(eng.transport.playing, kJazzA);

    music_remove_track(eng, kJazzA);
    EXPECT_FALSE(eng.transport.sounding);
    EXPECT_FALSE(eng.music.empty());  // kClassicalA is still in the rotation
}

TEST(MusicTransport, RemovingANonPlayingTrackDoesNotInterrupt) {
    AudioEngine eng = started_engine({kJazzA, kClassicalA, kBossaA});
    ASSERT_EQ(eng.transport.playing, kJazzA);
    music_remove_track(eng, kBossaA);
    EXPECT_TRUE(eng.transport.sounding);
    EXPECT_EQ(eng.transport.playing, kJazzA);  // untouched
}

TEST(MusicTransport, WideningTheFilterKeepsAnInScopeTrackPlaying) {
    // Lounge Jazz -> All genres while a Lounge Jazz track plays: it keeps playing.
    AudioEngine eng{1};
    eng.music.add(kJazzA);
    eng.music.add(kClassicalA);
    music_apply_genre_filter(eng, MusicGenre::LoungeJazz);
    eng.gesture_started = true;
    music_update(eng, 0.0f);
    ASSERT_EQ(eng.transport.playing, kJazzA);

    music_apply_genre_filter(eng, std::nullopt);
    EXPECT_TRUE(eng.transport.sounding);
    EXPECT_EQ(eng.transport.playing, kJazzA);  // not restarted, not skipped
}

TEST(MusicTransport, ChangingTheFilterMovesOffAnOutOfScopeTrack) {
    // Lounge Jazz -> Classical while a Lounge Jazz track plays: move off it now.
    AudioEngine eng = started_engine({kJazzA, kClassicalA});
    ASSERT_EQ(eng.transport.playing, kJazzA);
    music_apply_genre_filter(eng, MusicGenre::Classical);
    EXPECT_TRUE(eng.transport.sounding);
    EXPECT_EQ(eng.transport.playing, kClassicalA);
}

TEST(MusicTransport, FilteringToAGenreWithNoTracksInRotationHalts) {
    AudioEngine eng = started_engine({kJazzA, kClassicalA});
    ASSERT_TRUE(eng.transport.sounding);
    music_apply_genre_filter(eng, MusicGenre::Ambient);  // nothing Ambient in rotation
    EXPECT_FALSE(eng.transport.sounding);
    EXPECT_FALSE(eng.transport.playing.has_value());
}

TEST(MusicTransport, ReSelectingTheSameFilterDoesNothing) {
    AudioEngine eng = started_engine({kJazzA, kClassicalA});
    ASSERT_EQ(eng.transport.playing, kJazzA);
    music_apply_genre_filter(eng, std::nullopt);  // already All genres
    EXPECT_EQ(eng.transport.playing, kJazzA);
}

TEST(MusicTransport, ChangingPlaybackOrderDoesNotInterruptTheCurrentTrack) {
    AudioEngine eng = started_engine({kJazzA, kClassicalA, kBossaA});
    ASSERT_EQ(eng.transport.playing, kJazzA);
    eng.music.set_playback_order(MusicPlaybackOrder::Shuffle);
    music_update(eng, 16.0f);
    EXPECT_TRUE(eng.transport.sounding);
    EXPECT_EQ(eng.transport.playing, kJazzA);
}

TEST(MusicTransport, AddingToASilentRotationResumesPlayback) {
    AudioEngine eng{1};
    eng.gesture_started = true;
    music_update(eng, 0.0f);  // empty rotation -> silence
    ASSERT_FALSE(eng.transport.sounding);

    eng.music.add(kClassicalB);
    music_update(eng, 0.0f);
    EXPECT_TRUE(eng.transport.sounding);
    EXPECT_EQ(eng.transport.playing, kClassicalB);
}

}  // namespace
