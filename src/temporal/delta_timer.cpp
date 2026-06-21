#include "temporal/delta_timer.hpp"

#include "temporal/target_time.hpp"

#include "backbone/animation_clock.hpp"
#include "backbone/modal_state.hpp"
#include "backbone/scenario_events.hpp"
#include "backbone/screen_state.hpp"

#include "settings/settings.hpp"

#include <cstdint>
#include <functional>
#include <utility>

#include "bridge/frame_tick.hpp"
#include "bridge/game_launch.hpp"

namespace poker_trainer::temporal {

namespace {

// The single Delta Timer instance + the injected live-settings provider. This is
// the zone's only mutable state, owned here as a Z10-internal singleton (not a
// backbone primitive) exactly as the bridge owns the active scenario.
TimerState g_timer{};
std::function<settings::Settings()> g_settings_source;

[[nodiscard]] std::uint64_t now_ms() noexcept {
    return backbone::total_ms_since_app_start();
}

[[nodiscard]] settings::Settings current_settings() {
    return g_settings_source ? g_settings_source() : settings::Settings{};
}

}  // namespace

TimerState advance(TimerState s, std::uint64_t now, bool modal_open) noexcept {
    // Clamp against a non-monotonic clock reading so accumulation never underflows.
    const std::uint64_t delta = (now >= s.last_now_ms) ? (now - s.last_now_ms) : 0ULL;
    if (s.running && !s.paused && !s.disabled) {
        s.elapsed_ms += delta;
    }
    s.paused = s.running && modal_open;
    s.last_now_ms = now;
    return s;
}

void timer_start(const engine::ScenarioState& scenario) {
    const settings::Settings s = current_settings();
    g_timer.target_ms = target_for_scenario(scenario, s.gameplay);
    g_timer.elapsed_ms = 0ULL;
    g_timer.paused = false;
    g_timer.last_now_ms = now_ms();

    const bool tutorial_active =
        backbone::read_screen_state().tutorial_state.phase == backbone::TutorialPhase::Active;
    g_timer.disabled = tutorial_active;
    g_timer.running = !tutorial_active;
}

void timer_pause() noexcept { g_timer.paused = true; }

void timer_resume() noexcept {
    // Refresh the reference time so the paused interval is not back-counted on the
    // next advance (in the modal-driven path advance refreshes it every frame anyway).
    g_timer.last_now_ms = now_ms();
    g_timer.paused = false;
}

std::uint64_t timer_elapsed_ms() noexcept { return g_timer.elapsed_ms; }

void timer_update() {
    g_timer = advance(g_timer, now_ms(), backbone::is_any_modal_open());
}

void install_temporal(std::function<settings::Settings()> settings_source) {
    set_settings_source(std::move(settings_source));

    // Start on spawn. The bus event carries only the id; the full state is the single
    // authoritative ScenarioState the bridge holds (the documented consumer path,
    // mirroring Z03 audio) — request_game_launch stores it before firing the event.
    (void)backbone::subscribe_scenario_spawned(
        [](const backbone::ScenarioSpawnedEvent&) {
            if (const engine::ScenarioState* s = bridge::active_scenario(); s != nullptr) {
                timer_start(*s);
            }
        },
        "temporal.scenario_spawned");

    // Freeze on submit: elapsed_ms becomes the scenario's final actual time.
    (void)backbone::subscribe_answers_submitted(
        [](const backbone::AnswersSubmittedEvent&) { g_timer.running = false; },
        "temporal.answers_submitted");

    // Clear on exit to Mode Selection (Again re-starts via a fresh ScenarioSpawned).
    (void)backbone::subscribe_exit_to_mode_selection(
        [](const backbone::ExitToModeSelectionEvent&) { g_timer = TimerState{}; },
        "temporal.exit_to_mode_selection");

    bridge::register_frame_tick([] { timer_update(); });
}

void set_settings_source(std::function<settings::Settings()> settings_source) {
    g_settings_source = std::move(settings_source);
}

std::uint64_t target_time_ms() noexcept { return g_timer.target_ms; }

std::uint64_t actual_time_ms() noexcept { return timer_elapsed_ms(); }

bool is_overtime() noexcept { return g_timer.elapsed_ms > g_timer.target_ms; }

std::uint64_t overtime_ms() noexcept {
    return is_overtime() ? (g_timer.elapsed_ms - g_timer.target_ms) : 0ULL;
}

bool time_within_target() noexcept { return g_timer.elapsed_ms <= g_timer.target_ms; }

bool time_result_valid() noexcept { return !g_timer.disabled; }

bool countdown_should_render() {
    if (g_timer.disabled || g_timer.target_ms == 0ULL) {
        return false;
    }
    return current_settings().gameplay.show_countdown;
}

void reset_timer_for_testing() noexcept {
    g_timer = TimerState{};
    g_settings_source = nullptr;
}

void set_timer_state_for_testing(const TimerState& state) noexcept { g_timer = state; }

TimerState timer_state_for_testing() noexcept { return g_timer; }

}  // namespace poker_trainer::temporal
