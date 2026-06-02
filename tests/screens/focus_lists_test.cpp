// Zone 07 — focus-list registration + navigation unit tests.
//
// Verified behaviorally against the real focus_manager: registering each Zone 07
// list and walking it with advance_focus must reproduce the exact spec ordering,
// wrap correctly, and (for the Custom popup) push/pop as a modal focus trap.

#include "screens/custom_popup.hpp"
#include "screens/mode_selection_screen.hpp"
#include "screens/root_screen.hpp"

#include "backbone/focus_manager.hpp"

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace sc = poker_trainer::screens;
namespace bb = poker_trainer::backbone;

namespace {

std::uint64_t focused() { return bb::get_focused_element().value; }

// Snap to `first`, then advance forward `count-1` times, collecting the id at
// each stop.
std::vector<std::uint64_t> walk_forward(bb::FocusableId first, std::size_t count) {
    bb::snap_focus_to(first);
    std::vector<std::uint64_t> seen;
    seen.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        seen.push_back(focused());
        bb::advance_focus(/*reverse=*/false);
    }
    return seen;
}

class FocusListTest : public ::testing::Test {
protected:
    void SetUp() override {
        bb::reset_focus_manager_for_testing();
        bb::activate_keyboard_mode();  // otherwise get_focused_element returns kNoFocus
    }
    void TearDown() override { bb::reset_focus_manager_for_testing(); }
};

}  // namespace

TEST_F(FocusListTest, RootOrderAndWrap) {
    sc::register_root_focus_list();
    const std::vector<std::uint64_t> want{
        sc::kFocusRootPlay.value, sc::kFocusRootSettings.value, sc::kFocusRootShop.value,
        sc::kFocusRootHelp.value, sc::kFocusRootHome.value,
    };
    EXPECT_EQ(walk_forward(sc::kFocusRootPlay, want.size()), want);

    // One more advance wraps Home -> Play.
    EXPECT_EQ(focused(), sc::kFocusRootPlay.value);
    // Reverse from Play wraps back to Home.
    bb::advance_focus(/*reverse=*/true);
    EXPECT_EQ(focused(), sc::kFocusRootHome.value);
}

TEST_F(FocusListTest, ModeSelectionOrderAndWrap) {
    sc::register_mode_selection_focus_list();
    const std::vector<std::uint64_t> want{
        sc::kFocusStandard.value,   sc::kFocusAggressorButton.value, sc::kFocusCallerButton.value,
        sc::kFocusCustomButton.value, sc::kFocusModeShop.value,      sc::kFocusModeHelp.value,
        sc::kFocusModeSettings.value, sc::kFocusModeHome.value,
    };
    EXPECT_EQ(walk_forward(sc::kFocusStandard, want.size()), want);
    EXPECT_EQ(focused(), sc::kFocusStandard.value);  // wrapped Home -> STANDARD
}

TEST_F(FocusListTest, SnapMovesPointer) {
    sc::register_root_focus_list();
    bb::snap_focus_to(sc::kFocusRootHelp);
    EXPECT_EQ(focused(), sc::kFocusRootHelp.value);
    bb::snap_focus_to(sc::kFocusRootShop);
    EXPECT_EQ(focused(), sc::kFocusRootShop.value);
}

TEST_F(FocusListTest, CustomPopupTrapsThenRestores) {
    sc::register_mode_selection_focus_list();
    bb::snap_focus_to(sc::kFocusStandard);
    EXPECT_EQ(bb::context_depth(), 0u);

    sc::push_custom_popup_focus();
    EXPECT_EQ(bb::context_depth(), 1u);
    EXPECT_EQ(focused(), sc::kFocusAggressorInput.value);  // default pointer 0

    const std::vector<std::uint64_t> want{
        sc::kFocusAggressorInput.value, sc::kFocusAggressorSlider.value,
        sc::kFocusCallerInput.value,    sc::kFocusCallerSlider.value,
        sc::kFocusSave.value,           sc::kFocusReset.value,
        sc::kFocusPlay.value,           sc::kFocusClose.value,
    };
    EXPECT_EQ(walk_forward(sc::kFocusAggressorInput, want.size()), want);
    EXPECT_EQ(focused(), sc::kFocusAggressorInput.value);  // wrapped X -> Aggressor input

    sc::pop_custom_popup_focus();
    EXPECT_EQ(bb::context_depth(), 0u);
    EXPECT_EQ(focused(), sc::kFocusStandard.value);  // restored
}

TEST_F(FocusListTest, CloseCustomPopupClearsOpenAndPopsFocus) {
    sc::register_mode_selection_focus_list();
    bb::snap_focus_to(sc::kFocusStandard);

    sc::CustomPopupState state;
    EXPECT_FALSE(state.just_opened);  // opening-frame guard defaults clear (B1)
    state.open = true;
    state.just_opened = true;
    sc::push_custom_popup_focus();
    EXPECT_EQ(bb::context_depth(), 1u);

    sc::close_custom_popup(state);  // X / click-outside / Escape dismissal path
    EXPECT_FALSE(state.open);
    EXPECT_FALSE(state.just_opened);  // guard cleared on close so a reopen re-arms (B1)
    EXPECT_EQ(bb::context_depth(), 0u);
    EXPECT_EQ(focused(), sc::kFocusStandard.value);  // focus restored to the screen
}
