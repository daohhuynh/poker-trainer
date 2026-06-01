#include "bridge/screen_dispatch.hpp"

#include "backbone/screen_state.hpp"

#include <gtest/gtest.h>

namespace br = poker_trainer::bridge;
namespace bb = poker_trainer::backbone;

class ScreenDispatchTest : public ::testing::Test {
 protected:
    void SetUp() override { br::reset_screen_dispatch_for_testing(); }
    void TearDown() override { br::reset_screen_dispatch_for_testing(); }
};

TEST_F(ScreenDispatchTest, UnregisteredScreenReportsNoRenderer) {
    EXPECT_FALSE(br::has_screen_renderer(bb::ScreenId::Game));
    EXPECT_FALSE(br::render_screen(bb::ScreenId::Game));
}

TEST_F(ScreenDispatchTest, RegisteredRendererIsInvoked) {
    int calls = 0;
    br::register_screen_renderer(bb::ScreenId::Game, [&calls] { ++calls; });
    EXPECT_TRUE(br::has_screen_renderer(bb::ScreenId::Game));

    EXPECT_TRUE(br::render_screen(bb::ScreenId::Game));
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(br::render_screen(bb::ScreenId::Game));
    EXPECT_EQ(calls, 2);
}

TEST_F(ScreenDispatchTest, RenderersAreKeyedPerScreen) {
    int game_calls = 0;
    int root_calls = 0;
    br::register_screen_renderer(bb::ScreenId::Game, [&] { ++game_calls; });
    br::register_screen_renderer(bb::ScreenId::Root, [&] { ++root_calls; });

    br::render_screen(bb::ScreenId::Game);
    EXPECT_EQ(game_calls, 1);
    EXPECT_EQ(root_calls, 0);

    br::render_screen(bb::ScreenId::Root);
    EXPECT_EQ(game_calls, 1);
    EXPECT_EQ(root_calls, 1);

    // A screen with no registration still reports/returns false.
    EXPECT_FALSE(br::render_screen(bb::ScreenId::PostRound));
}

TEST_F(ScreenDispatchTest, NullRendererClearsRegistration) {
    br::register_screen_renderer(bb::ScreenId::Game, [] {});
    EXPECT_TRUE(br::has_screen_renderer(bb::ScreenId::Game));
    br::register_screen_renderer(bb::ScreenId::Game, nullptr);
    EXPECT_FALSE(br::has_screen_renderer(bb::ScreenId::Game));
}

TEST_F(ScreenDispatchTest, ResetClearsAll) {
    br::register_screen_renderer(bb::ScreenId::Game, [] {});
    br::register_screen_renderer(bb::ScreenId::Root, [] {});
    br::reset_screen_dispatch_for_testing();
    EXPECT_FALSE(br::has_screen_renderer(bb::ScreenId::Game));
    EXPECT_FALSE(br::has_screen_renderer(bb::ScreenId::Root));
}
