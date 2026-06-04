// Zone 09 — numeric filter, per-branch input spawning, focus segment, parsing.

#include "math/input_boxes.hpp"
#include "math/bet_size_buttons.hpp"
#include "math/interrogator.hpp"

#include "backbone/focus_manager.hpp"
#include "engine/scenario.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

namespace it = poker_trainer::interrogator;
namespace eng = poker_trainer::engine;
namespace bb = poker_trainer::backbone;

namespace {

eng::ScenarioState caller_state() {
    eng::ScenarioState s{};
    s.type = eng::ScenarioType::Caller;
    return s;
}

eng::ScenarioState aggressor_state(eng::ScenarioType type, bool multi_tier,
                                   eng::BetTier presented = eng::BetTier::HalfPot) {
    eng::ScenarioState s{};
    s.type = type;
    s.multi_tier = multi_tier;
    s.presented_tier = presented;
    return s;
}

it::NumericBox box_with(eng::InputId id, bool allow_decimal, bool allow_minus, const char* text) {
    it::NumericBox b{};
    b.input = id;
    b.allow_decimal = allow_decimal;
    b.allow_minus = allow_minus;
    std::strncpy(b.text.data(), text, b.text.size() - 1);
    return b;
}

std::vector<eng::InputId> input_ids(const std::vector<it::NumericBox>& boxes) {
    std::vector<eng::InputId> ids;
    ids.reserve(boxes.size());
    for (const it::NumericBox& b : boxes) {
        ids.push_back(b.input);
    }
    return ids;
}

std::size_t count_input(const std::vector<it::NumericBox>& boxes, eng::InputId id) {
    std::size_t n = 0;
    for (const it::NumericBox& b : boxes) {
        if (b.input == id) {
            ++n;
        }
    }
    return n;
}

}  // namespace

// ----- Numeric keystroke filter -----

TEST(NumericFilter, DigitsAlwaysAccepted) {
    for (char c = '0'; c <= '9'; ++c) {
        EXPECT_TRUE(it::accepts_numeric_char("", c, /*decimal=*/false, /*minus=*/false));
        EXPECT_TRUE(it::accepts_numeric_char("12", c, true, true));
    }
}

TEST(NumericFilter, SingleDecimalPointOnlyWhenAllowed) {
    EXPECT_TRUE(it::accepts_numeric_char("12", '.', /*decimal=*/true, false));
    EXPECT_FALSE(it::accepts_numeric_char("12.5", '.', /*decimal=*/true, false));  // second point
    EXPECT_FALSE(it::accepts_numeric_char("12", '.', /*decimal=*/false, false));   // Outs: integer
}

TEST(NumericFilter, LeadingMinusOnlyWhenAllowedAndEmpty) {
    EXPECT_TRUE(it::accepts_numeric_char("", '-', false, /*minus=*/true));    // EV, leading
    EXPECT_FALSE(it::accepts_numeric_char("3", '-', false, /*minus=*/true));  // not leading
    EXPECT_FALSE(it::accepts_numeric_char("", '-', false, /*minus=*/false));  // probability box
}

TEST(NumericFilter, RejectsPercentCommaAndLetters) {
    EXPECT_FALSE(it::accepts_numeric_char("30", '%', true, true));
    EXPECT_FALSE(it::accepts_numeric_char("1", ',', true, true));
    EXPECT_FALSE(it::accepts_numeric_char("", 'e', true, true));
    EXPECT_FALSE(it::accepts_numeric_char("", ' ', true, true));
}

// ----- Per-branch input spawning -----

TEST(InputSpawn, CallerSpawnsPotOddsOutsEquityEv) {
    const std::vector<it::NumericBox> boxes = it::build_boxes(caller_state());
    EXPECT_EQ(input_ids(boxes),
              (std::vector<eng::InputId>{eng::InputId::PotOdds, eng::InputId::Outs,
                                         eng::InputId::Equity, eng::InputId::Ev}));
    EXPECT_FALSE(it::bet_group_present(caller_state()));
    for (const it::NumericBox& b : boxes) {
        EXPECT_FALSE(b.tier.has_value());
    }
}

