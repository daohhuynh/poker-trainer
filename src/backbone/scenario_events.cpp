#include "backbone/scenario_events.hpp"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <utility>
#include <vector>

// Threading model:
// - subscribe_X, unsubscribe, and fire_X are all safe to call from any thread.
// - Mutexes are per-event-type; concurrent fires of different event types do not contend.
// - Callbacks are invoked on the firing thread. Subscribers whose callbacks may be invoked
//   from non-main threads (e.g., audio thread firing ScenarioSpawned for SFX scheduling)
//   must ensure their callback bodies are thread-safe with respect to the subscriber's
//   own state.
// - Callbacks may reentrantly call subscribe_X or unsubscribe without deadlock; the copy-
//   out-then-fire pattern releases the mutex before invoking callbacks.
// - SubscriberHandle.value encodes event type in upper 8 bits + monotonic counter in
//   lower 56 bits; this makes handles self-describing for unsubscribe routing.

namespace poker_trainer::backbone {

namespace {

template <typename HandlerT>
struct Subscriber {
    SubscriberHandle handle;
    HandlerT handler;
};

std::atomic<std::uint64_t> g_subscriber_counter{1};

std::vector<Subscriber<ScenarioSpawnedHandler>> g_scenario_spawned_subscribers;
std::vector<Subscriber<AnswersSubmittedHandler>> g_answers_submitted_subscribers;
std::vector<Subscriber<GradingCompleteHandler>> g_grading_complete_subscribers;
std::vector<Subscriber<AgainPressedHandler>> g_again_pressed_subscribers;
std::vector<Subscriber<ExitToModeSelectionHandler>> g_exit_to_mode_selection_subscribers;

std::mutex g_scenario_spawned_mutex;
std::mutex g_answers_submitted_mutex;
std::mutex g_grading_complete_mutex;
std::mutex g_again_pressed_mutex;
std::mutex g_exit_to_mode_selection_mutex;

constexpr std::uint8_t event_type_from_handle(SubscriberHandle h) noexcept {
    return static_cast<std::uint8_t>(h.value >> 56);
}

constexpr std::uint64_t make_handle_value(ScenarioEventType type,
                                          std::uint64_t counter) noexcept {
    return (static_cast<std::uint64_t>(type) << 56) | counter;
}

template <typename HandlerT>
SubscriberHandle do_subscribe(std::vector<Subscriber<HandlerT>>& vec,
                              std::mutex& mtx,
                              ScenarioEventType type,
                              HandlerT handler) noexcept {
    const std::uint64_t counter =
        g_subscriber_counter.fetch_add(1, std::memory_order_relaxed);
    const SubscriberHandle h{make_handle_value(type, counter)};
    std::lock_guard<std::mutex> lock(mtx);
    vec.push_back({h, std::move(handler)});
    return h;
}

template <typename HandlerT>
void do_unsubscribe(std::vector<Subscriber<HandlerT>>& vec,
                    std::mutex& mtx,
                    SubscriberHandle h) noexcept {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = std::find_if(vec.begin(), vec.end(),
        [h](const Subscriber<HandlerT>& s) { return s.handle.value == h.value; });
    if (it != vec.end()) {
        vec.erase(it);
    }
}

template <typename HandlerT, typename EventT>
void do_fire(std::vector<Subscriber<HandlerT>>& vec,
             std::mutex& mtx,
             const EventT& event) noexcept {
    std::vector<Subscriber<HandlerT>> snapshot;
    {
        std::lock_guard<std::mutex> lock(mtx);
        snapshot = vec;
    }
    for (const auto& sub : snapshot) {
        sub.handler(event);
    }
}

}  // namespace

SubscriberHandle subscribe_scenario_spawned(
    ScenarioSpawnedHandler handler, std::string_view /*tag*/) noexcept {
    return do_subscribe(g_scenario_spawned_subscribers,
                        g_scenario_spawned_mutex,
                        ScenarioEventType::ScenarioSpawned,
                        std::move(handler));
}

SubscriberHandle subscribe_answers_submitted(
    AnswersSubmittedHandler handler, std::string_view /*tag*/) noexcept {
    return do_subscribe(g_answers_submitted_subscribers,
                        g_answers_submitted_mutex,
                        ScenarioEventType::AnswersSubmitted,
                        std::move(handler));
}

SubscriberHandle subscribe_grading_complete(
    GradingCompleteHandler handler, std::string_view /*tag*/) noexcept {
    return do_subscribe(g_grading_complete_subscribers,
                        g_grading_complete_mutex,
                        ScenarioEventType::GradingComplete,
                        std::move(handler));
}

SubscriberHandle subscribe_again_pressed(
    AgainPressedHandler handler, std::string_view /*tag*/) noexcept {
    return do_subscribe(g_again_pressed_subscribers,
                        g_again_pressed_mutex,
                        ScenarioEventType::AgainPressed,
                        std::move(handler));
}

SubscriberHandle subscribe_exit_to_mode_selection(
    ExitToModeSelectionHandler handler, std::string_view /*tag*/) noexcept {
    return do_subscribe(g_exit_to_mode_selection_subscribers,
                        g_exit_to_mode_selection_mutex,
                        ScenarioEventType::ExitToModeSelection,
                        std::move(handler));
}

void unsubscribe(SubscriberHandle h) noexcept {
    if (h.value == 0) return;
    switch (static_cast<ScenarioEventType>(event_type_from_handle(h))) {
        case ScenarioEventType::ScenarioSpawned:
            do_unsubscribe(g_scenario_spawned_subscribers,
                           g_scenario_spawned_mutex, h);
            break;
        case ScenarioEventType::AnswersSubmitted:
            do_unsubscribe(g_answers_submitted_subscribers,
                           g_answers_submitted_mutex, h);
            break;
        case ScenarioEventType::GradingComplete:
            do_unsubscribe(g_grading_complete_subscribers,
                           g_grading_complete_mutex, h);
            break;
        case ScenarioEventType::AgainPressed:
            do_unsubscribe(g_again_pressed_subscribers,
                           g_again_pressed_mutex, h);
            break;
        case ScenarioEventType::ExitToModeSelection:
            do_unsubscribe(g_exit_to_mode_selection_subscribers,
                           g_exit_to_mode_selection_mutex, h);
            break;
        default:
            // Unknown event type tag — handle was forged or corrupted; silently ignore.
            break;
    }
}

void fire_scenario_spawned(const ScenarioSpawnedEvent& event) noexcept {
    do_fire(g_scenario_spawned_subscribers, g_scenario_spawned_mutex, event);
}

void fire_answers_submitted(const AnswersSubmittedEvent& event) noexcept {
    do_fire(g_answers_submitted_subscribers, g_answers_submitted_mutex, event);
}

void fire_grading_complete(const GradingCompleteEvent& event) noexcept {
    do_fire(g_grading_complete_subscribers, g_grading_complete_mutex, event);
}

void fire_again_pressed(const AgainPressedEvent& event) noexcept {
    do_fire(g_again_pressed_subscribers, g_again_pressed_mutex, event);
}

void fire_exit_to_mode_selection(const ExitToModeSelectionEvent& event) noexcept {
    do_fire(g_exit_to_mode_selection_subscribers,
            g_exit_to_mode_selection_mutex, event);
}

void reset_scenario_events_for_testing() noexcept {
    {
        std::lock_guard<std::mutex> lock(g_scenario_spawned_mutex);
        g_scenario_spawned_subscribers.clear();
    }
    {
        std::lock_guard<std::mutex> lock(g_answers_submitted_mutex);
        g_answers_submitted_subscribers.clear();
    }
    {
        std::lock_guard<std::mutex> lock(g_grading_complete_mutex);
        g_grading_complete_subscribers.clear();
    }
    {
        std::lock_guard<std::mutex> lock(g_again_pressed_mutex);
        g_again_pressed_subscribers.clear();
    }
    {
        std::lock_guard<std::mutex> lock(g_exit_to_mode_selection_mutex);
        g_exit_to_mode_selection_subscribers.clear();
    }
    g_subscriber_counter.store(1, std::memory_order_relaxed);
}

}  // namespace poker_trainer::backbone
