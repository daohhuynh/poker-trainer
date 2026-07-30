#pragma once

#include "audio/audio_paths.hpp"
#include "persistence/persistence_schema.hpp"

#include <array>
#include <cstdint>
#include <vector>

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

// Position-based Shop track prices — the single retune point for the catalog. Within each
// genre the three consecutive tracks are the free starter (position 0) and the two paid
// unlocks (positions 1 and 2). The sealed catalog field MusicTrackInfo::price_tomatoes is a
// frozen Phase-0 contract and is NOT the live price; this table supersedes it. Paid catalog
// total: 4 genres x (5 + 10) = 60 tomatoes. Change these three values to retune pricing.
inline constexpr std::array<std::uint32_t, 3> kTrackPriceByGenrePosition{0u, 5u, 10u};

// The live Shop price of `track`, derived from its position within its genre (the starter,
// position 0, is free). This — not the sealed price_tomatoes field — is the source of truth
// for the Shop snapshot and purchase_track.
[[nodiscard]] std::uint32_t track_price(audio::MusicTrackId track) noexcept;

// True when the spendable balance covers `price` (price 0 — the starter tracks — is
// always affordable).
[[nodiscard]] bool can_afford(const TomatoesState& wallet, std::uint64_t price) noexcept;

// A track is owned when it is a free starter or appears in unlocked_track_ids.
[[nodiscard]] bool is_track_owned(const MusicLibraryState& lib,
                                  audio::MusicTrackId track) noexcept;

// A track is in rotation when it appears in active_pool_track_ids (the one global,
// cross-genre rotation). Independent of ownership in storage shape, though only owned
// tracks are ever added. Linear scan, not a binary search: the rotation is ordered by
// ADD ORDER, not by id.
[[nodiscard]] bool is_track_in_pool(const MusicLibraryState& lib,
                                    audio::MusicTrackId track) noexcept;

// Attempt to purchase `track`. Succeeds only when the track is currently LOCKED (not
// already owned) AND affordable. On success: spendable decrements by the track price
// and the track joins unlocked_track_ids (kept sorted + unique); lifetime is untouched
// (spending never reduces the leaderboard metric) and the shuffle pool is NOT changed
// (a freshly-bought track lands in the Owned-not-in-shuffle state per the Shop spec).
// Returns true on commit, false when already owned or unaffordable (state unchanged).
[[nodiscard]] bool purchase_track(AppState& state, audio::MusicTrackId track);

// APPEND `track` to the end of the rotation. A no-op when the track is not owned or
// already present (so it never duplicates, and re-adding does not move a track to the
// back). Mutates persisted state only; the caller updates the live audio rotation via
// audio::add_to_rotation.
void add_track_to_pool(MusicLibraryState& lib, audio::MusicTrackId track);

// Remove `track` from the rotation, preserving the order of everything else. A no-op
// when absent. Mutates persisted state only; the caller updates the live audio rotation
// via audio::remove_from_rotation.
void remove_track_from_pool(MusicLibraryState& lib, audio::MusicTrackId track);

// The rotation as typed track ids, in add order — what the Shop's rotation list renders
// and what boot replays into the audio engine. Ids the catalog does not define are
// skipped (see normalize_rotation).
[[nodiscard]] std::vector<audio::MusicTrackId> rotation_tracks(const MusicLibraryState& lib);

// Append the four free starter tracks to the rotation, in genre order (Lounge Jazz,
// Classical, Bossa Nova, Ambient). ARCHITECTURE Module 7 (Shop UI — "Default tracks"):
// the first track of each genre is permanently owned and pre-added on first session.
// This is the ONLY seeding path: boot calls it on a profile whose rotation is empty
// after normalization, which is the fresh-profile case. From that point on
// active_pool_track_ids is the single source of truth, so a later removal sticks across
// reloads instead of being re-seeded. Idempotent, and it preserves (never reorders)
// tracks already in the rotation.
void add_starter_tracks_to_pool(MusicLibraryState& lib);

// Bring a loaded rotation into the invariants the playback model depends on: every id
// names a real catalog track, and no id appears twice. Order is preserved; the FIRST
// occurrence of a duplicate wins, because that is the position the user's playlist has
// had all along.
//
// MIGRATION of pre-rotation profiles: active_pool_track_ids used to hold the four
// per-genre shuffle pools unioned into one sorted set. Those ids are exactly the tracks
// the user had chosen to hear, so they are adopted verbatim as the initial rotation —
// nothing is dropped and nothing is re-seeded. The only observable change is that their
// ascending-id order now also means playback order, which is a defensible reading of a
// set that never carried an order to begin with. A profile that had tracks therefore
// cannot come back empty, and cannot gain duplicates.
void normalize_rotation(MusicLibraryState& lib);

}  // namespace poker_trainer::persistence