TEST(InputSpawn, AggressorPureBluffSingleTierSpawnsFoldEv) {
    const eng::ScenarioState s =
        aggressor_state(eng::ScenarioType::AggressorPureBluff, /*multi_tier=*/false);
    const std::vector<it::NumericBox> boxes = it::build_boxes(s);
    EXPECT_EQ(input_ids(boxes),
              (std::vector<eng::InputId>{eng::InputId::FoldProbability, eng::InputId::Ev}));
    EXPECT_TRUE(it::bet_group_present(s));
    // Fold/EV carry the presented tier.
    EXPECT_EQ(boxes[0].tier, std::optional<std::uint8_t>{1});
    EXPECT_EQ(boxes[1].tier, std::optional<std::uint8_t>{1});
}

TEST(InputSpawn, AggressorValueBetSingleTierSpawnsFoldEv) {
    const eng::ScenarioState s =
        aggressor_state(eng::ScenarioType::AggressorValueBet, /*multi_tier=*/false);
    EXPECT_EQ(input_ids(it::build_boxes(s)),
              (std::vector<eng::InputId>{eng::InputId::FoldProbability, eng::InputId::Ev}));
}

TEST(InputSpawn, AggressorSemiBluffSingleTierSpawnsFoldEquityEv) {
    const eng::ScenarioState s =
        aggressor_state(eng::ScenarioType::AggressorSemiBluff, /*multi_tier=*/false);
    EXPECT_EQ(input_ids(it::build_boxes(s)),
              (std::vector<eng::InputId>{eng::InputId::FoldProbability, eng::InputId::Equity,
                                         eng::InputId::Ev}));
}

TEST(InputSpawn, AggressorMultiTierRepeatsFoldEvPerTier) {
    const eng::ScenarioState s =
        aggressor_state(eng::ScenarioType::AggressorPureBluff, /*multi_tier=*/true);
    const std::vector<it::NumericBox> boxes = it::build_boxes(s);
    EXPECT_EQ(boxes.size(), 2u * eng::kBetTierCount);  // Fold+EV per tier
    EXPECT_EQ(count_input(boxes, eng::InputId::FoldProbability), eng::kBetTierCount);
    EXPECT_EQ(count_input(boxes, eng::InputId::Ev), eng::kBetTierCount);
    // Each per-tier box is tagged with its tier 0..3 in order.
    for (std::uint8_t t = 0; t < eng::kBetTierCount; ++t) {
        EXPECT_EQ(boxes[2u * t].input, eng::InputId::FoldProbability);
        EXPECT_EQ(boxes[2u * t].tier, std::optional<std::uint8_t>{t});
        EXPECT_EQ(boxes[2u * t + 1u].input, eng::InputId::Ev);
        EXPECT_EQ(boxes[2u * t + 1u].tier, std::optional<std::uint8_t>{t});
    }
}

TEST(InputSpawn, MultiTierSemiBluffSpawnsEquityIfCalledExactlyOnce) {
    const eng::ScenarioState s =
        aggressor_state(eng::ScenarioType::AggressorSemiBluff, /*multi_tier=*/true);
    const std::vector<it::NumericBox> boxes = it::build_boxes(s);
    // The bet-size-independent Equity-if-Called locks after tier 1: it appears
    // once, not per tier.
    EXPECT_EQ(count_input(boxes, eng::InputId::Equity), 1u);
    EXPECT_EQ(count_input(boxes, eng::InputId::FoldProbability), eng::kBetTierCount);
    EXPECT_EQ(count_input(boxes, eng::InputId::Ev), eng::kBetTierCount);
    // The single Equity box carries no tier index.
    for (const it::NumericBox& b : boxes) {
        if (b.input == eng::InputId::Equity) {
            EXPECT_FALSE(b.tier.has_value());
        }
    }
}

// ----- Focus segment ordering -----

