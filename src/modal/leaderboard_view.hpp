#pragma once

#include <cstddef>
#include <string>
#include <string_view>

// Zone 11 — Leaderboard view (ARCHITECTURE L575), rendered inside the Shop modal
// frame. CONSUMER-LIGHT this wave: the leaderboard data is server-side (absent) and
// the Shop track-purchase content is Module 7 (unbuilt), so this builds a thin
// shell — search input (32-char cap, case-insensitive plain-substring filter, no
// regex), list scaffold, and the loading / error / retry states — over stubbed/
// empty data. The pure search filter is unit-tested; the render is a marked seam.

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
