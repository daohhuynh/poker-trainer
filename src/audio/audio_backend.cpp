// Zone 03 audio backend bindings.
//
// Emscripten: SFX through miniaudio's Web Audio device (one-shot voices, a gain per
// call) and music through HTML5 <audio> elements (streamed, two crossfade slots).
// Native: no-op definitions so the pure `audio` library links into the test binaries
// without an audio backend (audio output is browser-verified only).
//
// Under Emscripten this TU is held to the reduced binding warning baseline (EM_ASM /
// miniaudio plumbing), mirroring the Z05 bridge_platform split. miniaudio.h is a
// SYSTEM include, so its single-translation-unit implementation never trips -Werror.

#include "audio/backend.hpp"

#ifdef __EMSCRIPTEN__

#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <emscripten/emscripten.h>

#include <array>
#include <cstddef>
#include <string>

namespace poker_trainer::audio::backend {

namespace {

// ---- SFX: a miniaudio engine + a small fixed voice pool ----

constexpr std::size_t kMaxVoices = 16;

ma_resource_manager g_resource_manager;
ma_engine g_engine;
bool g_engine_ready = false;

struct Voice {
    ma_sound sound;
    bool active = false;
};
std::array<Voice, kMaxVoices> g_voices;

void reap_finished_voices() {
    for (Voice& voice : g_voices) {
        if (voice.active && ma_sound_at_end(&voice.sound)) {
            ma_sound_uninit(&voice.sound);
            voice.active = false;
        }
    }
}

}  // namespace

void sfx_init() {
    if (g_engine_ready) {
        return;
    }
    // jobThreadCount = 0: no internal job thread (the build has no -pthread), so
    // sample decodes happen synchronously on the calling thread; the Web Audio
    // device itself is driven by the browser's audio thread.
    ma_resource_manager_config rm_config = ma_resource_manager_config_init();
    rm_config.jobThreadCount = 0;
    if (ma_resource_manager_init(&rm_config, &g_resource_manager) != MA_SUCCESS) {
        return;
    }
    ma_engine_config engine_config = ma_engine_config_init();
    engine_config.pResourceManager = &g_resource_manager;
    if (ma_engine_init(&engine_config, &g_engine) != MA_SUCCESS) {
        ma_resource_manager_uninit(&g_resource_manager);
        return;
    }
    g_engine_ready = true;
}

void sfx_play(std::string_view path, float gain) {
    if (!g_engine_ready) {
        return;
    }
    reap_finished_voices();
    Voice* slot = nullptr;
    for (Voice& voice : g_voices) {
        if (!voice.active) {
            slot = &voice;
            break;
        }
    }
    if (slot == nullptr) {
        return;  // all voices busy: drop this one-shot rather than grow unbounded
    }
    const std::string path_z(path);  // miniaudio needs a NUL-terminated path
    const ma_uint32 flags = MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION;
    if (ma_sound_init_from_file(&g_engine, path_z.c_str(), flags, nullptr, nullptr,
                                &slot->sound) != MA_SUCCESS) {
        return;  // missing / undecodable sample -> silent skip
    }
    ma_sound_set_volume(&slot->sound, gain);
    if (ma_sound_start(&slot->sound) != MA_SUCCESS) {
        ma_sound_uninit(&slot->sound);
        return;
    }
    slot->active = true;
}

void sfx_update() {
    if (g_engine_ready) {
        reap_finished_voices();
    }
}

// ---- Music: HTML5 <audio> with two crossfade slots ----

void music_init() {
    // clang-format off
    // NOTE: the EM_ASM JS body must contain no top-level comma — EM_ASM(code, ...)
    // takes the JS as its first macro argument, and the preprocessor balances only
    // (), not [] or {}, so a bare array literal `[a, b]` would split the macro args.
    // Build the slot array with push() (commas inside parens) instead.
    EM_ASM({
        if (!Module.PT_Music) {
            var slots = [];
            slots.push(new Audio());
            slots.push(new Audio());
            for (var i = 0; i < slots.length; ++i) {
                slots[i].loop = false;
                slots[i].preload = 'auto';
            }
            Module.PT_Music = { slots: slots };
        }
    });
    // clang-format on
}

void music_load(int slot, std::string_view url, float volume, bool play) {
    const std::string url_z(url);
    // clang-format off
    EM_ASM({
        var m = Module.PT_Music; if (!m) return;
        var a = m.slots[$0]; if (!a) return;
        a.src = UTF8ToString($1);
        a.volume = Math.max(0, Math.min(1, $2));
        if ($3) { var p = a.play(); if (p && p.catch) { p.catch(function() {}); } }
    }, slot, url_z.c_str(), static_cast<double>(volume), play ? 1 : 0);
    // clang-format on
}

void music_set_volume(int slot, float volume) {
    // clang-format off
    EM_ASM({
        var m = Module.PT_Music; if (!m) return;
        var a = m.slots[$0]; if (!a) return;
        a.volume = Math.max(0, Math.min(1, $1));
    }, slot, static_cast<double>(volume));
    // clang-format on
}

void music_stop(int slot) {
    // clang-format off
    EM_ASM({
        var m = Module.PT_Music; if (!m) return;
        var a = m.slots[$0]; if (!a) return;
        a.pause();
    }, slot);
    // clang-format on
}

void music_stop_all() {
    // clang-format off
    EM_ASM({
        var m = Module.PT_Music; if (!m) return;
        m.slots.forEach(function(a) { a.pause(); });
    });
    // clang-format on
}

float music_remaining_ms(int slot) {
    const double remaining = EM_ASM_DOUBLE({
        var m = Module.PT_Music; if (!m) return -1.0;
        var a = m.slots[$0]; if (!a) return -1.0;
        if (!isFinite(a.duration) || a.duration <= 0) return -1.0;
        return (a.duration - a.currentTime) * 1000.0;
    }, slot);
    return static_cast<float>(remaining);
}

bool music_ended(int slot) {
    const int ended = EM_ASM_INT({
        var m = Module.PT_Music; if (!m) return 0;
        var a = m.slots[$0]; if (!a) return 0;
        return a.ended ? 1 : 0;
    }, slot);
    return ended != 0;
}

}  // namespace poker_trainer::audio::backend

#else  // ---------------- Native (test) build: no-op backend ----------------

namespace poker_trainer::audio::backend {

void sfx_init() {}
void sfx_play(std::string_view, float) {}
void sfx_update() {}
void music_init() {}
void music_load(int, std::string_view, float, bool) {}
void music_set_volume(int, float) {}
void music_stop(int) {}
void music_stop_all() {}
float music_remaining_ms(int) { return -1.0f; }
bool music_ended(int) { return false; }

}  // namespace poker_trainer::audio::backend

#endif  // __EMSCRIPTEN__
