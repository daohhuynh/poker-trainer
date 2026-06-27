#include "modal/leaderboard_view.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

// Zone 11 — leaderboard search filter (case-insensitive plain substring, no regex, 32-char
// cap) and the pure list-stop navigation: the rolling digit buffer, arrow ±1, and the
// search jump-by-name (highest-ranked match).

namespace {

using namespace poker_trainer::modal;

// A small contiguous board: ranks 1..max, names tagged so substring matches are testable.
[[nodiscard]] std::vector<LeaderboardRow> make_board(std::uint32_t max_rank) {
    std::vector<LeaderboardRow> rows;
    for (std::uint32_t r = 1; r <= max_rank; ++r) {
        rows.push_back(LeaderboardRow{r, "Player" + std::to_string(r), 0});
    }
    return rows;
}

TEST(LeaderboardSearch, EmptyQueryMatchesEveryone) {
    EXPECT_TRUE(leaderboard_username_matches("Alice", ""));
}

TEST(LeaderboardSearch, CaseInsensitiveSubstring) {
    EXPECT_TRUE(leaderboard_username_matches("PokerKing", "king"));
    EXPECT_TRUE(leaderboard_username_matches("PokerKing", "KING"));
    EXPECT_TRUE(leaderboard_username_matches("PokerKing", "Poker"));
    EXPECT_TRUE(leaderboard_username_matches("PokerKing", "erki"));
}

TEST(LeaderboardSearch, NonMatchReturnsFalse) {
    EXPECT_FALSE(leaderboard_username_matches("Alice", "bob"));
}

TEST(LeaderboardSearch, SpecialCharsAreLiteralNotRegex) {
    EXPECT_TRUE(leaderboard_username_matches("a.b*c", "b*c"));   // '*' is a literal char
    EXPECT_FALSE(leaderboard_username_matches("abc", ".*"));     // not a wildcard
    EXPECT_TRUE(leaderboard_username_matches("hi there", " th"));  // spaces are literal
}

TEST(LeaderboardSearch, QueryLongerThanUsernameNeverMatches) {
    EXPECT_FALSE(leaderboard_username_matches("ab", "abc"));
}

TEST(LeaderboardSearch, ClampEnforces32CharCap) {
    const std::string over(65, 'x');
    EXPECT_EQ(leaderboard_clamp_search(over).size(), kLeaderboardSearchMaxChars);
    EXPECT_EQ(leaderboard_clamp_search("short"), "short");
    const std::string exact(kLeaderboardSearchMaxChars, 'y');
    EXPECT_EQ(leaderboard_clamp_search(exact).size(), kLeaderboardSearchMaxChars);
}

// ----- rank_jump_digit: rolling window, leading-0, per-keystroke clamp -----

TEST(RankJumpDigit, SingleDigitJumpsToThatRank) {
    RankJumpBuffer buf{};
    EXPECT_EQ(rank_jump_digit(buf, 9, /*now=*/0, /*min=*/1, /*max=*/100), 9);
}

TEST(RankJumpDigit, AppendsWithinWindowThenClearsAfter) {
    // Spec example: 9 -> rank 9; within 3s a 3 -> rank 93; after >3s a 2 -> rank 2.
    RankJumpBuffer buf{};
    EXPECT_EQ(rank_jump_digit(buf, 9, 0, 1, 100), 9);
    EXPECT_EQ(rank_jump_digit(buf, 3, 2000, 1, 100), 93);     // 2000ms after the 9 (<= window)
    EXPECT_EQ(rank_jump_digit(buf, 2, 5001, 1, 100), 2);      // 3001ms after the 3 (> window): fresh
}

TEST(RankJumpDigit, ExactlyWindowAppendsJustOverResets) {
    RankJumpBuffer a{};
    EXPECT_EQ(rank_jump_digit(a, 5, 0, 1, 100), 5);
    EXPECT_EQ(rank_jump_digit(a, 5, kRankJumpWindowMs, 1, 100), 55);  // exactly 3000ms: appends
    RankJumpBuffer b{};
    EXPECT_EQ(rank_jump_digit(b, 5, 0, 1, 100), 5);
    EXPECT_EQ(rank_jump_digit(b, 7, kRankJumpWindowMs + 1, 1, 100), 7);  // 3001ms: fresh
}

TEST(RankJumpDigit, LeadingZeroIsIgnoredThenNextDigitStartsFresh) {
    RankJumpBuffer buf{};
    EXPECT_EQ(rank_jump_digit(buf, 0, 0, 1, 100), std::nullopt);  // leading 0 -> no-op
    EXPECT_FALSE(buf.active);
    EXPECT_EQ(rank_jump_digit(buf, 1, 100, 1, 100), 1);  // fresh sequence
}

TEST(RankJumpDigit, ZeroIsValidOnceSequenceStarted) {
    RankJumpBuffer buf{};
    EXPECT_EQ(rank_jump_digit(buf, 1, 0, 1, 100), 1);
    EXPECT_EQ(rank_jump_digit(buf, 0, 1000, 1, 100), 10);  // 0 appends now -> 10
}

TEST(RankJumpDigit, ClampsEveryIntermediateValue) {
    // 111 with max 100 walks 1 -> 11 -> 100 (clamp on each keystroke, not just the final).
    RankJumpBuffer buf{};
    EXPECT_EQ(rank_jump_digit(buf, 1, 0, 1, 100), 1);
    EXPECT_EQ(rank_jump_digit(buf, 1, 1000, 1, 100), 11);
    EXPECT_EQ(rank_jump_digit(buf, 1, 2000, 1, 100), 100);
    // A further digit stays clamped at the max (no overflow on a long sequence).
    EXPECT_EQ(rank_jump_digit(buf, 1, 2500, 1, 100), 100);
}

// ----- rank_jump_arrow: ±1 relative to current, clamped -----

TEST(RankJumpArrow, NoCurrentSnapsToMin) {
    EXPECT_EQ(rank_jump_arrow(-1, +1, 1, 100), 1);
    EXPECT_EQ(rank_jump_arrow(-1, -1, 1, 100), 1);
}

TEST(RankJumpArrow, DownIsNextRankUpIsPrevious) {
    EXPECT_EQ(rank_jump_arrow(5, +1, 1, 100), 6);  // down = next rank
    EXPECT_EQ(rank_jump_arrow(5, -1, 1, 100), 4);  // up = previous rank
}

TEST(RankJumpArrow, ClampsAtBothEnds) {
    EXPECT_EQ(rank_jump_arrow(1, -1, 1, 100), 1);
    EXPECT_EQ(rank_jump_arrow(100, +1, 1, 100), 100);
}

// ----- leaderboard_highest_match: best (lowest) rank among matches -----

TEST(LeaderboardHighestMatch, ReturnsLowestRankAmongMatches) {
    const std::vector<LeaderboardRow> rows{
        LeaderboardRow{3, "alice", 0}, LeaderboardRow{1, "bob", 0}, LeaderboardRow{5, "alanna", 0}};
    EXPECT_EQ(leaderboard_highest_match(rows, "al"), 3);  // alice(3) + alanna(5) -> 3
    EXPECT_EQ(leaderboard_highest_match(rows, "bob"), 1);
}

TEST(LeaderboardHighestMatch, NoMatchReturnsNegativeOne) {
    EXPECT_EQ(leaderboard_highest_match(make_board(10), "zzzzz"), -1);
}

TEST(LeaderboardHighestMatch, EmptyQueryMatchesEveryoneSoBestIsRankOne) {
    EXPECT_EQ(leaderboard_highest_match(make_board(10), ""), 1);
}

// ----- leaderboard_handoff_rank: the search -> list landing for the 3 handoff cases -----

TEST(LeaderboardHandoffRank, Case1EmptySearchLandsRankOne) {
    EXPECT_EQ(leaderboard_handoff_rank(make_board(50), ""), 1);
}

TEST(LeaderboardHandoffRank, Case2MatchLandsHighestMatch) {
    const std::vector<LeaderboardRow> rows{
        LeaderboardRow{3, "alice", 0}, LeaderboardRow{1, "bob", 0}, LeaderboardRow{5, "alanna", 0}};
    EXPECT_EQ(leaderboard_handoff_rank(rows, "al"), 3);  // alice(3) + alanna(5) -> 3
}

TEST(LeaderboardHandoffRank, Case3NoMatchFallsBackToRankOne) {
    EXPECT_EQ(leaderboard_handoff_rank(make_board(50), "zzzzz"), 1);
}

TEST(LeaderboardHandoffRank, EmptyBoardReturnsNegativeOne) {
    EXPECT_EQ(leaderboard_handoff_rank({}, "anything"), -1);
    EXPECT_EQ(leaderboard_handoff_rank({}, ""), -1);
}

}  // namespace
