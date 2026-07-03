#pragma once

#include "engine/scenario.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

// Zone 14 — the tutorial step sequencer (pure data + pure helpers).
//
// The tutorial is a scripted sequence of overlay "sub-steps" spread across the
// existing screens (Root, Mode Selection, Game ×2, Post-Round ×2) plus the
// terminal Tutorial Complete screen. This header models that script as a flat,
// ordered table of SubStep records and exposes pure helpers over it. The live
// overlay (overlay.cpp) walks this table: it resolves each sub-step's SpotTarget
// to an on-screen rect, renders the callout, and advances the cursor per the
// sub-step's AdvanceKind. Keeping the script and its invariants here (separate
// from the ImGui render path) is what makes the sequence unit-testable.
//
// ARCHITECTURE "Notes — Tutorial System" §Sequence is the source of truth for the
// step contents; this table is its 1:1 transcription.

namespace poker_trainer::tutorial {

// The eight coarse steps the architecture enumerates. The backbone TutorialState
// carries this 1-based coarse index (active_step); the finer sub-step cursor is
// internal to the overlay.
inline constexpr std::uint8_t kStepCount = 8;

// Which screen a sub-step lives on. The overlay uses this to gate a sub-step to
// the screen it belongs to and to detect the screen-transition advances.
enum class StepScreen : std::uint8_t {
    Root = 0,
    ModeSelection = 1,
    Game = 2,
    PostRound = 3,
};

// The element a sub-step spotlights. overlay.cpp::resolve_spotlight maps each to a
// canvas-relative rect. None = no spotlight cutout (the lens steps back; e.g. the
// "do tiers 2-4 yourself" free-play moment).
enum class SpotTarget : std::uint8_t {
    None = 0,

    // Root screen.
    RootLogo,
    RootButtonGrid,
    RootSettings,
    RootShop,
    RootHelp,
    RootHome,
    RootPlay,

    // Mode Selection screen.
    ModeCluster,
    ModeAggressor,
    ModeCaller,
    ModeCustom,
    ModeStandard,

    // Game screen — table tour.
    GameChipLegend,
    GamePotBlinds,
    GameCommunityCards,
    GameHoleCards,
    GameOpponents,
    GamePushedChips,
    GameClusterX,
    GameOppEmptyChips,  // Aggressor: empty chip area in front of the opponent
    GameOppFold,        // Aggressor: the on-felt "Opp fold: X%" readout (the shown F)

    // Game screen — math inputs (spotlight follows the walked input).
    GameMathPotOdds,
    GameMathOuts,
    GameMathEquity,
    GameMathEv,
    GameMathFold,
    GameMathTierEv,
    GameMathBetSize,

    // Post-Round screen.
    PostStatColumns,
    PostTimeGradeRow,
    PostOverallRow,
    PostDealer,
    PostScenarioId,
    PostAgain,
    PostTierTabs,
    PostSummaryTab,
};

// How the user advances off a sub-step.
enum class AdvanceKind : std::uint8_t {
    // Informational: the Next button (or Enter outside a math context) advances.
    Next = 0,
    // Click the spotlit element to open its modal; advance when the modal closes
    // (the "open ... then close to continue" pattern). Settings / Shop / Help.
    ModalExplore,
    // Reach the Leaderboard the real way: the user reopens the Shop (the spotlight
    // sits on the Root Shop button; clicking it / Enter opens the Shop), then clicks
    // the Leaderboard swap icon INSIDE the Shop modal (spotlit on the overlay's
    // foreground draw list while the Shop is open). Advance when the Leaderboard has
    // been opened (via that in-Shop icon) and then closed.
    LeaderboardExplore,
    // Same, but the Custom popup (a Z07 popup, not a Z11 stack modal).
    CustomExplore,
    // Click Play; advance when the Root→Mode morph lands on Mode Selection.
    PlayMorph,
    // Click STANDARD; intercept → launch the Caller teaching seed; advance on Game.
    LaunchCaller,
    // Caller Post-Round: Again (arm then commit); intercept the commit → launch the
    // Aggressor teaching seed; advance on Game.
    LaunchAggressor,
    // Aggressor Post-Round: Again (arm then commit); intercept the commit → go to
    // the Tutorial Complete screen.
    FinishTutorial,
    // Math walkthrough stage 1 (formula + definition; input inert, no Next button).
    // Advances ONLY when the user presses the positional number key the callout
    // names, which also moves focus to the taught input. No other box is focusable.
    MathStage1,
    // Math walkthrough stage 2 (live numbers; ONLY the taught input is interactive;
    // no Next button). Advances ONLY by the prescribed action: type the answer, then
    // Tab to the next input's stage 1 (or the submit sub-step).
    MathStage2,
    // Final Caller / tier-1 input walked: "Press Enter to submit." Z09 owns Enter;
    // advance when the screen transitions to Post-Round.
    MathSubmit,
    // "Now do tiers 2-4 yourself." The lens steps back; advance on Post-Round.
    MathTiers234,
};

// One scripted sub-step.
struct SubStep {
    std::uint8_t step{1};            // coarse step 1..8 (drives backbone active_step)
    StepScreen screen{StepScreen::Root};
    SpotTarget spot{SpotTarget::None};
    AdvanceKind advance{AdvanceKind::Next};
    // The math input a MathStage1/MathStage2 sub-step walks (for the live-number
    // callout). engine::InputId::PotOdds is the unused default for non-math steps.
    engine::InputId math_input{engine::InputId::PotOdds};
    std::string_view callout;        // the static teaching text (stage 2 appends live numbers)
};

// The full ordered script (Root → … → Aggressor Post-Round). Step 8 (Tutorial
// Complete) is a terminal screen, not a spotlit sub-step, so it has no entry here;
// reaching the end of the table is what drives the transition to it.
[[nodiscard]] std::span<const SubStep> tutorial_script() noexcept;

// The index just past the last sub-step (the "finished" cursor sentinel).
[[nodiscard]] std::size_t tutorial_script_len() noexcept;

// The coarse step (1..8) the overlay should report to the backbone for the sub-step
// at `index`. At or past the end → kStepCount (8, Tutorial Complete).
[[nodiscard]] std::uint8_t coarse_step_for_index(std::size_t index) noexcept;

// True when the sub-step's advance is driven by an in-world action (a click that
// opens a modal, a launch, a morph, an Again commit) rather than the Next button.
[[nodiscard]] bool is_action_advance(AdvanceKind kind) noexcept;

// True when the Next button (and Enter, outside math) should advance this sub-step.
[[nodiscard]] bool advances_on_next(AdvanceKind kind) noexcept;

}  // namespace poker_trainer::tutorial
