#pragma once

#include "engine/scenario_id.hpp"

#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

namespace poker_trainer::backbone {

// Scenario lifecycle event types.
enum class ScenarioEventType : std::uint8_t {
    // A new scenario has been generated and is about to be rendered.
    // Fired by Z01 after generation, before any rendering.
    ScenarioSpawned = 0,

    // The user has submitted their answers for grading. Fired by
    // Z09 when the submission gesture (Enter or click Submit) is
    // received. The grading happens after this event; consumers
    // can use this event to stop timers, animate chip resolution, etc.
    AnswersSubmitted = 1,

    // Grading has completed and the result is available. Fired by
    // Z01 after evaluating the user's answers against the scenario.
    GradingComplete = 2,

    // The user has clicked the Again button to start a new scenario.
    // Fired by Z13 on the commit click (double-confirm).
    AgainPressed = 3,

    // The user has exited the Post-Round Screen to Mode Selection.
    // Fired by Z13 when the Exit button is clicked.
    ExitToModeSelection = 4,
};

// Payload for ScenarioSpawned events.
struct ScenarioSpawnedEvent {
    engine::ScenarioId scenario_id;
};

// Payload for AnswersSubmitted events.
struct AnswersSubmittedEvent {
    engine::ScenarioId scenario_id;
    // The actual answer values are not in this payload; consumers
    // that need them query Z01's scenario state directly.
};

// Payload for GradingComplete events.
struct GradingCompleteEvent {
    engine::ScenarioId scenario_id;
    bool passed;
    std::uint32_t elapsed_ms;
    // Detailed grade breakdown is not in this payload; consumers
    // that need it query Z01's grading result directly.
};

// Payload for AgainPressed events.
struct AgainPressedEvent {
    // The just-completed scenario; a new scenario will be generated
    // in response to this event.
    engine::ScenarioId previous_scenario_id;
};

// Payload for ExitToModeSelection events.
struct ExitToModeSelectionEvent {
    engine::ScenarioId previous_scenario_id;
};

// Subscriber callbacks. One callback type per event payload.
using ScenarioSpawnedHandler =
    std::function<void(const ScenarioSpawnedEvent&)>;
using AnswersSubmittedHandler =
    std::function<void(const AnswersSubmittedEvent&)>;
using GradingCompleteHandler =
    std::function<void(const GradingCompleteEvent&)>;
using AgainPressedHandler =
    std::function<void(const AgainPressedEvent&)>;
using ExitToModeSelectionHandler =
    std::function<void(const ExitToModeSelectionEvent&)>;

// Opaque handle for an installed subscriber.
struct SubscriberHandle {
    std::uint64_t value{0};
    constexpr bool operator==(const SubscriberHandle&) const noexcept = default;
};

inline constexpr SubscriberHandle kInvalidSubscriberHandle{0};

// Subscribe to ScenarioSpawned events. Returns a handle that can
// be used to unsubscribe. The tag is for debugging.
SubscriberHandle subscribe_scenario_spawned(
    ScenarioSpawnedHandler handler, std::string_view tag) noexcept;

SubscriberHandle subscribe_answers_submitted(
    AnswersSubmittedHandler handler, std::string_view tag) noexcept;

SubscriberHandle subscribe_grading_complete(
    GradingCompleteHandler handler, std::string_view tag) noexcept;

SubscriberHandle subscribe_again_pressed(
    AgainPressedHandler handler, std::string_view tag) noexcept;

SubscriberHandle subscribe_exit_to_mode_selection(
    ExitToModeSelectionHandler handler, std::string_view tag) noexcept;

// Unsubscribe a previously-installed subscriber. The handle is
// invalidated by this call. Calling unsubscribe on an invalid
// handle is a no-op.
void unsubscribe(SubscriberHandle handle) noexcept;

// Fire events. Called by the zones that produce each event type.
void fire_scenario_spawned(const ScenarioSpawnedEvent& event) noexcept;
void fire_answers_submitted(const AnswersSubmittedEvent& event) noexcept;
void fire_grading_complete(const GradingCompleteEvent& event) noexcept;
void fire_again_pressed(const AgainPressedEvent& event) noexcept;
void fire_exit_to_mode_selection(
    const ExitToModeSelectionEvent& event) noexcept;

// Clear all subscribers. Used by the integration test only.
void reset_scenario_events_for_testing() noexcept;

}  // namespace poker_trainer::backbone
