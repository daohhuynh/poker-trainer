#include "bridge/canvas_sizing.hpp"

#include <gtest/gtest.h>

namespace br = poker_trainer::bridge;

TEST(CanvasDims, ViewportPassesThroughUnchanged) {
    EXPECT_EQ(br::canvas_dims_from_viewport(1920, 1080),
              (br::CanvasDims{1920, 1080}));
    EXPECT_EQ(br::canvas_dims_from_viewport(800, 600),
              (br::CanvasDims{800, 600}));
    EXPECT_EQ(br::canvas_dims_from_viewport(3840, 2160),
              (br::CanvasDims{3840, 2160}));
}

TEST(CanvasDims, NegativeViewportClampsToZero) {
    EXPECT_EQ(br::canvas_dims_from_viewport(-10, -5), (br::CanvasDims{0, 0}));
    EXPECT_EQ(br::canvas_dims_from_viewport(1024, -1), (br::CanvasDims{1024, 0}));
}

TEST(MinSizeThreshold, BooleanAtTheBoundary) {
    // Threshold is 600: below is too small, at-or-above is fine.
    EXPECT_TRUE(br::is_below_min_size(599));
    EXPECT_FALSE(br::is_below_min_size(600));
    EXPECT_FALSE(br::is_below_min_size(601));
    EXPECT_TRUE(br::is_below_min_size(0));
    EXPECT_FALSE(br::is_below_min_size(1080));
}

TEST(MobileUserAgent, DetectsMobileBrowsers) {
    // iOS Safari (iPhone).
    EXPECT_TRUE(br::is_mobile_user_agent(
        "Mozilla/5.0 (iPhone; CPU iPhone OS 16_0 like Mac OS X) "
        "AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.0 Mobile/15E148 "
        "Safari/604.1"));
    // Android Chrome.
    EXPECT_TRUE(br::is_mobile_user_agent(
        "Mozilla/5.0 (Linux; Android 13; Pixel 7) AppleWebKit/537.36 (KHTML, "
        "like Gecko) Chrome/119.0.0.0 Mobile Safari/537.36"));
    // iPad (pre-iPadOS-13 UA).
    EXPECT_TRUE(br::is_mobile_user_agent(
        "Mozilla/5.0 (iPad; CPU OS 15_0 like Mac OS X) AppleWebKit/605.1.15 "
        "(KHTML, like Gecko) Version/15.0 Mobile/15E148 Safari/604.1"));
}

TEST(MobileUserAgent, RejectsDesktopBrowsers) {
    // Desktop Chrome on Windows.
    EXPECT_FALSE(br::is_mobile_user_agent(
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, "
        "like Gecko) Chrome/119.0.0.0 Safari/537.36"));
    // Desktop Firefox on Linux.
    EXPECT_FALSE(br::is_mobile_user_agent(
        "Mozilla/5.0 (X11; Linux x86_64; rv:120.0) Gecko/20100101 "
        "Firefox/120.0"));
    // Desktop Safari on macOS.
    EXPECT_FALSE(br::is_mobile_user_agent(
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 "
        "(KHTML, like Gecko) Version/17.0 Safari/605.1.15"));
    EXPECT_FALSE(br::is_mobile_user_agent(""));
}

TEST(DisplayMode, MobileTakesPrecedenceOverSize) {
    // A mobile device is always the mobile fallback, even at a large height.
    EXPECT_EQ(br::resolve_display_mode(/*is_mobile=*/true, 1080),
              br::DisplayMode::Mobile);
    EXPECT_EQ(br::resolve_display_mode(/*is_mobile=*/true, 100),
              br::DisplayMode::Mobile);
}

TEST(DisplayMode, NonMobileChecksSizeThreshold) {
    EXPECT_EQ(br::resolve_display_mode(/*is_mobile=*/false, 1080),
              br::DisplayMode::Normal);
    EXPECT_EQ(br::resolve_display_mode(/*is_mobile=*/false, 600),
              br::DisplayMode::Normal);
    EXPECT_EQ(br::resolve_display_mode(/*is_mobile=*/false, 599),
              br::DisplayMode::TooSmall);
}
