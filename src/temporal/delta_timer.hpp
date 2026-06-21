#pragma once

#include "engine/scenario.hpp"
#include "settings/settings.hpp"

#include <cstdint>
#include <functional>

// Zone 10 (Temporal) — the Delta Timer (Module 6).
//
// Tracks time since a scenario spawned, pauses while a modal is open over the Game
// screen, freezes on submit, and clears on exit. The per-frame logic is a pure
// `advance` over a plain TimerState value (so the modal-pause behavior is unit-
// testable without the clock, the bus, or ImGui); the lifecycle entry points and
// the single file-static instance live in delta_timer.cpp.

namespace poker_trainer::temporal {

// The Delta Timer's per-frame state. A plain value advanced once per frame.
struct TimerState {
    std::uint64_t target_ms{0};    // computed at timer_start; 0 == not started / cleared
    std::uint64_t elapsed_ms{0};   // scenario time so far; frozen on AnswersSubmitted
    std::uint64_t last_now_ms{0};  // animation-clock reading at the previous advance
    bool running{false};   // false before start, after submit, after exit, during tutorial
    bool paused{false};    // true while a modal is open over the scenario
    bool disabled{false};  // tutorial: the timer produces no valid result
};

// Pure per-frame advance. Adds the frame delta to elapsed_ms only while the timer
// is running, not paused, and not disabled, then recomputes the pause latch from the
// live modal state. The frame on which a modal opens still counts its own delta (the
// modal was not open on the prior frame); every later frame is frozen. A backwards
// clock reading clamps the delta to 0.
[[nodiscard]] TimerState advance(TimerState s, std::uint64_t now, bool modal_open) noexcept;

// ----- Contracted lifecycle (ZONES.md Z10 exports) -----

// Begin timing a freshly spawned scenario: compute its target from the live
// settings, reset elapsed to 0, start running. While the tutorial walkthrough is
// active the timer is disabled instead (no tracking, no overtime, no grade).
void timer_start(const engine::ScenarioState& scenario);

// Manual pause/resume. The automatic driver is the modal poll in timer_update;
// these are the contracted manual API and set the same pause latch.
void timer_pause() noexcept;
void timer_resume() noexcept;

// Scenario time so far, in milliseconds (the frozen final value after submit).
[[nodiscard]] std::uint64_t timer_elapsed_ms() noexcept;

// ----- Per-frame host + boot wiring -----

// Advance the file-static timer one frame: reads animation_clock for the current
// time and polls modal_state for the pause. Registered as a per-frame tick by
// install_temporal so it runs every frame regardless of countdown visibility (the
// time penalty applies even when the countdown is hidden).
void timer_update();

// Install Z10 at boot: wire the live-settings source, subscribe to the scenario
// lifecycle (start on spawn, freeze on submit, clear on exit), and register the
// per-frame timer_update tick. Call once at startup.
void install_temporal(std::function<settings::Settings()> settings_source);

// Inject the live-settings source on its own (install_temporal calls this). Unset
// -> Settings{} defaults, keeping the zone self-contained for tests.
void set_settings_source(std::function<settings::Settings()> settings_source);

// ----- Exposed time result (Z13 Time-Grade row + the Module-7 award seam) -----

[[nodiscard]] std::uint64_t target_time_ms() noexcept;   // the scenario's target
[[nodiscard]] std::uint64_t actual_time_ms() noexcept;   // == timer_elapsed_ms()
[[nodiscard]] bool is_overtime() noexcept;               // actual > target
[[nodiscard]] std::uint64_t overtime_ms() noexcept;      // actual - target, or 0
[[nodiscard]] bool time_within_target() noexcept;        // actual <= target (inclusive)
[[nodiscard]] bool time_result_valid() noexcept;         // false while tutorial-disabled

// Whether render_countdown() should draw this frame: the Show countdown timer
// setting is on, the timer is not tutorial-disabled, and a scenario is active.
[[nodiscard]] bool countdown_should_render();

// ----- Test seams (mirrors the repo's reset_*_for_testing convention) -----
void reset_timer_for_testing() noexcept;
void set_timer_state_for_testing(const TimerState& state) noexcept;
[[nodiscard]] TimerState timer_state_for_testing() noexcept;

}  // namespace poker_trainer::temporal
