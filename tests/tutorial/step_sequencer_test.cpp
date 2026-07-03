#include "tutorial/step_sequencer.hpp"

#include "engine/scenario.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace pt = poker_trainer::tutorial;
namespace eng = poker_trainer::engine;

namespace {

[[nodiscard]] pt::StepScreen expected_screen_for(std::uint8_t step) {
    switch (step) {
        case 1: return pt::StepScreen::Root;
        case 2: return pt::StepScreen::ModeSelection;
        case 3: return pt::StepScreen::Game;
        case 4: return pt::StepScreen::Game;
        case 5: return pt::StepScreen::PostRound;
        case 6: return pt::StepScreen::Game;
        case 7: return pt::StepScreen::PostRound;
        default: return pt::StepScreen::Root;
    }
}

[[nodiscard]] std::size_t count_advance(pt::AdvanceKind kind) {
    std::size_t n = 0;
    for (const pt::SubStep& s : pt::tutorial_script()) {
        if (s.advance == kind) {
            ++n;
        }
    }
    return n;
}

}  // namespace

TEST(TutorialScript, NonEmptyAndLengthMatchesAccessor) {
    EXPECT_FALSE(pt::tutorial_script().empty());
    EXPECT_EQ(pt::tutorial_script().size(), pt::tutorial_script_len());
}

TEST(TutorialScript, CoarseStepsAreNonDecreasingAndSpanOneToSeven) {
    std::uint8_t prev = 1;
    bool seen[pt::kStepCount + 1] = {};
    for (const pt::SubStep& s : pt::tutorial_script()) {
        EXPECT_GE(s.step, prev) << "steps must be ordered (non-decreasing)";
        EXPECT_GE(s.step, 1);
        EXPECT_LE(s.step, 7) << "Step 8 (Tutorial Complete) is terminal, not a scripted sub-step";
        prev = s.step;
        seen[s.step] = true;
    }
    for (std::uint8_t step = 1; step <= 7; ++step) {
        EXPECT_TRUE(seen[step]) << "coarse step " << static_cast<int>(step) << " missing";
    }
}

TEST(TutorialScript, EachSubStepIsOnItsStepsScreen) {
    for (const pt::SubStep& s : pt::tutorial_script()) {
        EXPECT_EQ(s.screen, expected_screen_for(s.step))
            << "sub-step in step " << static_cast<int>(s.step) << " on the wrong screen";
    }
}

TEST(TutorialScript, CoarseStepForIndexRunsOffEndToTutorialComplete) {
    EXPECT_EQ(pt::coarse_step_for_index(0), 1);
    EXPECT_EQ(pt::coarse_step_for_index(pt::tutorial_script_len()), pt::kStepCount);
    EXPECT_EQ(pt::coarse_step_for_index(pt::tutorial_script_len() + 5), pt::kStepCount);
}

TEST(TutorialScript, ExactlyOneOfEachEngineDrivingAction) {
    // The two teaching-seed launches and the finish are each scripted exactly once.
    EXPECT_EQ(count_advance(pt::AdvanceKind::LaunchCaller), 1u);
    EXPECT_EQ(count_advance(pt::AdvanceKind::LaunchAggressor), 1u);
    EXPECT_EQ(count_advance(pt::AdvanceKind::FinishTutorial), 1u);
    EXPECT_EQ(count_advance(pt::AdvanceKind::PlayMorph), 1u);
    EXPECT_EQ(count_advance(pt::AdvanceKind::MathTiers234), 1u);
    // Three "open a modal and close it" steps on Root (Settings/Shop/Help).
    EXPECT_EQ(count_advance(pt::AdvanceKind::ModalExplore), 3u);
    EXPECT_EQ(count_advance(pt::AdvanceKind::CustomExplore), 1u);
    // The Shop -> Leaderboard step (Group E #4).
    EXPECT_EQ(count_advance(pt::AdvanceKind::LeaderboardExplore), 1u);
}

TEST(TutorialScript, LaunchActionsLandOnTheRightStepsAndElements) {
    for (const pt::SubStep& s : pt::tutorial_script()) {
        if (s.advance == pt::AdvanceKind::LaunchCaller) {
            EXPECT_EQ(s.step, 2);
            EXPECT_EQ(s.spot, pt::SpotTarget::ModeStandard);  // STANDARD click
        }
        if (s.advance == pt::AdvanceKind::LaunchAggressor) {
            EXPECT_EQ(s.step, 5);
            EXPECT_EQ(s.spot, pt::SpotTarget::PostAgain);  // Caller recap Again
        }
        if (s.advance == pt::AdvanceKind::FinishTutorial) {
            EXPECT_EQ(s.step, 7);
            EXPECT_EQ(s.spot, pt::SpotTarget::PostAgain);  // Aggressor recap Again
        }
        if (s.advance == pt::AdvanceKind::PlayMorph) {
            EXPECT_EQ(s.step, 1);
            EXPECT_EQ(s.spot, pt::SpotTarget::RootPlay);
        }
    }
}

