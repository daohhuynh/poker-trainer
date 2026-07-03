#include "tutorial/step_sequencer.hpp"

#include "engine/scenario.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace poker_trainer::tutorial {

namespace {

using engine::InputId;

// The full scripted sequence, a 1:1 transcription of ARCHITECTURE "Notes -
// Tutorial System" Sequence (Steps 1-7; Step 8 is the terminal Tutorial Complete
// screen, reached by running off the end of this table). The overlay appends the
// live action line to each math callout: "Press N to focus this input" on a
// MathStage1 (N is the positional keybind, computed from the live scenario), the
// worked "= <numbers>" line plus "Type your answer, then Tab" on a MathStage2.
// All copy here is ASCII (the bitmap font has no em-dash / x-sign / curly glyphs).
constexpr std::array<SubStep, 46> kScript{{
    // ----- Step 1 - Root screen -----
    {1, StepScreen::Root, SpotTarget::RootLogo, AdvanceKind::Next, InputId::PotOdds,
     "Welcome to Poker Trainer, your home base for drilling real-time poker math."},
    {1, StepScreen::Root, SpotTarget::RootButtonGrid, AdvanceKind::Next, InputId::PotOdds,
     "These are your main controls."},
    {1, StepScreen::Root, SpotTarget::RootSettings, AdvanceKind::ModalExplore, InputId::PotOdds,
     "Settings: themes, audio, units, and gameplay toggles. Click to open it, then "
     "close it to continue."},
    {1, StepScreen::Root, SpotTarget::RootShop, AdvanceKind::ModalExplore, InputId::PotOdds,
     "The Shop: spend Tomatoes you earn by passing scenarios on extra music tracks. "
     "Click to open it, then close it to continue."},
    {1, StepScreen::Root, SpotTarget::RootShop, AdvanceKind::LeaderboardExplore, InputId::PotOdds,
     "There is also a Leaderboard that ranks players by Lifetime Tomatoes. It shares the "
     "Shop's window. Reopen the Shop, then click the highlighted trophy icon at the top "
     "to open the Leaderboard. Close it to continue."},
    {1, StepScreen::Root, SpotTarget::RootHelp, AdvanceKind::ModalExplore, InputId::PotOdds,
     "Help: an equation reference with every formula and the grading rules. Click to "
     "open it, then close it to continue."},
    {1, StepScreen::Root, SpotTarget::RootHome, AdvanceKind::Next, InputId::PotOdds,
     "Home reloads the page to start fresh."},
    {1, StepScreen::Root, SpotTarget::RootPlay, AdvanceKind::PlayMorph, InputId::PotOdds,
     "Click Play to begin."},

    // ----- Step 2 - Mode Selection -----
    {2, StepScreen::ModeSelection, SpotTarget::ModeCluster, AdvanceKind::Next, InputId::PotOdds,
     "The controls from the home screen are here as icons now."},
    {2, StepScreen::ModeSelection, SpotTarget::ModeAggressor, AdvanceKind::Next, InputId::PotOdds,
     "Aggressor: you are on offense every hand. You decide how much to bet."},
    {2, StepScreen::ModeSelection, SpotTarget::ModeCaller, AdvanceKind::Next, InputId::PotOdds,
     "Caller: you face a bet every hand and decide whether to call."},
    {2, StepScreen::ModeSelection, SpotTarget::ModeCustom, AdvanceKind::CustomExplore, InputId::PotOdds,
     "Custom lets you mix the modes with your own weights. Click to open it, then "
     "close it to continue."},
    {2, StepScreen::ModeSelection, SpotTarget::ModeStandard, AdvanceKind::LaunchCaller, InputId::PotOdds,
     "STANDARD plays a balanced mix of modes. Click it to start your first scenario."},

    // ----- Step 3 - Game screen, Caller scenario (table tour) -----
    {3, StepScreen::Game, SpotTarget::GameChipLegend, AdvanceKind::Next, InputId::PotOdds,
     "This legend shows what each chip color is worth."},
    {3, StepScreen::Game, SpotTarget::GamePotBlinds, AdvanceKind::Next, InputId::PotOdds,
     "The pot total and the blinds for this hand."},
    {3, StepScreen::Game, SpotTarget::GameCommunityCards, AdvanceKind::Next, InputId::PotOdds,
     "The community cards everyone shares."},
    {3, StepScreen::Game, SpotTarget::GameHoleCards, AdvanceKind::Next, InputId::PotOdds,
     "Your two hole cards."},
    {3, StepScreen::Game, SpotTarget::GameOpponents, AdvanceKind::Next, InputId::PotOdds,
     "Your opponents, with their positions and stack sizes."},
    {3, StepScreen::Game, SpotTarget::GamePushedChips, AdvanceKind::Next, InputId::PotOdds,
     "This is a Caller scenario: an opponent has bet (chips pushed in toward the pot). "
     "You decide whether calling is profitable."},
    {3, StepScreen::Game, SpotTarget::GameClusterX, AdvanceKind::Next, InputId::PotOdds,
     "These icons open the Shop, Help, and Settings; the X leaves the drill. We will "
     "skip them for now."},

    // ----- Step 4 - Caller math walkthrough (two-stage per input) -----
    {4, StepScreen::Game, SpotTarget::GameMathPotOdds, AdvanceKind::MathStage1, InputId::PotOdds,
     "Pot Odds = bet / (pot + bet). The price you are getting to call: the equity you "
     "need to break even. Lower is cheaper."},
    {4, StepScreen::Game, SpotTarget::GameMathPotOdds, AdvanceKind::MathStage2, InputId::PotOdds,
     "Work it out with this hand's numbers."},
    {4, StepScreen::Game, SpotTarget::GameMathOuts, AdvanceKind::MathStage1, InputId::Outs,
     "Outs = the unseen cards that improve you to the best hand. Count them from your "
     "draw."},
    {4, StepScreen::Game, SpotTarget::GameMathOuts, AdvanceKind::MathStage2, InputId::Outs,
     "Count your outs for this hand."},
    {4, StepScreen::Game, SpotTarget::GameMathEquity, AdvanceKind::MathStage1, InputId::Equity,
     "Equity = your chance to win by the river. Rule of 2 and 4: outs x 4 on the flop, "
     "outs x 2 on the turn."},
    {4, StepScreen::Game, SpotTarget::GameMathEquity, AdvanceKind::MathStage2, InputId::Equity,
     "Apply the rule to your outs."},
    {4, StepScreen::Game, SpotTarget::GameMathEv, AdvanceKind::MathStage1, InputId::Ev,
     "EV = the average dollars a call wins or loses over the long run. Caller EV = "
     "equity x (pot + bet) - (1 - equity) x bet."},
    {4, StepScreen::Game, SpotTarget::GameMathEv, AdvanceKind::MathSubmit, InputId::Ev,
     "Plug your equity into the EV formula and type your answer, then press Enter to "
     "submit your answers whenever you are ready."},

    // ----- Step 5 - Post-Round, Caller recap -----
    {5, StepScreen::PostRound, SpotTarget::PostStatColumns, AdvanceKind::Next, InputId::PotOdds,
     "Each row shows the input, the correct answer with its margin, and what you "
     "entered. Green is within range, red is off."},
    {5, StepScreen::PostRound, SpotTarget::PostTimeGradeRow, AdvanceKind::Next, InputId::PotOdds,
     "In real scenarios this row grades your speed against a target time. It is "
     "disabled during the tutorial."},
    {5, StepScreen::PostRound, SpotTarget::PostOverallRow, AdvanceKind::Next, InputId::PotOdds,
     "Your overall result combines math accuracy and speed."},
    {5, StepScreen::PostRound, SpotTarget::PostDealer, AdvanceKind::Next, InputId::PotOdds,
     "The dealer's expression reflects how you did."},
    {5, StepScreen::PostRound, SpotTarget::PostScenarioId, AdvanceKind::Next, InputId::PotOdds,
     "Every scenario has an ID you can copy or share to replay it exactly."},
    {5, StepScreen::PostRound, SpotTarget::PostAgain, AdvanceKind::LaunchAggressor, InputId::PotOdds,
     "Click Again to continue to your next tutorial scenario."},

    // ----- Step 6 - Game screen, Aggressor Pure Bluff (multi-tier) -----
    {6, StepScreen::Game, SpotTarget::GameOppEmptyChips, AdvanceKind::Next, InputId::PotOdds,
     "Aggressor scenario: the opponent has not acted and there is no bet to face "
     "(their chip area is empty). You are on offense: you choose whether and how much "
     "to bet."},
    {6, StepScreen::Game, SpotTarget::GameOppFold, AdvanceKind::Next, InputId::PotOdds,
     "The opponent's fold chance is given to you, shown here on the felt and in the "
     "top-left readout. It changes with the bet size, board, and street. You read it "
     "off the screen - you never guess it. This shown fold % is the key to the "
     "aggressor's math."},
    {6, StepScreen::Game, SpotTarget::GameMathFold, AdvanceKind::MathStage1, InputId::FoldProbability,
     "Breakeven Fold % = bet / (pot + bet). It is the fold chance you NEED for a pure "
     "bluff to break even - the aggressor's mirror of pot odds. You compare it against "
     "the fold % shown above."},
    {6, StepScreen::Game, SpotTarget::GameMathFold, AdvanceKind::MathStage2, InputId::FoldProbability,
     "Work out the breakeven for this tier's size, then compare it to the shown fold "
     "chance."},
    {6, StepScreen::Game, SpotTarget::GameMathTierEv, AdvanceKind::MathStage1, InputId::Ev,
     "EV = the average dollars this bet wins or loses at this size. Pure Bluff EV = "
     "P(fold) x pot - P(call) x bet. Because the fold % is shown, you can compute it "
     "exactly."},
    {6, StepScreen::Game, SpotTarget::GameMathTierEv, AdvanceKind::MathStage2, InputId::Ev,
     "Plug the shown fold % into the EV formula for this tier's size."},
    {6, StepScreen::Game, SpotTarget::GameMathBetSize, AdvanceKind::MathStage1, InputId::BetSize,
     "Bet Size is one persistent pick of the best overall size. Since the fold % is "
     "shown for every size, you can compare their EVs and choose the highest."},
    {6, StepScreen::Game, SpotTarget::GameMathBetSize, AdvanceKind::MathStage2, InputId::BetSize,
     "Pick the size with the best EV; your choice carries to the later tiers."},
    {6, StepScreen::Game, SpotTarget::None, AdvanceKind::MathTiers234, InputId::PotOdds,
     "Now do tiers 2 to 4 yourself. Each screen shows that size's fold %, so the math "
     "is the same with new numbers. Press Enter to advance through each, and update "
     "your Bet Size pick if a later size looks better."},

    // ----- Step 7 - Post-Round, Aggressor recap -----
    {7, StepScreen::PostRound, SpotTarget::PostTierTabs, AdvanceKind::Next, InputId::PotOdds,
     "Multi-tier scenarios show your math at each bet size. Click the tabs to compare "
     "tiers."},
    {7, StepScreen::PostRound, SpotTarget::PostSummaryTab, AdvanceKind::Next, InputId::PotOdds,
     "The Summary tab rolls up the whole hand."},
    {7, StepScreen::PostRound, SpotTarget::PostAgain, AdvanceKind::FinishTutorial, InputId::PotOdds,
     "Click Again to finish the tutorial."},
}};

}  // namespace