TEST(FocusSegment, CallerSegmentIsTheFourBoxesNoBetGroup) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, caller_state());
    EXPECT_EQ(state.focus_segment.size(), 4u);
    EXPECT_EQ(state.focus_segment.front(), it::kFocusPotOdds);
    EXPECT_FALSE(state.bet_group.present);
}

TEST(FocusSegment, AggressorSegmentEndsWithBetGroup) {
    it::InterrogatorState state{};
    it::configure_for_scenario(
        state, aggressor_state(eng::ScenarioType::AggressorPureBluff, /*multi_tier=*/false));
    ASSERT_FALSE(state.focus_segment.empty());
    EXPECT_TRUE(state.bet_group.present);
    EXPECT_EQ(state.focus_segment.back(), it::kFocusBetSizeGroup);
}

TEST(FocusSegment, RegisteredListWalksMathThenBetGroup) {
    bb::reset_focus_manager_for_testing();
    bb::activate_keyboard_mode();

    it::InterrogatorState state{};
    it::configure_for_scenario(
        state, aggressor_state(eng::ScenarioType::AggressorSemiBluff, /*multi_tier=*/false));
    bb::register_focus_list(bb::ScreenId::Game, state.focus_segment);

    // Walk forward across the whole segment and confirm it ends at the bet group
    // then wraps to the first math input.
    bb::snap_focus_to(state.focus_segment.front());
    for (std::size_t i = 1; i < state.focus_segment.size(); ++i) {
        bb::advance_focus(/*reverse=*/false);
        EXPECT_EQ(bb::get_focused_element(), state.focus_segment[i]);
    }
    EXPECT_EQ(bb::get_focused_element(), it::kFocusBetSizeGroup);
    bb::advance_focus(/*reverse=*/false);  // wrap
    EXPECT_EQ(bb::get_focused_element(), state.focus_segment.front());

    bb::reset_focus_manager_for_testing();
}

// ----- Buffer parsing -----

TEST(BufferParse, DoubleParsesAndRejectsIncomplete) {
    EXPECT_FALSE(it::parse_box_double(box_with(eng::InputId::Ev, true, true, "")).has_value());
    EXPECT_FALSE(it::parse_box_double(box_with(eng::InputId::Ev, true, true, "-")).has_value());
    EXPECT_FALSE(it::parse_box_double(box_with(eng::InputId::Ev, true, true, ".")).has_value());
    EXPECT_FALSE(it::parse_box_double(box_with(eng::InputId::Ev, true, true, "-.")).has_value());
    EXPECT_EQ(it::parse_box_double(box_with(eng::InputId::PotOdds, true, false, "30")), 30.0);
    EXPECT_EQ(it::parse_box_double(box_with(eng::InputId::Ev, true, true, "-12.5")), -12.5);
    EXPECT_EQ(it::parse_box_double(box_with(eng::InputId::Equity, true, false, ".5")), 0.5);
}

TEST(BufferParse, IntParsesDigitsOnly) {
    EXPECT_FALSE(it::parse_box_int(box_with(eng::InputId::Outs, false, false, "")).has_value());
    EXPECT_EQ(it::parse_box_int(box_with(eng::InputId::Outs, false, false, "8")), 8);
    EXPECT_EQ(it::parse_box_int(box_with(eng::InputId::Outs, false, false, "012")), 12);
}

// ----- ImGui keyboard-focus reconciliation (pure decision) -----

namespace {

it::InterrogatorState aggressor_interrogator_state() {
    it::InterrogatorState state{};
    it::configure_for_scenario(
        state, aggressor_state(eng::ScenarioType::AggressorPureBluff, /*multi_tier=*/false));
    return state;  // boxes: Fold(t1), EV(t1); bet group present
}

}  // namespace

TEST(FocusReconcile, NavToTextBoxRequestsImGuiKeyboardFocus) {
    const it::InterrogatorState state = aggressor_interrogator_state();
    const bb::FocusableId box0 = state.boxes.front().focus_id;
    // Outline moved onto a numeric box this frame -> steer ImGui's text focus there.
    const it::FocusReconcile r = it::reconcile_imgui_focus(state, bb::kNoFocus, box0);
    EXPECT_EQ(r.action, it::ImGuiFocusAction::FocusTextBox);
    EXPECT_EQ(r.target, box0);
}

