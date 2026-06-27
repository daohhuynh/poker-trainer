#pragma once

#include "modal/modals.hpp"

#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Zone 11 — Leaderboard view (ARCHITECTURE L575), rendered inside the Shop modal frame
// (in-place content swap). The ranked top-100 list, the live case-insensitive substring
// search (32-char cap, Enter-to-jump + highlight), the persistent "your rank" bottom row
// (logged-in opted-in / opted-out / guest variants), and the loading / error / Retry
// states render from a boot-wired LeaderboardController (fetch + your-rank + the guest /
// opt-out links). The pure search filter + clamp are unit-tested; the render is a marked
// seam (CLAUDE.md §9, browser-verified). render_leaderboard_view is declared in
// modal_base.hpp alongside the other per-modal render entry points.

namespace poker_trainer::modal {

// Keystroke-level cap: input beyond 32 chars is rejected (ARCHITECTURE).
inline constexpr std::size_t kLeaderboardSearchMaxChars = 32;

// Live filter: true when `username` contains `query` as a case-insensitive plain
// substring. An empty query matches everything. Special characters (punctuation,
// spaces, symbols) are literal — no regex parsing. Case folding is ASCII; full
// Unicode case-folding is a seam (ARCHITECTURE allows unicode input, but the V1
// match is a literal byte-substring with ASCII case-insensitivity).
[[nodiscard]] bool leaderboard_username_matches(std::string_view username,
                                                std::string_view query) noexcept;

// Enforce the 32-char keystroke cap: returns `candidate` truncated to the cap.
[[nodiscard]] std::string leaderboard_clamp_search(std::string_view candidate);

// ----- Pure list-stop navigation logic (jump-by-rank), unit-tested -----

// Apply a digit (0-9) to `buf` at `now_ms`, given the smallest and largest present ranks.
// Returns the rank to highlight after this keystroke, or nullopt for a no-op. Rules:
//   * A digit more than kRankJumpWindowMs after the previous one (or the first ever) starts
//     a FRESH number; within the window it APPENDS (value*10 + digit).
//   * A leading 0 (fresh sequence, digit 0) is ignored — the sequence stays inactive and
//     this returns nullopt. Once a sequence is active, 0 is a valid appended digit (10, 20).
//   * The result is clamped to [min_rank, max_rank] on EVERY keystroke (so typing 111 with
//     max 100 walks 1 -> 11 -> 100). The raw typed number keeps accumulating (capped just
//     above max_rank to avoid overflow) so a later digit still extends what was typed.
// A leading '-' is not representable in KeyCode and so never reaches this function; the
// leading-0 rule covers the only no-op start the dispatch can produce.
[[nodiscard]] std::optional<std::int64_t> rank_jump_digit(RankJumpBuffer& buf, int digit,
                                                          std::uint64_t now_ms,
                                                          std::int64_t min_rank,
                                                          std::int64_t max_rank) noexcept;

// Move a highlight by `delta` (+1 = next rank / -1 = previous), clamped to [min,max]. A
// `current` below min_rank (e.g. -1, "no highlight yet") snaps to min_rank.
[[nodiscard]] std::int64_t rank_jump_arrow(std::int64_t current, int delta,
                                           std::int64_t min_rank, std::int64_t max_rank) noexcept;

// The lowest (best) rank whose name matches `query` (case-insensitive substring), or -1
// when nothing matches. The search Enter / result-click jump to this rank.
[[nodiscard]] std::int64_t leaderboard_highest_match(const std::vector<LeaderboardRow>& rows,
                                                     std::string_view query);

// The rank the search-to-list handoff lands on for `query` (Enter OR forward-Tab, which are
// interchangeable): the highest-ranked (lowest-rank) substring match, or the rank-1 default
// (the lowest present rank) when `query` is empty OR matches nothing. -1 only when the board
// itself is empty. The rank-1 default backs Case 1 (empty search) and Case 3 (no match) of
// the handoff; a real match backs Case 2.
[[nodiscard]] std::int64_t leaderboard_handoff_rank(const std::vector<LeaderboardRow>& rows,
                                                    std::string_view query);

// ----- Keyboard focus traversal (5 stops: Shop icon, search, list, your-rank, X close) -----

// The fixed 5-stop focus order (wraps from X close back to the Shop icon).
[[nodiscard]] std::span<const backbone::FocusableId> leaderboard_focus_list() noexcept;

// Reset the per-open navigation state and (re)populate the modal's own focus registry
// (search = text field; the other four stops non-text). Called from open_leaderboard_modal.
void leaderboard_on_open(ModalRuntime& runtime);

// ModalLayer key dispatch for the Leaderboard. The search stop yields typing + Enter to
// the ImGui field; the list stop runs the digit/arrow jump; the your-rank stop runs the
// guest digit-key links / opt-in; the Shop icon + X close activate on Space/Enter. Returns
// true when consumed; false (e.g. Tab) falls through to the backbone focus-nav handler.
[[nodiscard]] bool leaderboard_dispatch_key(ModalRuntime& runtime, const backbone::KeyEvent& e);

}  // namespace poker_trainer::modal
