#pragma once

#include "tutorial/forced_settings.hpp"

#include "backbone/focus_manager.hpp"
#include "engine/scenario_id.hpp"
#include "settings/settings.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>

// Zone 14 — Tutorial System public interface (ZONES.md exports + the boot seams).
//
// Z14 is a coordination layer: it drives the existing zones through a scripted
// 8-step sequence (overlay.cpp + step_sequencer.cpp), forces three settings
// (forced_settings.cpp), renders callouts + the skip button + the first-launch
// prompt (callout.cpp), and ends on the terminal Tutorial Complete screen
// (screens/tutorial_complete_screen.cpp). It owns almost no isolated logic; the
// engine, math interrogator, temporal layer, and modal system are unaware they are
// in a tutorial and are reached through the backbone (the shared tutorial phase)
// and the boot-wired seams below.

namespace poker_trainer::tutorial {

// ----- Hardcoded teaching scenario seeds (ARCHITECTURE §Hardcoded Scenario Seeds) -----
//
// Found by a one-time scan of seed values against the real generator and LOCKED
// here (the locking test asserts the produced hands still match these criteria, so a
// generator change is caught). The engine generates from these deterministically and
// is unaware they are the tutorial's.
//
//   kTutorialScenarioCallerSeed   (TUTORIAL_SCENARIO_CALLER_SEED):
//     Caller on the Flop — pot $100, faced bet $50 → pot odds exactly 33.3%
//     (= 50 / (100 + 50), the spec's worked example), 4 outs / 16% equity.
//   kTutorialScenarioAggressorSeed (TUTORIAL_SCENARIO_AGGRESSOR_SEED):
//     Aggressor Pure Bluff on the Turn — pot $2400 (divisible by 600, so all four
//     bet sizes are round: $800 / $1200 / $2400 / $3600), multi-tier, with a clean
//     monotonic EV ranking (tier 1 = 1/3 pot clearly best).
inline constexpr engine::ScenarioId kTutorialScenarioCallerSeed{54244};
inline constexpr engine::ScenarioId kTutorialScenarioAggressorSeed{471};

// ----- Boot-wired seams -----
//
// Every cross-zone effect the tutorial needs that isn't a backbone primitive flows
// through one of these, wired once in finish_boot_after_persistence. Keeping them as
// std::function members means Z14's library links only backbone / theme / bridge /
// modal / engine — never persistence, settings, or the screen zones directly.
struct TutorialSeams {
    // The app's live settings snapshot (the one the generator + Z08/Z10 read). The
    // tutorial captures three fields, forces them, and restores them on exit — in
    // memory only, never persisted (so durable IDBFS settings are untouched).
    settings::Settings* live_settings{nullptr};

    // has_seen_tutorial_prompt lifecycle (persistence_service).
    std::function<bool()> has_seen_prompt;   // read
    std::function<void()> mark_prompt_seen;  // set true  (skip / completion)
    std::function<void()> clear_prompt_seen; // set false (manual re-launch from Help)
    std::function<void()> mark_completed;    // has_completed_tutorial = true (reach Step 8)

    // Account state — guests get a Sign Up button on Tutorial Complete; signed-in
    // users do not.
    std::function<bool()> is_authenticated;

    // Launch a specific scenario id through the normal launch path (Z05 game_launch).
    // The engine generates from the seed unaware it is in a tutorial.
    std::function<void(engine::ScenarioId)> launch_scenario;

    // Z07 Custom popup open flag — the CustomExplore advance (open → close).
    std::function<bool()> custom_popup_open;

    // Auth0 health check + open the real Sign Up modal (Tutorial Complete → Sign Up).
    std::function<bool()> auth_health_check;
    std::function<void()> open_sign_up_modal;
};

// App-root tutorial runtime, owned by Z05 boot (CLAUDE.md §10 — the tutorial keeps
// no global mutable state; install_tutorial stores a non-owning pointer to this, the
// same pattern as modal_runtime()). Holds the wired seams plus the live cursor.
struct TutorialRuntime {
    TutorialSeams seams{};

