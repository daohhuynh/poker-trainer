// Zone 09 — sequential multi-tier Aggressor flow (the PURE decision logic).
//
// These cover the tier state machine introduced for the sequential redesign:
// the gate (is_sequential), forward-only advance (next_tier), the per-tier visible
// set (current_view_boxes), the per-tier advance gate (current_tier_required_filled),
// and what Enter does (enter_action). They also assert the containment guarantee:
// the persistent Bet Size pick survives tier changes and the accumulated answer set
// gather_answers produces is the SAME all-at-once shape Z01 grades -- the flow
// changes, the submitted answers do not. No ImGui / focus / browser here: the live
// Enter/focus/ImGui wiring (advance_tier driving register_focus_list + the platform
// keyboard gate) is verified by the owner in-browser, not by a decision-function
// test.

#include "math/tier_flow.hpp"
#include "math/input_boxes.hpp"
#include "math/interrogator.hpp"
#include "math/submission.hpp"

#include "engine/scenario.hpp"

#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

namespace it = poker_trainer::interrogator;
namespace eng = poker_trainer::engine;

namespace {

eng::ScenarioState caller() {
    eng::ScenarioState s{};
    s.id = eng::ScenarioId{1};
    s.type = eng::ScenarioType::Caller;
    return s;
}

eng::ScenarioState single_tier_aggressor() {
    eng::ScenarioState s{};
    s.id = eng::ScenarioId{2};
    s.type = eng::ScenarioType::AggressorPureBluff;
    s.multi_tier = false;
    s.presented_tier = eng::BetTier::HalfPot;
    return s;
}

eng::ScenarioState multi_tier_pure_bluff() {
    eng::ScenarioState s{};
    s.id = eng::ScenarioId{3};
    s.type = eng::ScenarioType::AggressorPureBluff;
    s.multi_tier = true;
    return s;
}

eng::ScenarioState multi_tier_semi_bluff() {
    eng::ScenarioState s{};
    s.id = eng::ScenarioId{4};
    s.type = eng::ScenarioType::AggressorSemiBluff;
    s.multi_tier = true;
    s.aggressor_equity_pct = 45.0;
    return s;
}

void set_box(it::InterrogatorState& state, eng::InputId id, std::optional<std::uint8_t> tier,
             const char* text) {
    for (it::NumericBox& b : state.boxes) {
        if (b.input == id && b.tier == tier) {
            std::strncpy(b.text.data(), text, b.text.size() - 1);
            return;
        }
    }
    ADD_FAILURE() << "no box for the requested input/tier";
}

// Fill THIS tier's Fold + EV (and the tier-1 Equity-if-Called for Semi-Bluff).
void fill_tier(it::InterrogatorState& state, std::uint8_t tier, bool semi) {
    set_box(state, eng::InputId::FoldProbability, tier, "40");
    set_box(state, eng::InputId::Ev, tier, "12");
    if (semi && tier == 0) {
        set_box(state, eng::InputId::Equity, std::nullopt, "45");
    }
}

}  // namespace

// ----- The gate: only multi-tier Aggressor is sequential -----

TEST(TierFlow, IsSequentialOnlyForMultiTierAggressor) {
    it::InterrogatorState empty{};
    EXPECT_FALSE(it::is_sequential(empty));  // no scenario

    it::InterrogatorState c{};
    it::configure_for_scenario(c, caller());
    EXPECT_FALSE(it::is_sequential(c));

    it::InterrogatorState single{};
    it::configure_for_scenario(single, single_tier_aggressor());
    EXPECT_FALSE(it::is_sequential(single));

    it::InterrogatorState multi{};
    it::configure_for_scenario(multi, multi_tier_pure_bluff());
    EXPECT_TRUE(it::is_sequential(multi));
}

// ----- Forward-only advance, clamped at the last tier -----

TEST(TierFlow, NextTierIsForwardOnlyAndClamps) {
    EXPECT_EQ(it::next_tier(0), 1);
    EXPECT_EQ(it::next_tier(1), 2);
    EXPECT_EQ(it::next_tier(2), 3);
    EXPECT_EQ(it::next_tier(3), 3);  // last tier: no wrap, no revisit
}

