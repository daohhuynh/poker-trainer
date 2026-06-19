#include "modal/confirm_modal.hpp"

#include <gtest/gtest.h>

// Zone 11 — confirmation modal focus contract: the uniform No -> Yes -> X close
// list with No (the safe option) as the default.

namespace {

using namespace poker_trainer::modal;

TEST(ConfirmModal, FocusOrderIsNoThenYesThenClose) {
    ASSERT_EQ(kConfirmFocusOrder.size(), 3u);
    EXPECT_TRUE(kConfirmFocusOrder[0] == kConfirmNo);
    EXPECT_TRUE(kConfirmFocusOrder[1] == kConfirmYes);
    EXPECT_TRUE(kConfirmFocusOrder[2] == kConfirmClose);
}

TEST(ConfirmModal, DefaultFocusIsNo) {
    EXPECT_EQ(kConfirmDefaultFocusIndex, 0u);
    EXPECT_TRUE(kConfirmFocusOrder[kConfirmDefaultFocusIndex] == kConfirmNo);
}

TEST(ConfirmModal, FocusablesAreDistinct) {
    EXPECT_FALSE(kConfirmNo == kConfirmYes);
    EXPECT_FALSE(kConfirmYes == kConfirmClose);
    EXPECT_FALSE(kConfirmNo == kConfirmClose);
}

}  // namespace
