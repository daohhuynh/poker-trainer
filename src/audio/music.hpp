#pragma once

#include "audio/audio_paths.hpp"
#include "audio/shuffle_pool.hpp"

#include <array>
#include <cstdint>

namespace poker_trainer::audio {

// Duration of the crossfade between consecutive tracks (ARCHITECTURE Module 2:
// "~3-second crossfades"). Progressed by the animation clock in audio_update.
inline constexpr float kCrossfadeMs = 3000.0f;

// The four genres' in-memory shuffle pools plus the active-genre selection. The
// active genre's pool is the one that plays; switching genres switches pools.
class MusicEngine {
public:
    explicit MusicEngine(std::uint64_t seed) noexcept;

    // Seed each genre's pool with its free starter track (the default in-memory
    // composition; Module 7 later restores the persisted owned-track set).
    void seed_starter_tracks();

    [[nodiscard]] MusicGenre active_genre() const noexcept { return active_; }
    void set_active_genre(MusicGenre genre) noexcept { active_ = genre; }

    [[nodiscard]] ShufflePool& pool(MusicGenre genre) noexcept;
    [[nodiscard]] ShufflePool& active_pool() noexcept { return pool(active_); }

    void add(MusicGenre genre, MusicTrackId track) { pool(genre).add(track); }
    void remove(MusicGenre genre, MusicTrackId track) { pool(genre).remove(track); }

private:
    std::array<ShufflePool, kMusicGenreCount> pools_;
    MusicGenre active_{MusicGenre::LoungeJazz};
};

// Music transport gate: the pure halt/resume decision given the gesture gate, the
// active pool's empty state, and whether a track is currently sounding. Kept pure
// (no backend) so the spec's empty-pool / remove-last / add-to-empty behaviors are
// unit-testable; the crossfade ramp itself is backend glue (browser-verified).
enum class MusicGate : std::uint8_t {
    Silent,    // gate closed (no gesture yet) or empty pool & nothing sounding
    Halt,      // pool emptied while sounding -> stop immediately
    Start,     // pool non-empty and nothing sounding -> start the next track
    Continue,  // pool non-empty and already sounding -> advance / crossfade
};

[[nodiscard]] MusicGate music_gate(bool gesture_started, bool pool_empty,
                                   bool sounding) noexcept;

}  // namespace poker_trainer::audio
