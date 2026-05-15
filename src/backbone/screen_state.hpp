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
    Error = 4,  // The Tier 1/2 fatal failure screen (Z05)
};

inline constexpr std::size_t kScreenCount = 5;

// Snapshot of the screen state at the moment of read.
struct ScreenStateSnapshot {
    ScreenId current{ScreenId::Root};

    // When current == Game or current == PostRound, this is the
    // ScenarioId of the active scenario. When current is any other
    // screen, this is std::nullopt.
    std::optional<engine::ScenarioId> active_scenario;
};

// Read the current screen state.
[[nodiscard]] ScreenStateSnapshot read_screen_state() noexcept;

// Set the current screen. Called by Z05 and Z14 to drive transitions.
// When transitioning into Game, the active_scenario must be provided.
// When transitioning into any other screen, active_scenario should
// be std::nullopt; the implementation clears it if provided.
void set_screen(ScreenId screen,
                std::optional<engine::ScenarioId> active_scenario) noexcept;

// Convenience: returns true if the current screen is Game or
// PostRound (i.e., a scenario is active or just completed).
[[nodiscard]] bool is_in_scenario() noexcept;

// Reset to initial state (Root, no scenario). Used by the integration
// test only.
void reset_for_testing() noexcept;

}  // namespace poker_trainer::backbone
