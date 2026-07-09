#pragma once

// Cross-zone SFX-trigger seam (Zone 05).
//
// Some zones must fire a one-shot SFX in response to input but do NOT depend on Zone 03
// (audio). For example, Zone 09's math-input keybinds fire the ButtonClickConfirmation cue
// when a 1-6 key focuses an input, yet ZONES.md excludes Zone 03 from Zone 09's dependency
// set. Rather than add that cross-zone edge, such a zone depends on the bridge (which it
// already does) and calls play_sfx here; boot wires the seam to audio::play_sfx once at
// startup. Unset (native tests) => a silent no-op. audio_paths.hpp is a Phase 0 header, so
// audio::SfxId is universally includable without taking a Zone 03 dependency.

#include "audio/audio_paths.hpp"  // audio::SfxId

#include <functional>

namespace poker_trainer::bridge {

// Wire the SFX player (boot passes a lambda over audio::play_sfx). Call once at startup.
void set_sfx_player(std::function<void(audio::SfxId)> player);

// Fire a one-shot SFX through the wired player, which routes through Zone 03's normal
// mute/volume handling. A no-op when the seam is unset (native tests). This is the trigger
// seam, not an audio engine — it only forwards to whatever boot wired.
void play_sfx(audio::SfxId id);

}  // namespace poker_trainer::bridge
