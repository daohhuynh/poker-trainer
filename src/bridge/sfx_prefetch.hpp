#pragma once

// SFX delivery into the Emscripten filesystem (Zone 05 owns the CDN fetch path).
//
// Zone 03's SFX play through miniaudio, which loads samples with fopen — so the
// .ogg files (audio_paths.hpp) must exist in the Emscripten MEMFS by the time a cue
// fires. The PNG pipeline streams bytes into textures and never needs them on disk;
// SFX do. This reuses the SAME production CDN fetch (make_cdn_fetch) the PNG loader
// uses and writes each downloaded sample into MEMFS, so a sample dropped onto the
// CDN later is on disk for miniaudio with no further work. A missing / 404 sample is
// skipped (the file simply stays absent, which Z03 already degrades on). Music is
// unaffected — it streams by URL through HTML5 <audio> and needs no filesystem.
//
// Emscripten-only (compiled into the wasm app's bridge_platform layer, never the
// native test build), like cdn_fetch.hpp.

namespace poker_trainer::bridge {

// Fetch every SFX sample named in audio_paths.hpp from the CDN and write it into
// MEMFS at its asset-root-relative path. Fire-and-forget: each fetch resolves
// asynchronously and writes on completion; a failed fetch is silently skipped. Call
// once at boot, after the PNG tier load is kicked (before any cue would fire).
void prefetch_sfx_into_memfs();

}  // namespace poker_trainer::bridge
