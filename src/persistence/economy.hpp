#pragma once

#include "audio/audio_paths.hpp"
#include "persistence/persistence_schema.hpp"

#include <cstdint>

// Module 7 — the tomato economy core (pure logic over the persisted wallet + music
// library). This is the testable heart of the Retention Engine: the dual-track award
// math, the Shop purchase / spend rules, and the owned / in-rotation predicates. The
// boot layer wires these into the grading-complete award hook and the Shop UI's
// buy/add/remove callbacks; the Shop UI (Zone 11) never calls in here — it renders a
// boot-computed snapshot. Audio playback (Zone 03) and persistence I/O (the rest of
// Zone 04) are the caller's job; these functions only transform value state.

namespace poker_trainer::persistence {

// Tomatoes granted per fully-passed scenario (all math correct AND total time at or
// under the target — the existing GradingCompleteEvent.passed conjunction).
//
// SEAM(award amount): ARCHITECTURE Module 7 specifies WHEN tomatoes are awarded (the
// two-condition pass) and the catalog cost (25/track, 200 total) but NOT the per-pass
// grant. 1 is the minimal unit (200 clean passes unlock the full catalog); change this
// one constant if the design wants a different earn cadence.
inline constexpr std::uint64_t kTomatoesPerPass = 1;

// Dual-track award: both spendable and lifetime increase by `amount`. Lifetime is the
// leaderboard metric and never decreases; only spendable is spent in the Shop. Returns
// the new wallet (saturating add guards the unsigned overflow corner).
[[nodiscard]] TomatoesState awarded(TomatoesState wallet, std::uint64_t amount) noexcept;

// Apply one scenario-pass award (kTomatoesPerPass) to `state` in place. Called from the
// grading-complete hook only on a passing verdict.
void apply_pass_award(AppState& state) noexcept;

// True when the spendable balance covers `price` (price 0 — the starter tracks — is
// always affordable).
[[nodiscard]] bool can_afford(const TomatoesState& wallet, std::uint64_t price) noexcept;

// A track is owned when it is a free starter or appears in unlocked_track_ids.
[[nodiscard]] bool is_track_owned(const MusicLibraryState& lib,
                                  audio::MusicTrackId track) noexcept;

// A track is in rotation when it appears in active_pool_track_ids (its genre's shuffle
// pool). Independent of ownership in storage shape, though only owned tracks are ever
// added.
[[nodiscard]] bool is_track_in_pool(const MusicLibraryState& lib,
                                    audio::MusicTrackId track) noexcept;

// Attempt to purchase `track`. Succeeds only when the track is currently LOCKED (not
// already owned) AND affordable. On success: spendable decrements by the track price
// and the track joins unlocked_track_ids (kept sorted + unique); lifetime is untouched
// (spending never reduces the leaderboard metric) and the shuffle pool is NOT changed
// (a freshly-bought track lands in the Owned-not-in-shuffle state per the Shop spec).
// Returns true on commit, false when already owned or unaffordable (state unchanged).
[[nodiscard]] bool purchase_track(AppState& state, audio::MusicTrackId track);

// Add `track` to its genre's shuffle pool (active_pool_track_ids, sorted + unique). A
// no-op when the track is not owned or already present. Mutates persisted state only;
// the caller updates the in-memory audio pool via audio::add_to_shuffle.
void add_track_to_pool(MusicLibraryState& lib, audio::MusicTrackId track);

// Remove `track` from its genre's shuffle pool. A no-op when absent. Mutates persisted
// state only; the caller updates the in-memory audio pool via audio::remove_from_shuffle.
void remove_track_from_pool(MusicLibraryState& lib, audio::MusicTrackId track);

}  // namespace poker_trainer::persistence
