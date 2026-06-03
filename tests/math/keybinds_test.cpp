// Zone 09 — keybind routing: bet-tier selection, number-key focus mapping, and
// the installed event-router handlers (Enter-submits-all + tutorial override,
// 1-6 focus, bet-group 1-4 select with 1-6 suppression).

#include "math/keybinds.hpp"
#include "math/bet_size_buttons.hpp"
#include "math/input_boxes.hpp"
#include "math/interrogator.hpp"
#include "math/submission.hpp"

#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"
#include "backbone/scenario_events.hpp"
#include "backbone/screen_state.hpp"
#include "engine/scenario.hpp"

#include "bridge/screen_dispatch.hpp"

#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

namespace it = poker_trainer::interrogator;
namespace eng = poker_trainer::engine;
namespace bb = poker_trainer::backbone;
namespace bridge = poker_trainer::bridge;

namespace {

eng::ScenarioState caller_state() {
    eng::ScenarioState s{};
    s.id = eng::ScenarioId{1};
    s.type = eng::ScenarioType::Caller;
    return s;
}

eng::ScenarioState pure_bluff_state() {
    eng::ScenarioState s{};
    s.id = eng::ScenarioId{2};
    s.type = eng::ScenarioType::AggressorPureBluff;
    s.multi_tier = false;
    s.presented_tier = eng::BetTier::HalfPot;
    return s;
}

bb::KeyEvent key(bb::KeyCode code, bb::ModMask mods = bb::ModMask::None) {
    return bb::KeyEvent{bb::KeyEventType::KeyDown, code, mods};
}

void set_box(it::InterrogatorState& state, eng::InputId id, std::optional<std::uint8_t> tier,
             const char* text) {
    for (it::NumericBox& b : state.boxes) {
        if (b.input == id && b.tier == tier) {
            std::strncpy(b.text.data(), text, b.text.size() - 1);
            return;
        }
    }
    ADD_FAILURE() << "no box";
}

}  // namespace

// ----- Bet-tier selection by digit -----

TEST(BetSize, TierForDigitMapsVisualOrder) {
    EXPECT_EQ(it::bet_tier_for_digit(1), eng::BetTier::OneThirdPot);
    EXPECT_EQ(it::bet_tier_for_digit(2), eng::BetTier::HalfPot);
    EXPECT_EQ(it::bet_tier_for_digit(3), eng::BetTier::FullPot);
    EXPECT_EQ(it::bet_tier_for_digit(4), eng::BetTier::Overbet);
    EXPECT_FALSE(it::bet_tier_for_digit(0).has_value());
    EXPECT_FALSE(it::bet_tier_for_digit(5).has_value());
}

TEST(BetSize, SelectByDigitConsumesOneToFourOnly) {
    it::BetSizeGroup group{};
    group.present = true;
    EXPECT_TRUE(it::select_bet_tier_by_digit(group, 3));
    EXPECT_EQ(group.selected, eng::BetTier::FullPot);
    EXPECT_FALSE(it::select_bet_tier_by_digit(group, 6));  // out of range
    EXPECT_EQ(group.selected, eng::BetTier::FullPot);      // unchanged
}

// ----- Number-key -> focus mapping -----

TEST(NumberKeys, CallerMapping) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, caller_state());
    EXPECT_EQ(it::focus_target_for_digit(state, 1), it::kFocusPotOdds);
    EXPECT_EQ(it::focus_target_for_digit(state, 2), it::kFocusOuts);
    EXPECT_EQ(it::focus_target_for_digit(state, 3), it::kFocusEquity);
    EXPECT_EQ(it::focus_target_for_digit(state, 4), it::kFocusEvCaller);
    EXPECT_EQ(it::focus_target_for_digit(state, 5), bb::kNoFocus);  // no Fold box
    EXPECT_EQ(it::focus_target_for_digit(state, 6), bb::kNoFocus);  // no Bet Size group
}

TEST(NumberKeys, AggressorMapping) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, pure_bluff_state());
    EXPECT_EQ(it::focus_target_for_digit(state, 5), it::kFocusFoldTier[1]);  // presented tier
    EXPECT_EQ(it::focus_target_for_digit(state, 6), it::kFocusBetSizeGroup);
    EXPECT_EQ(it::focus_target_for_digit(state, 1), bb::kNoFocus);  // no Pot Odds box
}

// ----- Installed handlers, driven through the event router -----

class InstalledHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        bb::reset_event_router_for_testing();
        bb::reset_focus_manager_for_testing();
        bb::reset_screen_state_for_testing();
        bb::reset_scenario_events_for_testing();
        bridge::reset_screen_dispatch_for_testing();
        bb::activate_keyboard_mode();
        it::install_interrogator(runtime_);
    }
    void TearDown() override {
        bb::reset_event_router_for_testing();
        bb::reset_focus_manager_for_testing();
        bb::reset_screen_state_for_testing();
        bb::reset_scenario_events_for_testing();
        bridge::reset_screen_dispatch_for_testing();
    }

    // Put the runtime on the Game screen with `s` active + its focus list live.
    void enter_game(const eng::ScenarioState& s) {
        it::configure_for_scenario(runtime_.state, s);
        bb::register_focus_list(bb::ScreenId::Game, runtime_.state.focus_segment);
        bb::set_screen(bb::ScreenId::Game, s.id);
    }

    it::InterrogatorRuntime runtime_{};
};

TEST_F(InstalledHandlerTest, NumberKeyFocusesMappedBox) {
    enter_game(pure_bluff_state());
    bb::snap_focus_to(it::kFocusEvTier[1]);  // start on a non-bet input
    bb::dispatch_key_event(key(bb::KeyCode::Digit5));
    EXPECT_EQ(bb::get_focused_element(), it::kFocusFoldTier[1]);
}

TEST_F(InstalledHandlerTest, BetGroupFocusSuppressesGlobalKeysAndSelectsByDigit) {
    enter_game(pure_bluff_state());
    bb::snap_focus_to(it::kFocusBetSizeGroup);

    bb::dispatch_key_event(key(bb::KeyCode::Digit3));  // 3 -> Full Pot
    EXPECT_EQ(runtime_.state.bet_group.selected, eng::BetTier::FullPot);

    // While the group is focused, "5" (a global box-focus key) is suppressed:
    // focus stays on the group and the selection is unchanged.
    bb::dispatch_key_event(key(bb::KeyCode::Digit5));
    EXPECT_EQ(bb::get_focused_element(), it::kFocusBetSizeGroup);
    EXPECT_EQ(runtime_.state.bet_group.selected, eng::BetTier::FullPot);
}

TEST_F(InstalledHandlerTest, EnterSubmitsRegardlessOfFocusedElement) {
    bool fired = false;
    const bb::SubscriberHandle h = bb::subscribe_answers_submitted(
        [&](const bb::AnswersSubmittedEvent&) { fired = true; }, "test");

    enter_game(caller_state());
    bb::snap_focus_to(it::kFocusPotOdds);  // focus on an input, not a submit button
    bb::dispatch_key_event(key(bb::KeyCode::Enter));

    EXPECT_TRUE(fired);
    EXPECT_TRUE(runtime_.state.last_result.has_value());
    bb::unsubscribe(h);
}

TEST_F(InstalledHandlerTest, ScenarioSpawnedResetsAndRespawnsInputs) {
    enter_game(caller_state());
    set_box(runtime_.state, eng::InputId::PotOdds, std::nullopt, "99");  // stale entry
    runtime_.state.bet_group.selected = eng::BetTier::Overbet;

    // The bus subscription reconfigures from the new id and clears all buffers.
    bb::fire_scenario_spawned(bb::ScenarioSpawnedEvent{eng::ScenarioId{12345}});
    ASSERT_TRUE(runtime_.state.scenario.has_value());
    EXPECT_EQ(runtime_.state.scenario->id, eng::ScenarioId{12345});
    EXPECT_FALSE(runtime_.state.boxes.empty());
    for (const it::NumericBox& b : runtime_.state.boxes) {
        EXPECT_EQ(b.text[0], '\0');  // every buffer cleared by the respawn
    }
    EXPECT_FALSE(runtime_.state.bet_group.selected.has_value());
    EXPECT_FALSE(runtime_.state.last_result.has_value());
}

TEST_F(InstalledHandlerTest, TutorialSuppressesEnterUntilAllInputsFilled) {
    enter_game(caller_state());
    bb::set_tutorial_state(bb::TutorialState{bb::TutorialPhase::Active, 3});

    // Nothing filled yet: Enter is consumed but does not submit.
    bb::dispatch_key_event(key(bb::KeyCode::Enter));
    EXPECT_FALSE(runtime_.state.last_result.has_value());

    // Fill every visible box, then Enter submits normally.
    set_box(runtime_.state, eng::InputId::PotOdds, std::nullopt, "30");
    set_box(runtime_.state, eng::InputId::Outs, std::nullopt, "8");
    set_box(runtime_.state, eng::InputId::Equity, std::nullopt, "40");
    set_box(runtime_.state, eng::InputId::Ev, std::nullopt, "5");
    bb::dispatch_key_event(key(bb::KeyCode::Enter));
    EXPECT_TRUE(runtime_.state.last_result.has_value());
}
