#include "backbone/event_router.hpp"

#include <gtest/gtest.h>

namespace bb = poker_trainer::backbone;

namespace {

bb::KeyEvent key_down(bb::KeyCode code) {
    return bb::KeyEvent{bb::KeyEventType::KeyDown, code, bb::ModMask::None};
}

}  // namespace

class EventRouterTest : public ::testing::Test {
 protected:
    void SetUp() override { bb::reset_event_router_for_testing(); }
    void TearDown() override { bb::reset_event_router_for_testing(); }
};

TEST_F(EventRouterTest, TopmostPriorityHandlerConsumesFirst) {
    // Register lowest-to-highest to prove ordering is by priority, not insertion.
    int fired_priority = -1;
    const auto consume = [&fired_priority](int p) {
        return [&fired_priority, p](const bb::KeyEvent&) {
            fired_priority = p;
            return true;
        };
    };
    bb::register_key_handler({}, consume(3), bb::HandlerPriority::BackgroundCatchAll, "bg");
    bb::register_key_handler({}, consume(2), bb::HandlerPriority::ScreenContext, "screen");
    bb::register_key_handler({}, consume(1), bb::HandlerPriority::ModalLayer, "modal");
    bb::register_key_handler({}, consume(0), bb::HandlerPriority::TutorialOverlay, "tutorial");

    bb::dispatch_key_event(key_down(bb::KeyCode::Escape));
    EXPECT_EQ(fired_priority, 0);  // Tutorial overlay wins.
}

TEST_F(EventRouterTest, PriorityStackTutorialOutranksModalOutranksScreen) {
    int order = 0;
    int tutorial_order = -1;
    int modal_order = -1;
    int screen_order = -1;
    // Each handler passes through (returns false) so dispatch visits them in
    // priority order, recording the order it reached each.
    bb::register_key_handler({},
                             [&](const bb::KeyEvent&) { screen_order = order++; return false; },
                             bb::HandlerPriority::ScreenContext, "screen");
    bb::register_key_handler({},
                             [&](const bb::KeyEvent&) { modal_order = order++; return false; },
                             bb::HandlerPriority::ModalLayer, "modal");
    bb::register_key_handler({},
                             [&](const bb::KeyEvent&) { tutorial_order = order++; return false; },
                             bb::HandlerPriority::TutorialOverlay, "tutorial");

    bb::dispatch_key_event(key_down(bb::KeyCode::Enter));
    EXPECT_EQ(tutorial_order, 0);
    EXPECT_EQ(modal_order, 1);
    EXPECT_EQ(screen_order, 2);
}

TEST_F(EventRouterTest, NonConsumingHandlerFallsThrough) {
    bool top_ran = false;
    bool bottom_ran = false;
    // Top handler does NOT consume (returns false): the next one runs.
    bb::register_key_handler({},
                             [&](const bb::KeyEvent&) { top_ran = true; return false; },
                             bb::HandlerPriority::ModalLayer, "top");
    bb::register_key_handler({},
                             [&](const bb::KeyEvent&) { bottom_ran = true; return true; },
                             bb::HandlerPriority::ScreenContext, "bottom");
    bb::dispatch_key_event(key_down(bb::KeyCode::Enter));
    EXPECT_TRUE(top_ran);
    EXPECT_TRUE(bottom_ran);
}

TEST_F(EventRouterTest, ContextPredicateGatesEligibility) {
    bool gated_ran = false;
    bool fallthrough_ran = false;
    // Higher-priority handler is gated out by a false context predicate; the
    // lower-priority but eligible handler runs instead.
    bb::register_key_handler([] { return false; },
                             [&](const bb::KeyEvent&) { gated_ran = true; return true; },
                             bb::HandlerPriority::ModalLayer, "gated");
    bb::register_key_handler([] { return true; },
                             [&](const bb::KeyEvent&) { fallthrough_ran = true; return true; },
                             bb::HandlerPriority::ScreenContext, "eligible");
    bb::dispatch_key_event(key_down(bb::KeyCode::Enter));
    EXPECT_FALSE(gated_ran);
    EXPECT_TRUE(fallthrough_ran);
}

TEST_F(EventRouterTest, UninstalledHandlerNoLongerFires) {
    bool ran = false;
    const auto handle = bb::register_key_handler(
        {}, [&](const bb::KeyEvent&) { ran = true; return true; },
        bb::HandlerPriority::ScreenContext, "h");
    bb::uninstall_handler(handle);
    bb::dispatch_key_event(key_down(bb::KeyCode::Enter));
    EXPECT_FALSE(ran);
}

TEST_F(EventRouterTest, MouseAndKeyHandlersAreIndependent) {
    bool key_ran = false;
    bool mouse_ran = false;
    bb::register_key_handler({},
                             [&](const bb::KeyEvent&) { key_ran = true; return true; },
                             bb::HandlerPriority::ScreenContext, "k");
    bb::register_mouse_handler({},
                              [&](const bb::MouseEvent&) { mouse_ran = true; return true; },
                              bb::HandlerPriority::ScreenContext, "m");
    bb::dispatch_key_event(key_down(bb::KeyCode::Enter));
    EXPECT_TRUE(key_ran);
    EXPECT_FALSE(mouse_ran);

    key_ran = false;
    bb::dispatch_mouse_event(
        bb::MouseEvent{bb::MouseEventType::MouseDown, 10.0f, 20.0f, 0, 0.0f});
    EXPECT_FALSE(key_ran);
    EXPECT_TRUE(mouse_ran);
}