TEST(TutorialScript, CallerWalkthroughIsFourInputsThenSubmitOnEv) {
    // Collect Step 4's math sub-steps in order. The first three inputs each have two
    // stages (formula, then worked numbers); the last input (EV) collapses its second
    // stage into the submit step, whose callout ends with the Enter-to-submit prompt
    // (Group §5) -- so there is no separate post-Tab "press Enter" follow-up.
    std::vector<const pt::SubStep*> step4;
    for (const pt::SubStep& s : pt::tutorial_script()) {
        if (s.step == 4) {
            step4.push_back(&s);
        }
    }
    ASSERT_EQ(step4.size(), 8u);  // 3 inputs x 2 stages + EV(stage1 + submit)
    const eng::InputId order[3] = {eng::InputId::PotOdds, eng::InputId::Outs, eng::InputId::Equity};
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(step4[static_cast<std::size_t>(i * 2)]->advance, pt::AdvanceKind::MathStage1);
        EXPECT_EQ(step4[static_cast<std::size_t>(i * 2)]->math_input, order[i]);
        EXPECT_EQ(step4[static_cast<std::size_t>(i * 2 + 1)]->advance, pt::AdvanceKind::MathStage2);
        EXPECT_EQ(step4[static_cast<std::size_t>(i * 2 + 1)]->math_input, order[i]);
    }
    // EV: stage 1 (formula) then the submit step (no separate MathStage2).
    EXPECT_EQ(step4[6]->advance, pt::AdvanceKind::MathStage1);
    EXPECT_EQ(step4[6]->math_input, eng::InputId::Ev);
    EXPECT_EQ(step4[7]->advance, pt::AdvanceKind::MathSubmit);
    EXPECT_EQ(step4[7]->math_input, eng::InputId::Ev);
    // Exactly one submit step in the whole script (the caller EV), and no MathStage2 for EV.
    EXPECT_EQ(count_advance(pt::AdvanceKind::MathSubmit), 1u);
}

TEST(TutorialScript, AggressorStepSpotlightsTheShownFoldBeforeTheMath) {
    // The redesign shows the opponent fold % on screen; Step 6 must teach reading it
    // (an informational Next step spotlighting the on-felt readout) BEFORE the first
    // math sub-step. (Group §1: reading F is the key concept.)
    const pt::SubStep* fold_read = nullptr;
    const pt::SubStep* first_math = nullptr;
    std::size_t idx = 0;
    std::size_t fold_read_idx = 0;
    std::size_t first_math_idx = 0;
    for (const pt::SubStep& s : pt::tutorial_script()) {
        if (s.step == 6) {
            if (s.spot == pt::SpotTarget::GameOppFold && fold_read == nullptr) {
                fold_read = &s;
                fold_read_idx = idx;
            }
            if (first_math == nullptr && s.advance == pt::AdvanceKind::MathStage1) {
                first_math = &s;
                first_math_idx = idx;
            }
        }
        ++idx;
    }
    ASSERT_NE(fold_read, nullptr) << "Step 6 must spotlight the shown opponent fold %";
    EXPECT_EQ(fold_read->advance, pt::AdvanceKind::Next);
    ASSERT_NE(first_math, nullptr);
    EXPECT_LT(fold_read_idx, first_math_idx) << "reading F comes before the math";
}

TEST(TutorialScript, AggressorTierOneWalkthroughCoversFoldEvBetSize) {
    bool fold = false;
    bool ev = false;
    bool bet = false;
    for (const pt::SubStep& s : pt::tutorial_script()) {
        if (s.step != 6 || s.advance != pt::AdvanceKind::MathStage1) {
            continue;
        }
        fold |= (s.math_input == eng::InputId::FoldProbability);
        ev |= (s.math_input == eng::InputId::Ev);
        bet |= (s.math_input == eng::InputId::BetSize);
    }
    EXPECT_TRUE(fold);
    EXPECT_TRUE(ev);
    EXPECT_TRUE(bet);
}

TEST(TutorialScript, AdvanceClassifiers) {
    // Only the informational tour steps are Next-bearing now: the math walkthrough
    // advances by keypress / Tab and shows no Next button (Group A).
    EXPECT_TRUE(pt::advances_on_next(pt::AdvanceKind::Next));
    EXPECT_FALSE(pt::advances_on_next(pt::AdvanceKind::MathStage1));
    EXPECT_FALSE(pt::advances_on_next(pt::AdvanceKind::MathStage2));
    EXPECT_FALSE(pt::advances_on_next(pt::AdvanceKind::MathSubmit));
    EXPECT_FALSE(pt::advances_on_next(pt::AdvanceKind::LaunchCaller));

    EXPECT_TRUE(pt::is_action_advance(pt::AdvanceKind::LaunchCaller));
    EXPECT_TRUE(pt::is_action_advance(pt::AdvanceKind::ModalExplore));
    EXPECT_TRUE(pt::is_action_advance(pt::AdvanceKind::LeaderboardExplore));
    EXPECT_TRUE(pt::is_action_advance(pt::AdvanceKind::FinishTutorial));
    EXPECT_FALSE(pt::is_action_advance(pt::AdvanceKind::Next));
    EXPECT_FALSE(pt::is_action_advance(pt::AdvanceKind::MathStage1));
}

TEST(TutorialScript, EveryNonNoneSubStepHasCalloutText) {
    for (const pt::SubStep& s : pt::tutorial_script()) {
        EXPECT_FALSE(s.callout.empty()) << "sub-step in step " << static_cast<int>(s.step)
                                        << " has empty callout text";
    }
}
