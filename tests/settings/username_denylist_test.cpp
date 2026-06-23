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

// is_username_denylisted: substring match against the (placeholder) compiled list.

TEST(IsUsernameDenylisted, PlainMatchRejected) {
    EXPECT_TRUE(pt::is_username_denylisted("badword"));
    EXPECT_TRUE(pt::is_username_denylisted("denyme"));
}

TEST(IsUsernameDenylisted, CaseInsensitiveMatchRejected) {
    EXPECT_TRUE(pt::is_username_denylisted("BADWORD"));
    EXPECT_TRUE(pt::is_username_denylisted("BadWord"));
}

TEST(IsUsernameDenylisted, LeetspeakMatchRejected) {
    EXPECT_TRUE(pt::is_username_denylisted("b4dw0rd"));
    EXPECT_TRUE(pt::is_username_denylisted("B4D_W0RD"));
}

TEST(IsUsernameDenylisted, SubstringMatchRejected) {
    // The spec rejects names "containing matches".
    EXPECT_TRUE(pt::is_username_denylisted("xXbadwordXx"));
    EXPECT_TRUE(pt::is_username_denylisted("totally_denyme_please"));
}

TEST(IsUsernameDenylisted, CleanNameAllowed) {
    EXPECT_FALSE(pt::is_username_denylisted("RiverRat"));
    EXPECT_FALSE(pt::is_username_denylisted("pokerface99"));
}

TEST(IsUsernameDenylisted, EmptyAndAllSymbolNeverMatch) {
    EXPECT_FALSE(pt::is_username_denylisted(""));
    EXPECT_FALSE(pt::is_username_denylisted("___"));
}
