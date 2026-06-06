#include "audio/audio.hpp"

#include "audio/audio_engine.hpp"
#include "audio/audio_paths.hpp"
#include "audio/backend.hpp"

#include "bridge/game_launch.hpp"

#include "backbone/animation_clock.hpp"
#include "backbone/modal_state.hpp"
#include "backbone/scenario_events.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>

// Zone 03 coordinator: the process-global AudioEngine, the pure volume / mute /
// gate math, the boot install (scenario_spawned subscription), the per-frame update
// (choreography SFX, modal swoosh, music transport), and the autoplay gesture gate.

namespace poker_trainer::audio {

namespace {

// Seed the per-session shuffle order from a non-deterministic source so each
// session's music order differs (only saved / shared *scenarios* are deterministic;
// music order intentionally is not). Mirrors bridge::game_launch's master_rng.
[[nodiscard]] std::uint64_t make_session_seed() {
    std::random_device rd;
    const std::uint64_t hi = static_cast<std::uint64_t>(rd());
    const std::uint64_t lo = static_cast<std::uint64_t>(rd());
    return (hi << 32) ^ lo;
}

}  // namespace

AudioEngine& audio_engine() {
    static AudioEngine instance = [] {
        AudioEngine eng{make_session_seed()};
        eng.music.seed_starter_tracks();  // each genre starts with its free track
        return eng;
    }();
    return instance;
}

void AudioEngine::set_volume(int volume_0_100) noexcept {
    master_volume = std::clamp(volume_0_100, 0, 100);
}

float AudioEngine::sfx_gain(float call_gain) const noexcept {
    if (!gesture_started || mute_all || mute_sfx) {
        return 0.0f;
    }
    return (static_cast<float>(master_volume) / 100.0f) * call_gain;
}

float AudioEngine::music_gain() const noexcept {
    if (!gesture_started || mute_all || mute_music) {
        return 0.0f;
    }
    return static_cast<float>(master_volume) / 100.0f;
}

void install_audio() {
    // Spawn choreography: the bus event carries only the id, so read the single
    // authoritative scenario from the bridge and schedule the deal + chip / side-pot
    // cues against the animation clock.
    (void)backbone::subscribe_scenario_spawned(
        [](const backbone::ScenarioSpawnedEvent&) {
            const auto* scenario = bridge::active_scenario();
            if (scenario == nullptr) {
                return;
            }
            audio_engine().choreo.schedule(scenario->type, scenario->side_pot,
                                           backbone::total_ms_since_app_start());
        },
        "audio.choreography");
    // Modal open / close swoosh is observed via the modal-stack-depth edge in
    // audio_update (modal_state.hpp is query-only — no subscribe API).
}

void on_first_user_gesture() {
    AudioEngine& eng = audio_engine();
    if (eng.gesture_started) {
        return;  // one-shot
    }
    eng.gesture_started = true;
    // Initialize the backends inside the gesture callstack so the browser starts the
    // Web Audio device "running" and the first <audio>.play() is permitted.
    backend::sfx_init();
    backend::music_init();
    // Begin the active genre's shuffle pool now (in-gesture) so music persists across
    // the session; an empty active pool stays silent (the explicit state).
    ShufflePool& active = eng.music.active_pool();
    if (!active.empty()) {
        const std::optional<MusicTrackId> first = active.next();
        if (first.has_value()) {
            music_start_track(eng, *first);
        }
    }
}

void audio_update() {
    AudioEngine& eng = audio_engine();
    const std::uint64_t now = backbone::total_ms_since_app_start();

    // Fire any choreography SFX whose time has arrived (deal at T=0, chip / side-pot
    // cue at T=400), each at its scheduled relative gain.
    for (const ScheduledSfx& entry : eng.choreo.poll(now)) {
        play_sfx(entry.id, entry.gain);
    }

    // Modal open / close swoosh: edge-detect the modal stack depth. modal_state.hpp
    // exposes no subscribe API, so this is the non-invasive observer. Stays silent
    // until Zone 11 drives the modal stack (the W2 stub keeps depth at 0).
    const std::size_t depth = backbone::modal_stack_depth();
    if (depth > eng.prev_modal_depth) {
        play_sfx(SfxId::ModalSwooshOpen);
    } else if (depth < eng.prev_modal_depth) {
        play_sfx(SfxId::ModalSwooshClose);
    }
    eng.prev_modal_depth = depth;

    // Advance the music shuffle / crossfade.
    music_update(eng, backbone::delta_ms_since_last_frame());

    // Reap finished one-shot SFX voices.
    backend::sfx_update();
}

void set_volume(int volume_0_100) {
    audio_engine().set_volume(volume_0_100);
}

void set_mute_all(bool muted) {
    audio_engine().mute_all = muted;
}

void set_mute_music(bool muted) {
    audio_engine().mute_music = muted;
}

void set_mute_sfx(bool muted) {
    audio_engine().mute_sfx = muted;
}

}  // namespace poker_trainer::audio