    // Cursor into tutorial_script(); == tutorial_script_len() once the walkthrough has
    // run off the end (→ Tutorial Complete).
    std::size_t cursor{0};

    // The first-launch prompt shows at most once per session (ARCHITECTURE: "the first
    // time in a session"), gated by this even while has_seen_tutorial_prompt is false.
    bool prompt_shown_this_session{false};

    // Forced-settings backup, captured at start and restored on completion / skip.
    SavedSettings saved_settings{};
    bool settings_forced{false};

    // ModalExplore / CustomExplore / LeaderboardExplore tracking: true once the step's
    // modal has been seen open, so the open->closed edge advances (not the opening frame).
    bool explore_seen_open{false};

    // Which of the current step's tutorial controls (Previous / Next-or-ScriptedAction /
    // Skip) keyboard focus is trapped on. The tutorial owns its own focus ring here --
    // the underlying screen's focusables are never Tab-reachable while the overlay is
    // active (Group A). Reset to the step's primary control on every cursor move.
    std::size_t control_index{0};
};

// ----- ZONES.md exports -----

// Begin the walkthrough: capture + force settings, set phase Active at step 1, and
// (for a manual re-launch) clear has_seen_tutorial_prompt. Idempotent if already
// active.
void tutorial_start();

// Abort the walkthrough: restore forced settings, mark the prompt seen, return to
// Root, set phase Inactive. Used by the Skip confirmation's Yes.
void tutorial_skip();

// Advance to the next sub-step (the Next button / Enter outside a math context).
void tutorial_advance();

// True while the walkthrough is running (backbone tutorial phase == Active).
[[nodiscard]] bool is_tutorial_active() noexcept;

// True while the active sub-step is a math-walkthrough stage-1 callout ("Press N to
// focus this input"): the taught box must NOT capture ImGui keyboard, so the digit
// key reaches the tutorial (which focuses the box and advances) instead of being
// typed. Z09 reads this (a boot-wired std::function, dependency-inverted) to keep
// every math box inert that frame. False whenever no tutorial is active.
[[nodiscard]] bool tutorial_suppresses_text_focus() noexcept;

// The single math box the active sub-step is teaching (Steps 4 / 6 stage 1 & 2), so
// Z09 can disable EVERY OTHER box -- only the taught input is focusable / typeable,
// the rest reject focus and input until the tutorial advances to them (Group A's
// input contract). kNoFocus when the active step is not a per-box math stage (the
// table tour, the submit step, the tiers-2-4 free play, or no tutorial), so nothing
// is gated then. Z09 reads it through a boot-wired std::function (dependency-inverted).
[[nodiscard]] backbone::FocusableId tutorial_live_math_box() noexcept;

// Open the standard Yes/No Skip-Tutorial confirmation (named by ZONES.md as the
// escape handler's action). Yes → tutorial_skip().
void tutorial_skip_confirmation_open();

// The Again-commit redirect. Z13's commit_again calls this (via the override seam
// boot wires) on the Again button's committing press; while the tutorial is active it
// launches the Aggressor teaching seed (Caller recap) or moves to the Tutorial
// Complete screen (Aggressor recap), and returns true so Z13 skips its replay launch.
// Returns false when no tutorial is active (Z13 proceeds normally).
[[nodiscard]] bool on_again_commit();

// ----- Boot integration -----

// Store the runtime pointer, register the overlay event handlers (mouse/key/escape at
// the TutorialOverlay router priority), and register the Tutorial Complete screen
// renderer. Called once from Z05 boot; `runtime` must outlive the app.
void install_tutorial(TutorialRuntime& runtime);

// Per-frame overlay render + advance polling. Z05's main loop calls this once per
// frame, ABOVE the active screen and BELOW the Z11 modal overlay. A no-op unless the
// tutorial phase is Prompt or Active.
void render_tutorial_overlay();

}  // namespace poker_trainer::tutorial
