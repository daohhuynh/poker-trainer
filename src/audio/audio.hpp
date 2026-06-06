#pragma once

#include "audio/audio_paths.hpp"

// Zone 03 — Audio Engine (Module 2 audio), public umbrella header.
//
// This is the only audio header other zones include. It exposes the ZONES.md Z03
// control surface (play_music / play_sfx / set_volume / add_to_shuffle /
// remove_from_shuffle / on_first_user_gesture) plus the Module 2 + Audio Settings
// additions (independent mutes and active-genre selection), and the three lifecycle
// hooks the Z05 bridge calls at its established integration points.
//
// Sound output is gated behind the autoplay policy: nothing plays until
// on_first_user_gesture() fires (first click/key on Root). Until then every call
// here is a silent no-op. Music and SFX assets load by path and degrade gracefully
// when absent (a missing file simply plays no sound).

namespace poker_trainer::audio {

// ---- Lifecycle (called by the Z05 bridge) ----

// Install Z03 at boot: subscribe to scenario_spawned (spawn choreography) and set
// up per-frame state. Call once, after the backbone is initialized.
void install_audio();

// Advance the audio engine one frame: fire due choreography SFX, emit the modal
// swoosh on modal-stack edges, and progress the music shuffle / crossfade. Driven
// by the animation clock; call once per frame from the main loop.
void audio_update();

// The autoplay gate. Call from inside the first user-gesture callstack (first
// click / key on Root). Starts the audio backend and begins music for the session.
// Idempotent: only the first call has effect.
void on_first_user_gesture();

// ---- Control surface (ZONES.md Z03 exports + Module 2 / Audio Settings) ----

// Play a one-shot sound effect at `gain` in [0, 1] relative to the global volume
// (default 1.0). Overlapping calls mix. Other zones call this for their cues
// (button-click, frog, slide in/out); Z03 fires the deal / chip / swoosh cues.
void play_sfx(SfxId id, float gain = 1.0f);

// Immediately play a specific music track (e.g., a Shop / Settings preview),
// interrupting the current track; the active pool's shuffle resumes afterward.
void play_music(MusicTrackId track);

// Global output volume, 0-100 (default 50). Scales both music and SFX.
void set_volume(int volume_0_100);

// Independent mutes (Audio Settings). Mute All silences everything; Mute Music
// silences only music; Mute SFX silences only SFX. All compose with set_volume.
void set_mute_all(bool muted);
void set_mute_music(bool muted);
void set_mute_sfx(bool muted);

// The active music genre whose shuffle pool plays (default Lounge Jazz). Switching
// genres switches which pool plays.
void set_active_genre(MusicGenre genre);

// In-memory shuffle-pool membership for a genre. add joins the next reshuffle (and
// resumes playback if the active pool was empty); remove drops the track, halting
// the active pool immediately if it was the last one.
void add_to_shuffle(MusicGenre genre, MusicTrackId track);
void remove_from_shuffle(MusicGenre genre, MusicTrackId track);

}  // namespace poker_trainer::audio
