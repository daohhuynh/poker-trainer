#pragma once

#include <string_view>

namespace poker_trainer::audio::backend {

// The audio output seam. Under Emscripten these are implemented by miniaudio (SFX)
// and HTML5 <audio> (music) in audio_backend.cpp; in the native test build they are
// no-ops (audio output is browser-verified only). The pure audio engine calls these
// and never touches a real device, which keeps the engine logic unit-testable.

// ---- SFX (miniaudio one-shot voices) ----

// Initialize the SFX engine. Must run inside the first user-gesture callstack so
// the browser starts the Web Audio device in the "running" state (autoplay policy).
// Idempotent.
void sfx_init();

// Fire-and-forget play of the sample at `path` at absolute `gain` in [0, 1].
// Overlapping calls mix. A missing or undecodable sample is skipped silently.
void sfx_play(std::string_view path, float gain);

// Reap finished one-shot voices. Call once per frame.
void sfx_update();

// ---- Music (HTML5 <audio> streaming, two crossfade slots: 0 and 1) ----

// Set up the two audio slots. Run inside the first user-gesture callstack.
void music_init();

// Point `slot` at `url` at absolute `volume` in [0, 1] and optionally start it.
void music_load(int slot, std::string_view url, float volume, bool play);

// Set the absolute output volume of `slot` in [0, 1].
void music_set_volume(int slot, float volume);

// Pause `slot` (single slot / all slots).
void music_stop(int slot);
void music_stop_all();

// Milliseconds left in `slot`'s current track, or a negative value when unknown
// (metadata not yet loaded, no track, or streaming source without a duration).
[[nodiscard]] float music_remaining_ms(int slot);

// Whether `slot`'s current track has reached its end.
[[nodiscard]] bool music_ended(int slot);

}  // namespace poker_trainer::audio::backend
