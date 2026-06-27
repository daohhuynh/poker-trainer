#pragma once

#include <cstddef>
#include <string>
#include <string_view>

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

}  // namespace poker_trainer::modal