TEST(TierFlow, ConfigureOpensOnTierOne) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, multi_tier_semi_bluff());
    EXPECT_EQ(state.current_tier, 0);
}

// ----- The per-tier visible set -----

TEST(TierFlow, CurrentViewIsOneTierForMultiTier) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, multi_tier_semi_bluff());

    // Tier 1 (index 0): Fold0, EV0, and the once-only Equity-if-Called.
    {
        const std::vector<const it::NumericBox*> view = it::current_view_boxes(state);
        ASSERT_EQ(view.size(), 3u);
        EXPECT_EQ(view[0]->input, eng::InputId::FoldProbability);
        EXPECT_EQ(view[0]->tier, std::optional<std::uint8_t>{0});
        EXPECT_EQ(view[1]->input, eng::InputId::Ev);
        EXPECT_EQ(view[1]->tier, std::optional<std::uint8_t>{0});
        EXPECT_EQ(view[2]->input, eng::InputId::Equity);
        EXPECT_FALSE(view[2]->tier.has_value());  // bet-size-independent
    }

    // Tier 2 (index 1): just Fold1, EV1 -- Equity does NOT reappear.
    state.current_tier = 1;
    {
        const std::vector<const it::NumericBox*> view = it::current_view_boxes(state);
        ASSERT_EQ(view.size(), 2u);
        EXPECT_EQ(view[0]->input, eng::InputId::FoldProbability);
        EXPECT_EQ(view[0]->tier, std::optional<std::uint8_t>{1});
        EXPECT_EQ(view[1]->input, eng::InputId::Ev);
        EXPECT_EQ(view[1]->tier, std::optional<std::uint8_t>{1});
    }
}

TEST(TierFlow, MultiTierPureBluffTierOneHasNoEquity) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, multi_tier_pure_bluff());
    const std::vector<const it::NumericBox*> view = it::current_view_boxes(state);
    ASSERT_EQ(view.size(), 2u);  // Fold0, EV0 -- Pure Bluff has no Equity-if-Called
    EXPECT_EQ(view[0]->input, eng::InputId::FoldProbability);
    EXPECT_EQ(view[1]->input, eng::InputId::Ev);
}

TEST(TierFlow, CurrentViewIsEveryBoxForNonSequential) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, caller());
    const std::vector<const it::NumericBox*> view = it::current_view_boxes(state);
    EXPECT_EQ(view.size(), state.boxes.size());  // one screen, all four inputs
}

// ----- Advance only when THIS tier's required inputs are filled -----

TEST(TierFlow, RequiredFilledNeedsThisTiersFoldEvAndTierOneEquity) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, multi_tier_semi_bluff());

    EXPECT_FALSE(it::current_tier_required_filled(state));  // nothing filled
    set_box(state, eng::InputId::FoldProbability, static_cast<std::uint8_t>(0), "40");
    set_box(state, eng::InputId::Ev, static_cast<std::uint8_t>(0), "12");
    EXPECT_FALSE(it::current_tier_required_filled(state));  // Equity still missing on tier 1
    set_box(state, eng::InputId::Equity, std::nullopt, "45");
    EXPECT_TRUE(it::current_tier_required_filled(state));
}

TEST(TierFlow, RequiredFilledIgnoresOtherTiers) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, multi_tier_pure_bluff());
    // Fill ONLY tier 2's boxes; tier 1 (the current screen) is still empty.
    set_box(state, eng::InputId::FoldProbability, static_cast<std::uint8_t>(1), "40");
    set_box(state, eng::InputId::Ev, static_cast<std::uint8_t>(1), "12");
    EXPECT_FALSE(it::current_tier_required_filled(state));

    fill_tier(state, 0, /*semi=*/false);
    EXPECT_TRUE(it::current_tier_required_filled(state));
}

