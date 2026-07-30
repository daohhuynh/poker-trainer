#include "audio/music.hpp"

#include "audio/audio.hpp"
#include "audio/audio_engine.hpp"
#include "audio/audio_paths.hpp"
#include "audio/backend.hpp"

#include "settings/settings.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <vector>

namespace poker_trainer::audio {

// ---- MusicRotation ----

MusicRotation::MusicRotation(std::uint64_t seed) noexcept : shuffle_(seed) {}

void MusicRotation::seed_starter_tracks() {
    for (std::size_t i = 0; i < kMusicTrackCount; ++i) {
        const MusicTrackInfo& info = kMusicTracks[i];
        if (info.is_starter) {
            add(static_cast<MusicTrackId>(i));
        }
    }
}

bool MusicRotation::contains(MusicTrackId track) const noexcept {
    return std::find(tracks_.begin(), tracks_.end(), track) != tracks_.end();
}

bool MusicRotation::passes_filter(MusicTrackId track) const noexcept {
    return !filter_.has_value() || music_track_info(track).genre == *filter_;
}

bool MusicRotation::in_scope(MusicTrackId track) const noexcept {
    return contains(track) && passes_filter(track);
}

bool MusicRotation::scope_empty() const noexcept {
    return std::none_of(tracks_.begin(), tracks_.end(),
                        [this](MusicTrackId track) { return passes_filter(track); });
}

std::size_t MusicRotation::scope_size() const noexcept {
    return static_cast<std::size_t>(
        std::count_if(tracks_.begin(), tracks_.end(),
                      [this](MusicTrackId track) { return passes_filter(track); }));
}

void MusicRotation::add(MusicTrackId track) {
    if (contains(track)) {
        return;  // already queued: keep its existing position in the rotation
    }
    tracks_.push_back(track);
    // Appending never displaces an existing index, so the Loop cursor needs no
    // adjustment: the walk reaches the new track once it wraps around to the back.
    if (passes_filter(track)) {
        shuffle_.add(track);
    }
}

void MusicRotation::remove(MusicTrackId track) {
    const auto position = std::find(tracks_.begin(), tracks_.end(), track);
    if (position == tracks_.end()) {
        return;
    }
    const std::size_t index =
        static_cast<std::size_t>(std::distance(tracks_.begin(), position));
    tracks_.erase(position);
    shuffle_.remove(track);

    // Keep the Loop cursor pointing at the same *track* it pointed at before the
    // erase, so removing an earlier entry does not silently skip one. Erasing at or
    // after the cursor already leaves the cursor's index correct; the modulo covers
    // the case where the erased entry was the last one and the cursor now sits past
    // the end.
    if (tracks_.empty()) {
        loop_cursor_ = 0;
        return;
    }
    if (index < loop_cursor_) {
        --loop_cursor_;
    }
    loop_cursor_ %= tracks_.size();
}

void MusicRotation::set_genre_filter(std::optional<MusicGenre> genre) {
    filter_ = genre;
    sync_shuffle_scope();
    // The Loop cursor is deliberately left alone: it indexes the rotation, not the
    // scope, so it stays meaningful and the walk simply steps over whatever the new
    // filter excludes.
}

void MusicRotation::sync_shuffle_scope() {
    // Copy the membership first: ShufflePool::remove mutates the vector members()
    // views.
    const std::vector<MusicTrackId> members = shuffle_.members();
    for (const MusicTrackId track : members) {
        if (!in_scope(track)) {
            shuffle_.remove(track);
        }
    }
    for (const MusicTrackId track : tracks_) {
        if (passes_filter(track)) {
            shuffle_.add(track);  // no-op for tracks already in the pool
        }
    }
}

std::optional<MusicTrackId> MusicRotation::next_in_loop() {
    const std::size_t count = tracks_.size();
    for (std::size_t step = 0; step < count; ++step) {
        const std::size_t index = (loop_cursor_ + step) % count;
        if (passes_filter(tracks_[index])) {
            loop_cursor_ = (index + 1) % count;
            return tracks_[index];
        }
    }
    return std::nullopt;  // rotation empty, or the filter selects nothing
}

std::optional<MusicTrackId> MusicRotation::next() {
    if (order_ == settings::MusicPlaybackOrder::Shuffle) {
        return shuffle_.next();
    }
    return next_in_loop();
}

MusicGate music_gate(bool gesture_started, bool scope_empty, bool sounding) noexcept {
    if (!gesture_started) {
        return MusicGate::Silent;
    }
    if (scope_empty) {
        return sounding ? MusicGate::Halt : MusicGate::Silent;
    }
    return sounding ? MusicGate::Continue : MusicGate::Start;
}

namespace {

// Start `track` in the engine's current front slot at the live music gain.
void start_in_front_slot(AudioEngine& eng, MusicTrackId track) {
    const MusicTrackInfo& info = music_track_info(track);
    backend::music_load(eng.transport.front_slot, info.path, eng.music_gain(), /*play=*/true);
    eng.transport.sounding = true;
    eng.transport.crossfading = false;
    eng.transport.crossfade_ms = 0.0f;
    eng.transport.playing = track;
    eng.transport.incoming.reset();
}

// Stop everything and return the transport to its silent state.
void halt_playback(AudioEngine& eng) {
    backend::music_stop_all();
    eng.transport = MusicTransport{};
}

// Abandon an in-flight crossfade and restore the outgoing track to full gain. Used
// when the track fading IN leaves the scope: the outgoing track is still in scope,
// so it keeps playing and the next update picks a different successor.
void cancel_incoming(AudioEngine& eng) {
    MusicTransport& t = eng.transport;
    if (t.crossfading) {
        backend::music_stop(1 - t.front_slot);
        backend::music_set_volume(t.front_slot, eng.music_gain());
        t.crossfading = false;
        t.crossfade_ms = 0.0f;
    }
    t.incoming.reset();
}

// Move off the currently-playing track immediately: onto the next track in scope,
// or into silence when nothing is left in scope.
void advance_or_halt(AudioEngine& eng) {
    const std::optional<MusicTrackId> nxt = eng.music.next();
    if (nxt.has_value()) {
        music_start_track(eng, *nxt);
        return;
    }
    halt_playback(eng);
}

// Drive the front track while it sounds: keep its live volume applied, start a
// crossfade into the next track as the current one nears its end, and finish the
// crossfade by swapping the front slot. A missed crossfade window (very short track
// or unavailable duration) falls back to a hard advance on end.
void continue_playback(AudioEngine& eng, float delta_ms) {
    MusicTransport& t = eng.transport;

    if (!t.crossfading) {
        backend::music_set_volume(t.front_slot, eng.music_gain());
        const float remaining = backend::music_remaining_ms(t.front_slot);
        const bool near_end = remaining >= 0.0f && remaining <= kCrossfadeMs;
        if (near_end) {
            const std::optional<MusicTrackId> nxt = eng.music.next();
            if (nxt.has_value()) {
                const int back = 1 - t.front_slot;
                backend::music_load(back, music_track_info(*nxt).path, 0.0f, /*play=*/true);
                t.crossfading = true;
                t.crossfade_ms = 0.0f;
                t.incoming = nxt;
            }
        } else if (backend::music_ended(t.front_slot)) {
            const std::optional<MusicTrackId> nxt = eng.music.next();
            if (nxt.has_value()) {
                start_in_front_slot(eng, *nxt);
            }
        }
        return;
    }

    t.crossfade_ms += delta_ms;
    const float ramp = std::clamp(t.crossfade_ms / kCrossfadeMs, 0.0f, 1.0f);
    const float gain = eng.music_gain();
    const int back = 1 - t.front_slot;
    backend::music_set_volume(t.front_slot, gain * (1.0f - ramp));
    backend::music_set_volume(back, gain * ramp);
    if (ramp >= 1.0f) {
        backend::music_stop(t.front_slot);
        t.front_slot = back;
        t.playing = t.incoming;
        t.incoming.reset();
        t.crossfading = false;
        t.crossfade_ms = 0.0f;
    }
}

}  // namespace

void music_update(AudioEngine& eng, float delta_ms) {
    switch (music_gate(eng.gesture_started, eng.music.scope_empty(),
                       eng.transport.sounding)) {
        case MusicGate::Silent:
            return;
        case MusicGate::Halt:
            halt_playback(eng);
            return;
        case MusicGate::Start: {
            const std::optional<MusicTrackId> nxt = eng.music.next();
            if (nxt.has_value()) {
                start_in_front_slot(eng, *nxt);
            }
            return;
        }
        case MusicGate::Continue:
            continue_playback(eng, delta_ms);
            return;
    }
}

void music_start_track(AudioEngine& eng, MusicTrackId track) {
    // Direct play of a specific track (play_music): interrupt whatever is sounding.
    if (eng.transport.crossfading) {
        backend::music_stop(1 - eng.transport.front_slot);
    }
    start_in_front_slot(eng, track);
}

void music_remove_track(AudioEngine& eng, MusicTrackId track) {
    const MusicTransport& t = eng.transport;
    const bool was_playing = t.sounding && t.playing == track;
    const bool was_incoming = t.sounding && t.incoming == track;

    eng.music.remove(track);

    if (was_incoming) {
        cancel_incoming(eng);
    }
    if (was_playing) {
        // Removing the track that is sounding is the user saying "not this one,
        // now": skip immediately rather than letting a track that is no longer in
        // the rotation play out its remaining minutes.
        advance_or_halt(eng);
    }
}

void music_apply_genre_filter(AudioEngine& eng, std::optional<MusicGenre> genre) {
    if (eng.music.genre_filter() == genre) {
        return;
    }
    eng.music.set_genre_filter(genre);
    if (!eng.transport.sounding) {
        return;  // nothing playing; the next update starts the newly scoped rotation
    }
    if (eng.transport.incoming.has_value() && !eng.music.in_scope(*eng.transport.incoming)) {
        cancel_incoming(eng);
    }
    if (eng.transport.playing.has_value() && eng.music.in_scope(*eng.transport.playing)) {
        return;  // still in scope: a filter change never restarts a track
    }
    advance_or_halt(eng);
}

// ---- Public music exports ----

void play_music(MusicTrackId track) {
    music_start_track(audio_engine(), track);
}

void add_to_rotation(MusicTrackId track) {
    // Playback resumes on the next update via the gate's Start case, so an add to
    // an empty (or fully filtered-out) rotation needs no transport work here.
    audio_engine().music.add(track);
}

void remove_from_rotation(MusicTrackId track) {
    music_remove_track(audio_engine(), track);
}

bool in_rotation(MusicTrackId track) {
    return audio_engine().music.contains(track);
}

std::vector<MusicTrackId> rotation_tracks() {
    return audio_engine().music.tracks();
}

void set_genre_filter(std::optional<MusicGenre> genre) {
    music_apply_genre_filter(audio_engine(), genre);
}

void set_playback_order(settings::MusicPlaybackOrder order) {
    // Pure mode switch: whatever is sounding keeps sounding, only the choice of the
    // NEXT track changes.
    audio_engine().music.set_playback_order(order);
}

}  // namespace poker_trainer::audio
