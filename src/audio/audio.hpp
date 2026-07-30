#pragma once

#include "audio/audio_paths.hpp"

#include "settings/settings.hpp"

#include <optional>
#include <vector>

// Zone 03 — Audio Engine (Module 2 audio), public umbrella header.
//
// This is the only audio header other zones include. It exposes the ZONES.md Z03
// control surface (play_music / play_sfx / set_volume / on_first_user_gesture) plus
// the Module 2 + Audio Settings additions (independent mutes, the single music
// rotation and the filter / order controls over it), and the three lifecycle hooks
// the Z05 bridge calls at its established integration points.
//
// Music model: there is ONE rotation, shared by every genre and ordered by when the
// user added each track — a playlist, not four per-genre pools. The genre selection
// is a FILTER over that one rotation and never edits it. See the rotation block
// below.
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
// swoosh on modal-stack edges, and progress the music rotation / crossfade. Driven
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
// interrupting the current track; the rotation resumes from it afterward.
void play_music(MusicTrackId track);

// Global output volume, 0-100 (default 50). Scales both music and SFX.
void set_volume(int volume_0_100);

// Independent mutes (Audio Settings). Mute All silences everything; Mute Music
// silences only music; Mute SFX silences only SFX. All compose with set_volume.
void set_mute_all(bool muted);
void set_mute_music(bool muted);
void set_mute_sfx(bool muted);

// ---- The music rotation ----
//
// One ordered, cross-genre rotation shared by every genre, edited only through
// these calls (the Shop drives them). It is a queue: order is add order, and the
// rotation is not reorderable.

// Append `track` to the end of the rotation. No-op if it is already there, so a
// repeat add leaves the track where it was rather than moving it to the back. If
// the rotation was silent (empty, or nothing in the filtered scope), playback
// resumes on the next frame.
void add_to_rotation(MusicTrackId track);

// Erase `track` from the rotation, preserving the order of everything else. No-op
// if absent. If `track` is the one currently sounding, playback skips to the next
// in-scope track immediately — a removed track is never left playing out — and
// halts into silence when the removal empties the scope.
void remove_from_rotation(MusicTrackId track);

[[nodiscard]] bool in_rotation(MusicTrackId track);

// The rotation in add order. Zone 11's Shop rotation list renders exactly this.
[[nodiscard]] std::vector<MusicTrackId> rotation_tracks();

// Scope playback to one genre's tracks within the rotation. std::nullopt is "All
// genres" (the whole rotation plays) and is the default. This is a view, never an
// editor: it adds, removes and reorders nothing. A filter that selects no track is
// silence, exactly as an empty rotation is. A track that is still in scope after
// the change keeps playing uninterrupted; one the new filter excludes is left
// immediately.
void set_genre_filter(std::optional<MusicGenre> genre);

// Loop (default) walks the filtered scope in add order and wraps; Shuffle draws
// from the filtered scope without repeating a track immediately. Changing the mode
// never interrupts what is already sounding — only the choice of the next track
// changes.
void set_playback_order(settings::MusicPlaybackOrder order);

}  // namespace poker_trainer::audio