TEST(FocusReconcile, NavToBetGroupYieldsKeyboardCapture) {
    const it::InterrogatorState state = aggressor_interrogator_state();
    const bb::FocusableId box0 = state.boxes.front().focus_id;
    // Outline moved onto the bet group (a non-text stop) -> release any active box
    // so WantCaptureKeyboard goes false and digits 1-4 select buttons.
    const it::FocusReconcile r = it::reconcile_imgui_focus(state, box0, it::kFocusBetSizeGroup);
    EXPECT_EQ(r.action, it::ImGuiFocusAction::YieldKeyboard);
}

TEST(FocusReconcile, BetGroupYieldsEveryFrameNotJustOnChange) {
    // Bug B: a pending SetKeyboardFocusHere from the box we left re-activates that
    // box AFTER a one-shot ClearActiveID, leaving it the active InputText forever
    // (digits then type into it). The yield must therefore fire EVERY frame the
    // group is focused -- including when focus is unchanged from the prior frame --
    // so the re-activated box is released on the next frame. (The earlier
    // change-gated behavior, returning None here, was the root cause and is wrong.)
    const it::InterrogatorState state = aggressor_interrogator_state();
    EXPECT_EQ(
        it::reconcile_imgui_focus(state, it::kFocusBetSizeGroup, it::kFocusBetSizeGroup).action,
        it::ImGuiFocusAction::YieldKeyboard);
}

TEST(FocusReconcile, UnchangedTextBoxFocusIsNoOp) {
    const it::InterrogatorState state = aggressor_interrogator_state();
    const bb::FocusableId box0 = state.boxes.front().focus_id;
    // Same numeric box both frames -> never re-grab (would trap the text caret).
    EXPECT_EQ(it::reconcile_imgui_focus(state, box0, box0).action, it::ImGuiFocusAction::None);
}

TEST(FocusReconcile, FocusOnNonZoneElementLeavesImGuiAlone) {
    const it::InterrogatorState state = aggressor_interrogator_state();
    // kFocusPotOdds is a Caller box, absent here -- a stand-in for a future
    // Z08/Z11 cluster stop Z09 does not own. Z09 must not touch ImGui's state.
    const it::FocusReconcile r =
        it::reconcile_imgui_focus(state, it::kFocusBetSizeGroup, it::kFocusPotOdds);
    EXPECT_EQ(r.action, it::ImGuiFocusAction::None);
}

// ----- Mouse-click -> focus_manager coupling -----

TEST(FocusClick, ClickOnBoxMovesOutlineToIt) {
    bb::reset_focus_manager_for_testing();
    it::InterrogatorState state = aggressor_interrogator_state();
    bb::register_focus_list(bb::ScreenId::Game, state.focus_segment);
    bb::snap_focus_to(it::kFocusBetSizeGroup);  // outline elsewhere first

    it::focus_box_on_click(state.boxes.front());

    EXPECT_TRUE(bb::is_keyboard_mode_active());
    EXPECT_EQ(bb::get_focused_element(), state.boxes.front().focus_id);
    bb::reset_focus_manager_for_testing();
}

TEST(FocusClick, ClickOnBetButtonSelectsTierAndFocusesGroup) {
    bb::reset_focus_manager_for_testing();
    it::InterrogatorState state = aggressor_interrogator_state();
    bb::register_focus_list(bb::ScreenId::Game, state.focus_segment);
    bb::snap_focus_to(state.boxes.front().focus_id);  // outline on a box first

    it::select_bet_tier_on_click(state.bet_group, eng::BetTier::FullPot);

    EXPECT_EQ(state.bet_group.selected, eng::BetTier::FullPot);
    EXPECT_TRUE(bb::is_keyboard_mode_active());
    EXPECT_EQ(bb::get_focused_element(), it::kFocusBetSizeGroup);
    bb::reset_focus_manager_for_testing();
}
