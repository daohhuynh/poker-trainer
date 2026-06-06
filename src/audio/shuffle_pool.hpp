#pragma once

#include "audio/audio_paths.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <vector>

namespace poker_trainer::audio {

// One genre's in-memory shuffle pool: the set of tracks currently in the pool and
// a randomized play order that reshuffles automatically at the end of each cycle,
// so the whole pool is heard before any track repeats (ARCHITECTURE Module 2: "no
// single-track loop fatigue"). Persistence of pool membership is Module 7's job
// (IDBFS); this type holds only the in-memory state Z03 manages at runtime.
//
// The "empty pool" case is the explicit silence state: next() returns std::nullopt
// rather than falling back to any default track.
class ShufflePool {
public:
    // `seed` seeds the shuffle RNG. Production seeds it from a non-deterministic
    // source (so each session's order differs); tests pass a fixed seed for a
    // reproducible sequence.
    explicit ShufflePool(std::uint64_t seed) noexcept;

    // Add a track to the pool. No-op if already present. A newly added track joins
    // the next reshuffled cycle (it does not interrupt the current order).
    void add(MusicTrackId track);

    // Remove a track. No-op if absent. If the track has not yet played in the
    // current cycle it is also dropped from the remaining order, so it is not
    // selected again this cycle.
    void remove(MusicTrackId track);

    [[nodiscard]] bool empty() const noexcept { return members_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return members_.size(); }
    [[nodiscard]] bool contains(MusicTrackId track) const noexcept;

    // The next track to play, advancing the cursor. Returns std::nullopt when the
    // pool is empty (the explicit silence state). Reshuffles transparently when a
    // cycle completes; when the pool has more than one member the first track of a
    // new cycle never repeats the last track of the previous cycle.
    [[nodiscard]] std::optional<MusicTrackId> next();

    // Read-only view of current membership (unordered), for inspection/tests.
    [[nodiscard]] const std::vector<MusicTrackId>& members() const noexcept {
        return members_;
    }

private:
    void reshuffle();

    std::vector<MusicTrackId> members_;
    std::vector<MusicTrackId> order_;  // current cycle's sequence; [cursor_, end) is unplayed
    std::size_t cursor_{0};            // index of the NEXT track within order_
    std::optional<MusicTrackId> last_played_;
    std::mt19937_64 rng_;
};

}  // namespace poker_trainer::audio
