#pragma once

#include "audio/audio_paths.hpp"
#include "audio/shuffle_pool.hpp"

#include "settings/settings.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace poker_trainer::audio {

// Duration of the crossfade between consecutive tracks (ARCHITECTURE Module 2:
// "~3-second crossfades"). Progressed by the animation clock in audio_update.
inline constexpr float kCrossfadeMs = 3000.0f;

// The single music rotation: ONE ordered, cross-genre playlist that every genre
// shares, held in the order the user added tracks to it.
//
// Add order is user-visible — the Shop renders the rotation as a list, and Loop
// playback walks it front to back — so this is an ordered vector, never a set.
// Sorting or deduplicating it into a set would destroy the queue that is the whole
// point of the feature.
//
// The genre filter SCOPES playback to a subsequence of the rotation; it never
// adds, removes or reorders anything. std::nullopt means "All genres" (the whole
// rotation plays). A filter that selects nothing is silence, exactly as an empty
// rotation is: next() returns std::nullopt rather than falling back to any default
// track.
//
// The playback order decides how next() picks within that scope. Loop walks the
// scope in add order and wraps; Shuffle draws from it through the ShufflePool,
// which plays the whole scope before repeating and never repeats back-to-back.
class MusicRotation {
public:
    // `seed` seeds the Shuffle-mode RNG. Production seeds it from a
    // non-deterministic source (so each session's order differs); tests pass a
    // fixed seed for a reproducible sequence.
    explicit MusicRotation(std::uint64_t seed) noexcept;

    // Seed the rotation with every free starter track — a fresh profile starts
    // with all four already in rotation. Added in track-id order, which is genre
    // order (Lounge Jazz, Classical, Bossa Nova, Ambient).
    void seed_starter_tracks();

    // ---- Rotation membership (ordered; add order is the play order) ----

    // Append `track` to the end of the rotation. No-op if already present, so a
    // repeat add leaves the track at its original position instead of moving it.
    void add(MusicTrackId track);

    // Erase `track`, preserving the relative order of everything else. No-op if
    // absent. This edits the rotation only; skipping the track when it happens to
    // be the one sounding is the transport's job (music_remove_track).
    void remove(MusicTrackId track);

    [[nodiscard]] bool contains(MusicTrackId track) const noexcept;
    [[nodiscard]] bool empty() const noexcept { return tracks_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return tracks_.size(); }

    // The rotation in add order. The Shop's rotation list renders exactly this.
    [[nodiscard]] const std::vector<MusicTrackId>& tracks() const noexcept {
        return tracks_;
    }

    // ---- Filter and playback order ----

    // std::nullopt == All genres.
    [[nodiscard]] std::optional<MusicGenre> genre_filter() const noexcept {
        return filter_;
    }
    void set_genre_filter(std::optional<MusicGenre> genre);

    [[nodiscard]] settings::MusicPlaybackOrder playback_order() const noexcept {
        return order_;
    }
    void set_playback_order(settings::MusicPlaybackOrder order) noexcept {
        order_ = order;
    }

    // ---- The filtered scope (what actually plays) ----

    // True when `track` is in the rotation AND passes the current genre filter.
    [[nodiscard]] bool in_scope(MusicTrackId track) const noexcept;

    // True when the filter selects no track at all — either the rotation is empty,
    // or nothing in it belongs to the filtered genre. Both mean silence.
    [[nodiscard]] bool scope_empty() const noexcept;

    [[nodiscard]] std::size_t scope_size() const noexcept;

    // The next track to play, advancing whichever cursor the current playback
    // order uses. Returns std::nullopt when the scope is empty (the explicit
    // silence state).
    [[nodiscard]] std::optional<MusicTrackId> next();

private:
    [[nodiscard]] bool passes_filter(MusicTrackId track) const noexcept;
    [[nodiscard]] std::optional<MusicTrackId> next_in_loop();
    void sync_shuffle_scope();

    std::vector<MusicTrackId> tracks_;  // the rotation, in add order
    std::size_t loop_cursor_{0};        // index of the next Loop-mode candidate
    std::optional<MusicGenre> filter_;  // nullopt == All genres
    settings::MusicPlaybackOrder order_{settings::MusicPlaybackOrder::Loop};

    // Shuffle-mode draw order. Its membership mirrors the filtered scope and is
    // kept in sync incrementally, so tracks that survive a filter change keep
    // their place in the current shuffle cycle (and its no-immediate-repeat
    // memory survives with them).
    ShufflePool shuffle_;
};

// Music transport gate: the pure halt/resume decision given the gesture gate,
// whether the filtered scope is empty, and whether a track is currently sounding.
// Kept pure (no backend) so the spec's empty-scope / remove-last / add-to-empty
// behaviors are unit-testable; the crossfade ramp itself is backend glue
// (browser-verified).
enum class MusicGate : std::uint8_t {
    Silent,    // gate closed (no gesture yet) or empty scope & nothing sounding
    Halt,      // scope emptied while sounding -> stop immediately
    Start,     // scope non-empty and nothing sounding -> start the next track
    Continue,  // scope non-empty and already sounding -> advance / crossfade
};

// `scope_empty` is the FILTERED scope, not the whole rotation: a rotation holding
// only Lounge Jazz tracks is silent while the filter is Classical, even though the
// rotation itself is non-empty.
[[nodiscard]] MusicGate music_gate(bool gesture_started, bool scope_empty,
                                   bool sounding) noexcept;

}  // namespace poker_trainer::audio
