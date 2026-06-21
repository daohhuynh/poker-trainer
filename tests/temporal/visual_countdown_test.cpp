#include "temporal/visual_countdown.hpp"

#include "theme/theme_tokens.hpp"

#include <cstdint>

#include <gtest/gtest.h>

namespace tt = poker_trainer::temporal;
namespace th = poker_trainer::theme;

namespace {

// ----- Undertime: ceil of remaining seconds, text_secondary -----

TEST(CountdownFormat, UndertimeShowsTargetSecondsAtSpawn) {
    const auto d = tt::format_countdown(0, 18000);
    EXPECT_EQ(d.text, "18s");
    EXPECT_EQ(d.color, th::ColorToken::TextSecondary);
}
TEST(CountdownFormat, UndertimeCeilsRemainingSeconds) {
    // 15.5s remaining -> ceil -> 16s.
    const auto d = tt::format_countdown(2500, 18000);
    EXPECT_EQ(d.text, "16s");
    EXPECT_EQ(d.color, th::ColorToken::TextSecondary);
}
TEST(CountdownFormat, UndertimeFinalWholeSecondIsOne) {
    EXPECT_EQ(tt::format_countdown(17000, 18000).text, "1s");  // exactly 1s remaining
    EXPECT_EQ(tt::format_countdown(17999, 18000).text, "1s");  // 1ms remaining still ceils to 1
}
TEST(CountdownFormat, JustOverOneSecondRemainingCeilsToTwo) {
    EXPECT_EQ(tt::format_countdown(16999, 18000).text, "2s");  // 1001ms remaining
}

// ----- Crossover + overtime: floor of elapsed-over seconds, state_fail -----

TEST(CountdownFormat, CrossoverIsZeroSecondsRed) {
    const auto d = tt::format_countdown(18000, 18000);  // elapsed == target
    EXPECT_EQ(d.text, "0s");
    EXPECT_EQ(d.color, th::ColorToken::StateFail);
}
TEST(CountdownFormat, OvertimeFloorsAndStaysRed) {
    EXPECT_EQ(tt::format_countdown(18999, 18000).text, "0s");  // 999ms over -> floor 0
    EXPECT_EQ(tt::format_countdown(19000, 18000).text, "1s");  // 1s over
    EXPECT_EQ(tt::format_countdown(20000, 18000).text, "2s");  // 2s over
    EXPECT_EQ(tt::format_countdown(20000, 18000).color, th::ColorToken::StateFail);
}

}  // namespace
