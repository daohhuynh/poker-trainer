// Zone 05 — keyboard routing: the WantCaptureKeyboard gating decision (the
// keyboard sibling of the mouse gate) and the router's Enter dispatch to the
// focused element's handler (resolved via focus_manager.get_focused_element).

#include "bridge/input_routing.hpp"

#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"
#include "backbone/screen_state.hpp"

#include <array>

#include <gtest/gtest.h>

namespace br = poker_trainer::bridge;
namespace bb = poker_trainer::backbone;

namespace {

constexpr bb::FocusableId kA = bb::make_focusable_id("test.routing.a");
constexpr bb::FocusableId kB = bb::make_focusable_id("test.routing.b");

bb::KeyEvent down(bb::KeyCode code) {
    return bb::KeyEvent{bb::KeyEventType::KeyDown, code, bb::ModMask::None};
}

}  // namespace

// ----- WantCaptureKeyboard gating decision -----

TEST(KeyboardGate, RouterSeesEveryKeyWhenImGuiNotCapturing) {
    EXPECT_TRUE(br::router_should_see_key(false, bb::KeyCode::Digit1));
    EXPECT_TRUE(br::router_should_see_key(false, bb::KeyCode::Tab));
    EXPECT_TRUE(br::router_should_see_key(false, bb::KeyCode::Enter));
    EXPECT_TRUE(br::router_should_see_key(false, bb::KeyCode::Backspace));
    EXPECT_TRUE(br::router_should_see_key(false, bb::KeyCode::Escape));
}

TEST(KeyboardGate, TextKeysSuppressedWhileImGuiCaptures) {
    // Digits, editing keys, and arrows edit the active box; they are NOT also
    // dispatched as screen-level commands (no "1" re-focusing input box 1).
    EXPECT_FALSE(br::router_should_see_key(true, bb::KeyCode::Digit1));
    EXPECT_FALSE(br::router_should_see_key(true, bb::KeyCode::Digit5));
    EXPECT_FALSE(br::router_should_see_key(true, bb::KeyCode::Backspace));
    EXPECT_FALSE(br::router_should_see_key(true, bb::KeyCode::Delete));
    EXPECT_FALSE(br::router_should_see_key(true, bb::KeyCode::ArrowLeft));
}

TEST(KeyboardGate, NavigationKeysAlwaysRouteEvenWhileCapturing) {
    // Tab / Enter / Escape keep working mid-edit: focus nav, submit-all, dismiss.
    EXPECT_TRUE(br::router_should_see_key(true, bb::KeyCode::Tab));
    EXPECT_TRUE(br::router_should_see_key(true, bb::KeyCode::Enter));
    EXPECT_TRUE(br::router_should_see_key(true, bb::KeyCode::Escape));
}

// ----- Enter routes to the focused element's handler (via get_focused_element) -----

class FocusedEnterTest : public ::testing::Test {
protected:
    void SetUp() override {
        bb::reset_event_router_for_testing();
        bb::reset_focus_manager_for_testing();
        bb::activate_keyboard_mode();
    }
    void TearDown() override {
        bb::reset_event_router_for_testing();
        bb::reset_focus_manager_for_testing();
    }
};

TEST_F(FocusedEnterTest, EnterReachesOnlyTheFocusedElementsHandler) {
    const std::array<bb::FocusableId, 2> list{kA, kB};
    bb::register_focus_list(bb::ScreenId::Game, list);

    int a_fired = 0;
    int b_fired = 0;
    // Each element registers an Enter handler gated on being the focused element.
    // The router resolves the focused element via get_focused_element and the
    // matching handler is the one that runs — this is the backbone spec's "router
    // dispatches enter to the currently focused element's handler".
    (void)bb::register_key_handler(
        [] { return bb::get_focused_element() == kA; },
        [&](const bb::KeyEvent& e) {
            if (e.code == bb::KeyCode::Enter) {
                ++a_fired;
                return true;
            }
            return false;
        },
        bb::HandlerPriority::ScreenContext, "test.a.enter");
    (void)bb::register_key_handler(
        [] { return bb::get_focused_element() == kB; },
        [&](const bb::KeyEvent& e) {
            if (e.code == bb::KeyCode::Enter) {
                ++b_fired;
                return true;
            }
            return false;
        },
        bb::HandlerPriority::ScreenContext, "test.b.enter");

    bb::snap_focus_to(kA);
    bb::dispatch_key_event(down(bb::KeyCode::Enter));
    EXPECT_EQ(a_fired, 1);
    EXPECT_EQ(b_fired, 0);

    bb::snap_focus_to(kB);
    bb::dispatch_key_event(down(bb::KeyCode::Enter));
    EXPECT_EQ(a_fired, 1);
    EXPECT_EQ(b_fired, 1);
}
