#include "backbone/modal_state.hpp"

#include "backbone/screen_state.hpp"

#include <gtest/gtest.h>

// Zone 11 — the real modal stack implementation (src/backbone/modal_state.cpp).
// Exercises the sealed contract through its public API: push/pop, depth, topmost,
// nested restore, and the tutorial lock.

namespace {

using namespace poker_trainer::backbone;

class ModalStateTest : public ::testing::Test {
protected:
    void SetUp() override {
        reset_modal_state_for_testing();
        reset_screen_state_for_testing();
    }
    void TearDown() override { reset_modal_state_for_testing(); }
};

TEST_F(ModalStateTest, EmptyStackReportsNoModal) {
    EXPECT_FALSE(is_any_modal_open());
    EXPECT_EQ(modal_stack_depth(), 0u);
    EXPECT_FALSE(current_modal_id().has_value());
}

TEST_F(ModalStateTest, OpenPushesAndReportsTopmost) {
    notify_modal_opened(ModalId{7});
    EXPECT_TRUE(is_any_modal_open());
    EXPECT_EQ(modal_stack_depth(), 1u);
    ASSERT_TRUE(current_modal_id().has_value());
    EXPECT_EQ(current_modal_id()->value, 7u);
}

TEST_F(ModalStateTest, NestedOpenReportsNewTopmostThenRestoresOnPop) {
    notify_modal_opened(ModalId{2});
    notify_modal_opened(ModalId{5});
    EXPECT_EQ(modal_stack_depth(), 2u);
    ASSERT_TRUE(current_modal_id().has_value());
    EXPECT_EQ(current_modal_id()->value, 5u);

    notify_modal_closed(ModalId{5});
    EXPECT_EQ(modal_stack_depth(), 1u);
    ASSERT_TRUE(current_modal_id().has_value());
    EXPECT_EQ(current_modal_id()->value, 2u);  // the previous topmost is restored
}

TEST_F(ModalStateTest, CloseToEmptyClearsTopmost) {
    notify_modal_opened(ModalId{3});
    notify_modal_closed(ModalId{3});
    EXPECT_FALSE(is_any_modal_open());
    EXPECT_EQ(modal_stack_depth(), 0u);
    EXPECT_FALSE(current_modal_id().has_value());
}

TEST_F(ModalStateTest, CloseOnEmptyStackIsNoOp) {
    notify_modal_closed(ModalId{9});
    EXPECT_EQ(modal_stack_depth(), 0u);
    EXPECT_FALSE(is_any_modal_open());
}

TEST_F(ModalStateTest, NotLockedWhileTutorialInactive) {
    EXPECT_FALSE(is_modal_locked());  // default screen state: tutorial Inactive
}

TEST_F(ModalStateTest, LockedWhileTutorialActive) {
    set_tutorial_state(TutorialState{TutorialPhase::Active, 1});
    EXPECT_TRUE(is_modal_locked());
    set_tutorial_state(TutorialState{TutorialPhase::Inactive, 0});
    EXPECT_FALSE(is_modal_locked());
}

}  // namespace
