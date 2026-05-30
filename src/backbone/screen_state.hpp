#pragma once

#include "engine/scenario_id.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace poker_trainer::backbone {

// The set of top-level screens. The application is on exactly one
// of these at any time.
enum class ScreenId : std::uint8_t {
    Root = 0,
    ModeSelection = 1,
    Game = 2,
    PostRound = 3,
    TutorialComplete = 4,
    // Boot/asset fatal-failure screen (Z05). Not part of the
    // architecture's screen enumeration; tracked here as a 6th value per
    // the backbone amendment so the Tier 1/2 fatal-failure path has a
    // representable screen state.
    Error = 5,
};

inline constexpr std::size_t kScreenCount = 6;

// Runtime phase of the in-session tutorial. Distinct from the persisted
// has_seen_tutorial_prompt flag (persistence_schema.hpp): this tracks the
// live progression of the tutorial within the current session and resets
// every launch.
enum class TutorialPhase : std::uint8_t {
    Inactive = 0,  // No tutorial running.
    Prompt = 1,    // First-run prompt offered; not yet accepted or declined.
    Active = 2,    // Walkthrough running; the current step is active_step.
    Complete = 3,  // Walkthrough finished this session.
};

// Tutorial state tracked by the screen-state singleton. The architecture
// models this as Inactive | Prompt | Active(step_n) | Complete; active_step
// carries the step_n and is meaningful only when phase == Active.
struct TutorialState {
    TutorialPhase phase{TutorialPhase::Inactive};
    std::uint8_t active_step{0};  // 1-based step index while Active; 0 otherwise.
};

// Snapshot of the screen state at the moment of read.
struct ScreenStateSnapshot {
    ScreenId current{ScreenId::Root};

    // When current == Game or current == PostRound, this is the
    // ScenarioId of the active scenario. When current is any other
    // screen, this is std::nullopt.
    std::optional<engine::ScenarioId> active_scenario;

    // Live tutorial progression for this session.
    TutorialState tutorial_state{};
};

// Read the current screen state.
[[nodiscard]] ScreenStateSnapshot read_screen_state() noexcept;

// Set the current screen. Called by Z05 and Z14 to drive transitions.
// When transitioning into Game, the active_scenario must be provided.
// When transitioning into any other screen, active_scenario should
// be std::nullopt; the implementation clears it if provided.
void set_screen(ScreenId screen,
                std::optional<engine::ScenarioId> active_scenario) noexcept;

// Set the tutorial state. Called by the transition orchestrator (Z05/Z14)
// as the tutorial progresses. Read-only for everyone else (via
// read_screen_state). Preserves the current screen and active scenario.
void set_tutorial_state(TutorialState tutorial_state) noexcept;

// Convenience: returns true if the current screen is Game or
// PostRound (i.e., a scenario is active or just completed).
[[nodiscard]] bool is_in_scenario() noexcept;

// Reset to initial state (Root, no scenario). Used by the integration
// test only.
void reset_screen_state_for_testing() noexcept;

}  // namespace poker_trainer::backbone
