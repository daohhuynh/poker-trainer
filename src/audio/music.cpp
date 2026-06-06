#include "audio/music.hpp"

#include "audio/audio.hpp"
#include "audio/audio_engine.hpp"
#include "audio/audio_paths.hpp"
#include "audio/backend.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>

namespace poker_trainer::audio {

MusicEngine::MusicEngine(std::uint64_t seed) noexcept
    : pools_{ShufflePool{seed},
             ShufflePool{seed ^ 0x9E3779B97F4A7C15ULL},
             ShufflePool{seed + 0x1000ULL},
             ShufflePool{seed ^ 0xD1B54A32D192ED03ULL}} {}

void MusicEngine::seed_starter_tracks() {
    for (std::size_t i = 0; i < kMusicTrackCount; ++i) {
        const MusicTrackInfo& info = kMusicTracks[i];
        if (info.is_starter) {
            pool(info.genre).add(static_cast<MusicTrackId>(i));
        }
    }
}

ShufflePool& MusicEngine::pool(MusicGenre genre) noexcept {
    return pools_[static_cast<std::size_t>(genre)];
}

MusicGate music_gate(bool gesture_started, bool pool_empty, bool sounding) noexcept {
    if (!gesture_started) {
        return MusicGate::Silent;
    }
    if (pool_empty) {
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
            const std::optional<MusicTrackId> nxt = eng.music.active_pool().next();
            if (nxt.has_value()) {
                const int back = 1 - t.front_slot;
                backend::music_load(back, music_track_info(*nxt).path, 0.0f, /*play=*/true);
                t.crossfading = true;
                t.crossfade_ms = 0.0f;
                t.incoming = nxt;
            }
        } else if (backend::music_ended(t.front_slot)) {
            const std::optional<MusicTrackId> nxt = eng.music.active_pool().next();
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
    ShufflePool& active = eng.music.active_pool();
    switch (music_gate(eng.gesture_started, active.empty(), eng.transport.sounding)) {
        case MusicGate::Silent:
            return;
        case MusicGate::Halt:
            backend::music_stop_all();
            eng.transport = MusicTransport{};
            return;
        case MusicGate::Start: {
            const std::optional<MusicTrackId> nxt = active.next();
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

// ---- Public music exports ----

void play_music(MusicTrackId track) {
    music_start_track(audio_engine(), track);
}

void set_active_genre(MusicGenre genre) {
    AudioEngine& eng = audio_engine();
    if (eng.music.active_genre() == genre) {
        return;
    }
    eng.music.set_active_genre(genre);
    // Switch pools: stop the current track and reset the transport; the next
    // audio_update starts the new active pool (no cross-genre crossfade is spec'd).
    backend::music_stop_all();
    eng.transport = MusicTransport{};
}

void add_to_shuffle(MusicGenre genre, MusicTrackId track) {
    audio_engine().music.add(genre, track);
}

void remove_from_shuffle(MusicGenre genre, MusicTrackId track) {
    audio_engine().music.remove(genre, track);
}

}  // namespace poker_trainer::audio
