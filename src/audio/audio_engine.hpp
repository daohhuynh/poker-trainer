#pragma once

#include "audio/audio_paths.hpp"
#include "audio/choreography.hpp"
#include "audio/music.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace poker_trainer::audio {

// Default global output volume (ARCHITECTURE Audio Settings: 50%).
inline constexpr int kDefaultVolume = 50;

// Backend music crossfade slot state: which <audio> slot is the front (current)
// track, the crossfade-in-progress flag and its elapsed time, and the track ids in
// each role. This is transport glue (browser-verified), not unit-tested directly.
struct MusicTransport {
    bool sounding{false};
    bool crossfading{false};
    int front_slot{0};
    float crossfade_ms{0.0f};
    std::optional<MusicTrackId> playing;
    std::optional<MusicTrackId> incoming;  // the track fading in during a crossfade
};

// The single audio engine: global output volume + the three independent mutes +
// the autoplay gesture gate + the music engine, its transport, and the spawn
// choreography scheduler. One process-global instance backs the free-function API
// (see audio_engine()); the type is also constructed directly by unit tests to
// exercise the pure volume / mute / gate math without the backend.
struct AudioEngine {
    int master_volume{kDefaultVolume};
    bool mute_all{false};
    bool mute_music{false};
    bool mute_sfx{false};
    bool gesture_started{false};
    std::size_t prev_modal_depth{0};

    MusicEngine music;
    MusicTransport transport;
    ChoreographyScheduler choreo;

    explicit AudioEngine(std::uint64_t seed) noexcept : music(seed) {}

    void set_volume(int volume_0_100) noexcept;

    [[nodiscard]] bool gate_open() const noexcept { return gesture_started; }

    // Absolute output gain in [0, 1] for an SFX call at `call_gain`, folding in the
    // gesture gate, mute_all, mute_sfx and the master volume. 0 means "do not play".
    [[nodiscard]] float sfx_gain(float call_gain) const noexcept;

    // Absolute output gain in [0, 1] for music, folding in the gesture gate,
    // mute_all, mute_music and the master volume.
    [[nodiscard]] float music_gain() const noexcept;
};

// The process-global engine instance. Justified exactly as Z06's theme service is:
// the ZONES.md Z03 exports are parameter-less free functions, so a single engine
// must be globally reachable; the app is single-threaded (the browser main thread),
// so no synchronization is needed. All free-function exports funnel through this.
// Named audio_engine() (not engine()) to avoid shadowing the poker_trainer::engine
// namespace inside this zone.
[[nodiscard]] AudioEngine& audio_engine();

// Internal transport drivers (implemented in music.cpp), called by audio_update and
// play_music. Not part of the public API.
void music_update(AudioEngine& eng, float delta_ms);
void music_start_track(AudioEngine& eng, MusicTrackId track);

}  // namespace poker_trainer::audio
