#include "screens/again_button.hpp"

#include <gtest/gtest.h>

namespace pt = poker_trainer::screens;

// Default press arms; the screen stays on Post-Round.
TEST(AgainButton, FirstPressArms) {
    pt::AgainButton b{};
    EXPECT_EQ(b.state, pt::AgainState::Default);
    EXPECT_EQ(pt::press_again(b), pt::AgainPressOutcome::Armed);
    EXPECT_EQ(b.state, pt::AgainState::Armed);
}

// Second press while armed commits.
TEST(AgainButton, SecondPressCommits) {
    pt::AgainButton b{};
    (void)pt::press_again(b);
    EXPECT_EQ(pt::press_again(b), pt::AgainPressOutcome::Committed);
}

// After a commit the machine returns to Default so a re-render never shows a stale
// "CONFIRM".
TEST(AgainButton, CommitResetsToDefault) {
    pt::AgainButton b{};
    (void)pt::press_again(b);
    (void)pt::press_again(b);
    EXPECT_EQ(b.state, pt::AgainState::Default);
}

// The label reflects the visible state.
TEST(AgainButton, LabelReflectsState) {
    EXPECT_STREQ(pt::again_label(pt::AgainState::Default), "AGAIN");
    EXPECT_STREQ(pt::again_label(pt::AgainState::Armed), "CONFIRM");
}

// A fresh Post-Round entry resets to Default regardless of prior arming — and the
// reset path is what makes Exit independent of the armed state (Exit never reads or
// is blocked by AgainButton; a fresh recap simply re-disarms it).
TEST(AgainButton, ResetUnarms) {
    pt::AgainButton b{};
    (void)pt::press_again(b);
    ASSERT_EQ(b.state, pt::AgainState::Armed);
    pt::reset_again(b);
    EXPECT_EQ(b.state, pt::AgainState::Default);
}