TEST(TierFlow, BetSizePickDoesNotGateAdvance) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, multi_tier_pure_bluff());
    fill_tier(state, 0, /*semi=*/false);
    ASSERT_FALSE(state.bet_group.selected.has_value());  // no pick
    // An unpicked Bet Size still allows advancing (it just grades wrong on submit).
    EXPECT_TRUE(it::current_tier_required_filled(state));
    EXPECT_EQ(it::enter_action(state), it::EnterAction::Advance);
}

// ----- enter_action: None until filled, Advance mid-run, Submit on the last tier -----

TEST(TierFlow, EnterActionNoneWhenCurrentTierUnfilled) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, multi_tier_semi_bluff());
    EXPECT_EQ(it::enter_action(state), it::EnterAction::None);
}

TEST(TierFlow, EnterActionAdvanceWhenFilledAndNotLast) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, multi_tier_semi_bluff());
    fill_tier(state, 0, /*semi=*/true);
    EXPECT_FALSE(it::is_last_tier(state));
    EXPECT_EQ(it::enter_action(state), it::EnterAction::Advance);
}

TEST(TierFlow, EnterActionSubmitOnLastTierWhenFilled) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, multi_tier_semi_bluff());
    state.current_tier = static_cast<std::uint8_t>(eng::kBetTierCount - 1);  // tier 4
    EXPECT_TRUE(it::is_last_tier(state));
    EXPECT_EQ(it::enter_action(state), it::EnterAction::None);  // tier 4 unfilled
    fill_tier(state, static_cast<std::uint8_t>(eng::kBetTierCount - 1), /*semi=*/true);
    EXPECT_EQ(it::enter_action(state), it::EnterAction::Submit);
}

// ----- Containment: the Bet Size pick persists and the answer set is unchanged -----

TEST(TierFlow, BoxesHoldEveryTierImmediately) {
    // The sequential flow never rebuilds boxes on advance: configure_for_scenario
    // spawns every tier's Fold + EV (+ one Equity) up front, so the submitted set
    // is identical to the all-at-once shape -- only the per-screen VIEW differs.
    it::InterrogatorState state{};
    it::configure_for_scenario(state, multi_tier_semi_bluff());
    std::size_t fold = 0, ev = 0, equity = 0;
    for (const it::NumericBox& b : state.boxes) {
        if (b.input == eng::InputId::FoldProbability) ++fold;
        if (b.input == eng::InputId::Ev) ++ev;
        if (b.input == eng::InputId::Equity) ++equity;
    }
    EXPECT_EQ(fold, eng::kBetTierCount);
    EXPECT_EQ(ev, eng::kBetTierCount);
    EXPECT_EQ(equity, 1u);  // bet-size-independent: spawned once
}

TEST(TierFlow, PickPersistsAcrossTiersAndIsSubmitted) {
    it::InterrogatorState state{};
    it::configure_for_scenario(state, multi_tier_semi_bluff());
    state.bet_group.selected = eng::BetTier::FullPot;  // picked on tier 1

    // Walk forward through every tier, filling each tier's inputs as it shows.
    fill_tier(state, 0, /*semi=*/true);
    for (std::uint8_t t = 1; t < eng::kBetTierCount; ++t) {
        state.current_tier = it::next_tier(state.current_tier);
        ASSERT_EQ(state.current_tier, t);
        fill_tier(state, t, /*semi=*/true);
        // The pick made on tier 1 survives every advance.
        EXPECT_EQ(state.bet_group.selected, eng::BetTier::FullPot);
    }

    // gather_answers reads the full accumulated set regardless of current_tier.
    const eng::UserAnswers a = it::gather_answers(state);
    EXPECT_EQ(a.selected_bet_tier, eng::BetTier::FullPot);
    EXPECT_TRUE(a.equity_if_called_pct.has_value());
    for (std::size_t t = 0; t < eng::kBetTierCount; ++t) {
        EXPECT_TRUE(a.tier_fold_pct[t].has_value()) << "tier " << t << " fold";
        EXPECT_TRUE(a.tier_ev[t].has_value()) << "tier " << t << " ev";
    }
}
