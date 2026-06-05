#include "math/keybinds.hpp"

#include "math/bet_size_buttons.hpp"
#include "math/input_boxes.hpp"
#include "math/interrogator.hpp"
#include "math/submission.hpp"

#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"
#include "backbone/modal_state.hpp"
#include "backbone/scenario_events.hpp"
#include "backbone/screen_state.hpp"
#include "engine/generator.hpp"
#include "engine/scenario.hpp"
#include "engine/scenario_id.hpp"
#include "settings/settings.hpp"

#include <cstddef>
#include <optional>

#include "bridge/focus_registry.hpp"
#include "bridge/game_launch.hpp"
#include "bridge/screen_dispatch.hpp"

namespace poker_trainer::interrogator {

backbone::FocusableId focus_target_for_digit(const InterrogatorState& state, int digit) noexcept {
    if (digit < 1 || digit > 6) {
        return backbone::kNoFocus;
    }
    const engine::InputId target = kDigitToInput[static_cast<std::size_t>(digit - 1)];
    if (target == engine::InputId::BetSize) {
        return state.bet_group.present ? state.bet_group.focus_id : backbone::kNoFocus;
    }
    // First box of that kind; multi-tier per-tier inputs resolve to tier 0.
    // SEAM(Z08): per-tier number-key targeting (a "current tier" cursor) is a
    // render/layout concern finalized with the Game screen in W3.
    for (const NumericBox& box : state.boxes) {
        if (box.input == target) {
            return box.focus_id;
        }
    }
    return backbone::kNoFocus;
}

namespace {

// True while the math interrogator owns keyboard input: the Game screen is
// active and no modal is open over it (modals pause the scenario and trap
// focus). The tutorial overlay sits above this in the router priority stack.
[[nodiscard]] bool game_input_active() {
    return backbone::read_screen_state().current == backbone::ScreenId::Game &&
           !backbone::is_any_modal_open();
}

[[nodiscard]] std::optional<int> digit_of(backbone::KeyCode code) noexcept {
    switch (code) {
        case backbone::KeyCode::Digit1: return 1;
        case backbone::KeyCode::Digit2: return 2;
        case backbone::KeyCode::Digit3: return 3;
        case backbone::KeyCode::Digit4: return 4;
        case backbone::KeyCode::Digit5: return 5;
        case backbone::KeyCode::Digit6: return 6;
        default: return std::nullopt;
    }
}

[[nodiscard]] settings::Settings settings_or_default(const InterrogatorRuntime& runtime) {
    return runtime.settings_source ? runtime.settings_source() : settings::Settings{};
}

// Resolve the authoritative ScenarioState for `id`. Single source of truth: the
// scenario is generated exactly once at launch and stored in the bridge, so read
// it back here -- no consumer regenerates in the normal flow.
//
// FALLBACK: when the bridge store holds no matching scenario, regenerate from the
// seed with the injected LIVE settings. This covers the shared-scenario boot path
// (Z05 enters Game directly with the URL id, without a launch -- out of scope to
// wire this window) and unit tests that drive the bus directly. The fallback uses
// the same live settings the launch generates under, so it is consistent with the
// authoritative state, not divergent.
[[nodiscard]] engine::ScenarioState resolve_scenario(const InterrogatorRuntime& runtime,
                                                     engine::ScenarioId id) {
    const engine::ScenarioState* active = bridge::active_scenario();
    if (active != nullptr && active->id == id) {
        return *active;
    }
    return engine::generate_scenario(id, settings_or_default(runtime));
}

// Populate the shared focus-reconciliation registry for the current inputs: each
// math box is a text field (grabs ImGui keyboard focus when focused), the bet-size
// group is a non-text stop (yields it). Z09 registers NO activate/adjust hooks --
// its Enter is the submit-all override and arrows are no-ops by spec, both handled
// by its own router registrations, not the substrate dispatch. Null registry only
// in unit tests that drive the zone without the bridge runtime.
void populate_focus_registry(InterrogatorRuntime& runtime) {
    if (runtime.focus_registry == nullptr) {
        return;
    }
    bridge::FocusRegistry& registry = *runtime.focus_registry;
    registry.clear();
    for (const NumericBox& box : runtime.state.boxes) {
        registry.register_element(box.focus_id, bridge::FocusableEntry{.is_text_field = true});
    }
    if (runtime.state.bet_group.present) {
        registry.register_element(runtime.state.bet_group.focus_id,
                                  bridge::FocusableEntry{.is_text_field = false});
    }
}

// Reconfigure Z09's inputs for `scenario` and (re)register the Game focus segment
// (boxes then the bet group) plus the shared focus registry. The single place a new
// scenario takes effect. Z08 composes the full list (segment then Shop/Help/
// Settings/X) in W3 — SEAM(Z08/Z11).
void apply_scenario(InterrogatorRuntime& runtime, const engine::ScenarioState& scenario) {
    configure_for_scenario(runtime.state, scenario);
    backbone::register_focus_list(backbone::ScreenId::Game, runtime.state.focus_segment);
    populate_focus_registry(runtime);
}

// Bring the cached scenario in sync with the active scenario id, (re)spawning
// inputs and registering the Game focus segment on change. Reads the single
// authoritative state from the bridge (resolve_scenario).
void sync_scenario(InterrogatorRuntime& runtime, engine::ScenarioId id) {
    const bool changed =
        !runtime.state.scenario.has_value() || runtime.state.scenario->id != id;
    if (!changed) {
        return;
    }
    apply_scenario(runtime, resolve_scenario(runtime, id));
}

// The Game-screen render hook (W2 placeholder). In W3 Z08 owns ScreenId::Game's
// renderer and calls render_math_inputs directly; this registration is replaced
// at that point (register_screen_renderer is last-writer-wins) — SEAM(Z08).
void game_render_hook(InterrogatorRuntime& runtime) {
    const backbone::ScreenStateSnapshot snap = backbone::read_screen_state();
    if (snap.current != backbone::ScreenId::Game || !snap.active_scenario.has_value()) {
        return;
    }
    sync_scenario(runtime, *snap.active_scenario);
    if (runtime.state.scenario.has_value()) {
        render_math_inputs(runtime, *runtime.state.scenario);
    }
}

bool on_enter_key(InterrogatorRuntime& runtime, const backbone::KeyEvent& e) {
    if (e.type != backbone::KeyEventType::KeyDown || e.code != backbone::KeyCode::Enter) {
        return false;
    }
    // Enter submits ALL visible inputs at once, regardless of which is focused
    // (overrides the standard "Enter activates the focused element" rule).
    // Tutorial override: suppress until every visible input across all tiers is
    // filled, then submit normally.
    const backbone::ScreenStateSnapshot snap = backbone::read_screen_state();
    const bool tutorial_active =
        snap.tutorial_state.phase == backbone::TutorialPhase::Active;
    if (tutorial_active && !all_visible_inputs_filled(runtime.state)) {
        return true;  // consumed; submission suppressed until all filled
    }
    (void)on_submit(runtime);
    // SEAM(Z14): trigger the Game->Post-Round slide transition here once Z14 owns
    // it. Enter now reaches this handler reliably even while a box is active — the
    // platform gate routes Tab/Enter/Escape through (bridge/input_routing.hpp);
    // per-screen Enter-to-activate handlers for Root / Mode Selection are Zone 07's.
    return true;
}

bool on_number_key(InterrogatorRuntime& runtime, const backbone::KeyEvent& e) {
    if (e.type != backbone::KeyEventType::KeyDown || e.mods != backbone::ModMask::None) {
        return false;
    }
    const std::optional<int> digit = digit_of(e.code);
    if (!digit.has_value()) {
        return false;
    }
    InterrogatorState& state = runtime.state;
    const bool bet_focused = state.bet_group.present &&
                             backbone::get_focused_element() == state.bet_group.focus_id;
    if (bet_focused) {
        // Keys 1-4 select the tier; the global 1-6 box-focus keybinds are
        // suppressed while the group has focus, so 5/6 are consumed no-ops.
        (void)select_bet_tier_by_digit(state.bet_group, *digit);
        return true;
    }
    const backbone::FocusableId target = focus_target_for_digit(state, *digit);
    if (target == backbone::kNoFocus) {
        return false;  // no such input in this scenario — leave the key unhandled
    }
    backbone::activate_keyboard_mode();
    backbone::snap_focus_to(target);
    return true;
}

}  // namespace

void install_interrogator(InterrogatorRuntime& runtime) {
    // Render hook: the Game screen's renderer (W2 placeholder; SEAM(Z08)).
    bridge::register_screen_renderer(backbone::ScreenId::Game,
                                     [&runtime] { game_render_hook(runtime); });

    // Key handlers (screen-context priority, gated to the active Game screen).
    backbone::register_key_handler(
        game_input_active,
        [&runtime](const backbone::KeyEvent& e) { return on_enter_key(runtime, e); },
        backbone::HandlerPriority::ScreenContext, "interrogator.enter");
    backbone::register_key_handler(
        game_input_active,
        [&runtime](const backbone::KeyEvent& e) { return on_number_key(runtime, e); },
        backbone::HandlerPriority::ScreenContext, "interrogator.number_keys");
    // Tab / Shift-Tab navigation is the backbone's universal focus handler; the
    // focus segment registered on scenario entry gives it the math + bet stops.
    // Backspace clears within the focused box via ImGui InputText (text entry is
    // SEAM(Z05)); arrow keys are intentionally not bound (math boxes are
    // unbounded decimals).

    // Reset + respawn inputs on a new scenario. Fired by the launch path
    // (bridge::request_game_launch) after it generates + stores the authoritative
    // ScenarioState; this handler reads that single state back (resolve_scenario)
    // and reconfigures + re-registers the focus segment.
    (void)backbone::subscribe_scenario_spawned(
        [&runtime](const backbone::ScenarioSpawnedEvent& ev) {
            apply_scenario(runtime, resolve_scenario(runtime, ev.scenario_id));
        },
        "interrogator.scenario_spawned");
}

}  // namespace poker_trainer::interrogator
