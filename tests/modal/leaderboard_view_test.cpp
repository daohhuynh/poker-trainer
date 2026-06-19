#include "modal/leaderboard_view.hpp"

#include <string>

#include <gtest/gtest.h>

// Zone 11 — leaderboard search filter: case-insensitive plain substring, no regex,
// 32-char keystroke cap.

namespace {

using namespace poker_trainer::modal;

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

}  // namespace
