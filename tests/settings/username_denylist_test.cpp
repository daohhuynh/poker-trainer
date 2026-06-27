#include "settings/username_denylist.hpp"

#include <gtest/gtest.h>

namespace pt = poker_trainer::settings;

// normalize_for_denylist: lowercasing, leetspeak folding, separator stripping.

TEST(NormalizeForDenylist, LowercasesLetters) {
    EXPECT_EQ(pt::normalize_for_denylist("BadWord"), "badword");
}

TEST(NormalizeForDenylist, FoldsLeetspeakDigitsAndSymbols) {
    // 4->a, 0->o, 1->i, 3->e, 5->s, 7->t, @->a, $->s, !->i, +->t
    EXPECT_EQ(pt::normalize_for_denylist("b4dw0rd"), "badword");
    EXPECT_EQ(pt::normalize_for_denylist("d3nym3"), "denyme");
    EXPECT_EQ(pt::normalize_for_denylist("@$!+"), "asit");
}

TEST(NormalizeForDenylist, StripsSeparatorsAndPunctuation) {
    EXPECT_EQ(pt::normalize_for_denylist("b a d_w-o.r d"), "badword");
}

TEST(NormalizeForDenylist, KeepsMeaninglessDigitsAsContent) {
    // A digit with no leet meaning (2,6,8,9) is retained, not dropped.
    EXPECT_EQ(pt::normalize_for_denylist("user2"), "user2");
}

TEST(NormalizeForDenylist, AllSymbolsNormalizesToEmpty) {
    EXPECT_TRUE(pt::normalize_for_denylist("___...---").empty());
}

// is_username_denylisted: substring match against the compiled LDNOOBW list
// (detail::kDenylistTermsData, generated from en.txt). The assertions below use
// mild but real entries from that list ("anus", "butt") so they survive the swap
// from the old placeholder data; the matching mechanism itself is unchanged.

TEST(IsUsernameDenylisted, PlainMatchRejected) {
    EXPECT_TRUE(pt::is_username_denylisted("anus"));
    EXPECT_TRUE(pt::is_username_denylisted("butt"));
}

TEST(IsUsernameDenylisted, CaseInsensitiveMatchRejected) {
    EXPECT_TRUE(pt::is_username_denylisted("ANUS"));
    EXPECT_TRUE(pt::is_username_denylisted("Butt"));
}

TEST(IsUsernameDenylisted, LeetspeakMatchRejected) {
    // 4->a, @->a, $->s, 7->t: "@nu$" -> "anus", "bu77" -> "butt".
    EXPECT_TRUE(pt::is_username_denylisted("@nu$"));
    EXPECT_TRUE(pt::is_username_denylisted("BU77"));
}

TEST(IsUsernameDenylisted, SubstringMatchRejected) {
    // The spec rejects names "containing matches".
    EXPECT_TRUE(pt::is_username_denylisted("xXanusXx"));
    EXPECT_TRUE(pt::is_username_denylisted("my_butt_hurts"));
}

TEST(IsUsernameDenylisted, CleanNameAllowed) {
    EXPECT_FALSE(pt::is_username_denylisted("RiverRat"));
    EXPECT_FALSE(pt::is_username_denylisted("pokerface99"));
}

TEST(IsUsernameDenylisted, EmptyAndAllSymbolNeverMatch) {
    EXPECT_FALSE(pt::is_username_denylisted(""));
    EXPECT_FALSE(pt::is_username_denylisted("___"));
}
