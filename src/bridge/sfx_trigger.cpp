#include "bridge/sfx_trigger.hpp"

#include <functional>
#include <utility>

// Single-threaded (browser main thread): the seam is wired once during boot and read from
// input handlers, both on the same thread. No synchronization needed.

namespace poker_trainer::bridge {

namespace {

// The wired SFX player (boot: audio::play_sfx). Unset => play_sfx is a silent no-op, so the
// native test build links without any audio backend.
std::function<void(audio::SfxId)> g_sfx_player{};

}  // namespace

void set_sfx_player(std::function<void(audio::SfxId)> player) {
    g_sfx_player = std::move(player);
}

void play_sfx(audio::SfxId id) {
    if (g_sfx_player) {
        g_sfx_player(id);
    }
}

}  // namespace poker_trainer::bridge
