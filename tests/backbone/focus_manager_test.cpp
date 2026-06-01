#include "backbone/focus_manager.hpp"

#include "backbone/screen_state.hpp"

#include <gtest/gtest.h>

#include <array>

namespace bb = poker_trainer::backbone;

namespace {
constexpr bb::FocusableId kA = bb::make_focusable_id("test.a");
constexpr bb::FocusableId kB = bb::make_focusable_id("test.b");
constexpr bb::FocusableId kC = bb::make_focusable_id("test.c");
}  // namespace

class FocusManagerTest : public ::testing::Test {
 protected:
    void SetUp() override { bb::reset_focus_manager_for_testing(); }
    void TearDown() override { bb::reset_focus_manager_for_testing(); }
};

TEST_F(FocusManagerTest, NoFocusUntilKeyboardModeActive) {
    const std::array<bb::FocusableId, 3> list{kA, kB, kC};
    bb::register_focus_list(bb::ScreenId::Root, list);
    // Keyboard mode inactive -> the indicator is suppressed (kNoFocus) even
    // though a pointer exists internally.
    EXPECT_FALSE(bb::is_keyboard_mode_active());
    EXPECT_EQ(bb::get_focused_element(), bb::kNoFocus);

    bb::activate_keyboard_mode();
    EXPECT_TRUE(bb::is_keyboard_mode_active());
    EXPECT_EQ(bb::get_focused_element(), kA);  // pointer defaults to 0
}

TEST_F(FocusManagerTest, AdvanceWrapsForwardAndBackward) {
    const std::array<bb::FocusableId, 3> list{kA, kB, kC};
    bb::register_focus_list(bb::ScreenId::Root, list);
    bb::activate_keyboard_mode();

    EXPECT_EQ(bb::get_focused_element(), kA);
    bb::advance_focus(/*reverse=*/false);
    EXPECT_EQ(bb::get_focused_element(), kB);
    bb::advance_focus(false);
    EXPECT_EQ(bb::get_focused_element(), kC);
    bb::advance_focus(false);  // wraps to first
    EXPECT_EQ(bb::get_focused_element(), kA);

    bb::advance_focus(/*reverse=*/true);  // shift-tab wraps to last
    EXPECT_EQ(bb::get_focused_element(), kC);
    bb::advance_focus(true);
    EXPECT_EQ(bb::get_focused_element(), kB);
}

TEST_F(FocusManagerTest, SnapFocusSetsPointerAndActivatesKeyboardMode) {
    const std::array<bb::FocusableId, 3> list{kA, kB, kC};
    bb::register_focus_list(bb::ScreenId::Root, list);
    // snap_focus_to also activates keyboard mode (mouse-click path).
    EXPECT_FALSE(bb::is_keyboard_mode_active());
    bb::snap_focus_to(kC);
    EXPECT_TRUE(bb::is_keyboard_mode_active());
    EXPECT_EQ(bb::get_focused_element(), kC);
}

TEST_F(FocusManagerTest, ActivateKeyboardModeIsIdempotent) {
    bb::activate_keyboard_mode();
    bb::activate_keyboard_mode();
    bb::activate_keyboard_mode();
    EXPECT_TRUE(bb::is_keyboard_mode_active());
}

TEST_F(FocusManagerTest, PushAndPopContextRestoresPointer) {
    const std::array<bb::FocusableId, 3> base{kA, kB, kC};
    bb::register_focus_list(bb::ScreenId::Root, base);
    bb::activate_keyboard_mode();
    bb::snap_focus_to(kC);
    EXPECT_EQ(bb::context_depth(), 0u);

    constexpr bb::FocusableId kJ = bb::make_focusable_id("modal.j");
    constexpr bb::FocusableId kK = bb::make_focusable_id("modal.k");
    const std::array<bb::FocusableId, 2> modal{kJ, kK};
    bb::push_focus_context(modal, kJ, "modal");
    EXPECT_EQ(bb::context_depth(), 1u);
    EXPECT_EQ(bb::get_focused_element(), kJ);
    bb::advance_focus(false);
    EXPECT_EQ(bb::get_focused_element(), kK);

    // Pop restores the underlying screen context with its prior focus (kC).
    bb::pop_focus_context();
    EXPECT_EQ(bb::context_depth(), 0u);
    EXPECT_EQ(bb::get_focused_element(), kC);
}

TEST_F(FocusManagerTest, PopAtBaseContextIsNoOp) {
    const std::array<bb::FocusableId, 3> base{kA, kB, kC};
    bb::register_focus_list(bb::ScreenId::Root, base);
    bb::activate_keyboard_mode();
    bb::pop_focus_context();  // nothing pushed; must not crash or change state
    EXPECT_EQ(bb::context_depth(), 0u);
    EXPECT_EQ(bb::get_focused_element(), kA);
}