std::span<const SubStep> tutorial_script() noexcept { return kScript; }

std::size_t tutorial_script_len() noexcept { return kScript.size(); }

std::uint8_t coarse_step_for_index(std::size_t index) noexcept {
    if (index >= kScript.size()) {
        return kStepCount;  // ran off the end → Tutorial Complete (step 8)
    }
    return kScript[index].step;
}

bool is_action_advance(AdvanceKind kind) noexcept {
    switch (kind) {
        case AdvanceKind::ModalExplore:
        case AdvanceKind::LeaderboardExplore:
        case AdvanceKind::CustomExplore:
        case AdvanceKind::PlayMorph:
        case AdvanceKind::LaunchCaller:
        case AdvanceKind::LaunchAggressor:
        case AdvanceKind::FinishTutorial:
            return true;
        case AdvanceKind::Next:
        case AdvanceKind::MathStage1:
        case AdvanceKind::MathStage2:
        case AdvanceKind::MathSubmit:
        case AdvanceKind::MathTiers234:
            return false;
    }
    return false;
}

bool advances_on_next(AdvanceKind kind) noexcept {
    // Only the informational tour steps carry a Next button (and a Previous button,
    // and advance on Enter/Space). The math walkthrough advances by keypress / Tab,
    // and the action steps by their scripted in-world gesture -- none show Next.
    switch (kind) {
        case AdvanceKind::Next:
            return true;
        case AdvanceKind::MathStage1:
        case AdvanceKind::MathStage2:
        case AdvanceKind::ModalExplore:
        case AdvanceKind::LeaderboardExplore:
        case AdvanceKind::CustomExplore:
        case AdvanceKind::PlayMorph:
        case AdvanceKind::LaunchCaller:
        case AdvanceKind::LaunchAggressor:
        case AdvanceKind::FinishTutorial:
        case AdvanceKind::MathSubmit:
        case AdvanceKind::MathTiers234:
            return false;
    }
    return false;
}

}  // namespace poker_trainer::tutorial
