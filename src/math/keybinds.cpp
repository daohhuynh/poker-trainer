#include "math/keybinds.hpp"

#include "math/bet_size_buttons.hpp"
#include "math/input_boxes.hpp"
#include "math/interrogator.hpp"
#include "math/submission.hpp"
#include "math/tier_flow.hpp"

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
#include <vector>

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
    // The box of that kind on the CURRENT screen. In a sequential multi-tier
    // Aggressor that is the current tier's Fold / EV (so "5"/"4" focus THIS tier's
    // inputs, not tier 0); for Caller / single-tier it is the sole such box.
    for (const NumericBox* box : current_view_boxes(state)) {
        if (box->input == target) {
            return box->focus_id;
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
    for (const NumericBox* box : current_view_boxes(runtime.state)) {
        registry.register_element(box->focus_id, bridge::FocusableEntry{.is_text_field = true});
    }
    if (runtime.state.bet_group.present) {
        registry.register_element(runtime.state.bet_group.focus_id,
                                  bridge::FocusableEntry{.is_text_field = false});
    }
    // Z08's persistent-cluster tail (Shop/Help/Settings/X): each a NON-text focus
    // stop with no activate hook (opening their modals is the Z11 no-op seam).
    // Registering them non-text makes the reconcile substrate release the text caret
    // (ClearActiveID) when Tab lands on one. Empty before Z08 installs / in tests.
    for (const backbone::FocusableId id : runtime.cluster_focus_tail) {
        registry.register_element(id, bridge::FocusableEntry{.is_text_field = false});
    }
}

// Register the Game screen's full focus list: Z09's math segment (boxes then the
// bet group) followed by Z08's persistent-cluster tail (Shop/Help/Settings/X). The
// segment stays math-only in state.focus_segment (so focus_in_math_zone treats the
// cluster icons as OUTSIDE the math zone); the tail is appended only here, at
// registration, so Tab reaches the cluster and wraps from X back to the first input.
void register_game_focus_list(const InterrogatorRuntime& runtime) {
    std::vector<backbone::FocusableId> full = runtime.state.focus_segment;
    full.insert(full.end(), runtime.cluster_focus_tail.begin(), runtime.cluster_focus_tail.end());
    backbone::register_focus_list(backbone::ScreenId::Game, full);
}

// Reconfigure Z09's inputs for `scenario` and (re)register the Game focus list
// (math segment then the Z08 cluster tail) plus the shared focus registry. The
// single place a new scenario takes effect.
void apply_scenario(InterrogatorRuntime& runtime, const engine::ScenarioState& scenario) {
    configure_for_scenario(runtime.state, scenario);
    register_game_focus_list(runtime);
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

// Submit the gathered answers (on_submit fires the bus events + grades).
// SEAM(Z14): trigger the Game->Post-Round slide transition here once Z14 owns it.
// Enter reaches this handler reliably even while a box is active -- the platform
// gate routes Tab/Enter/Escape through (bridge/input_routing.hpp); per-screen
// Enter-to-activate handlers for Root / Mode Selection are Zone 07's.
void do_submit(InterrogatorRuntime& runtime) {
    (void)on_submit(runtime);
}

// Advance a sequential multi-tier Aggressor to the next tier screen (forward-only).
// Re-registers the new tier's focus list + registry, carries the persistent Bet
// Size pick (NOT reset), and lands default focus on the new tier's first input
// (Fold Probability). The per-tier typed answers stay in `state.boxes`, so the
// submitted set remains identical to the all-at-once shape Z01 grades.
void advance_tier(InterrogatorRuntime& runtime) {
    InterrogatorState& state = runtime.state;
    const std::uint8_t next = next_tier(state.current_tier);
    if (next == state.current_tier) {
        return;  // already on the last tier: forward-only, no wrap, no revisit
    }
    state.current_tier = next;
    state.focus_segment = build_focus_segment(state);
    register_game_focus_list(runtime);
    populate_focus_registry(runtime);
    state.last_synced_focus = backbone::kNoFocus;
    if (!state.focus_segment.empty()) {
        backbone::snap_focus_to(state.focus_segment.front());  // default focus = Fold Probability
    }
}

// True when keyboard focus is in Z09's math-input zone (a numeric box or the bet-
// size group) -- where Enter advances/submits. kNoFocus (an unarmed context) counts
// as the zone (no cluster icon is focused). A focused element outside the segment
// is a Z08/Z11 cluster icon (SEAM): Enter must activate it, not advance the tier.
[[nodiscard]] bool focus_in_math_zone(const InterrogatorState& state) {
    const backbone::FocusableId focused = backbone::get_focused_element();
    if (focused == backbone::kNoFocus) {
        return true;
    }
    for (const backbone::FocusableId id : state.focus_segment) {
        if (id == focused) {
            return true;
        }
    }
    return false;
}

bool on_enter_key(InterrogatorRuntime& runtime, const backbone::KeyEvent& e) {
    if (e.type != backbone::KeyEventType::KeyDown || e.code != backbone::KeyCode::Enter) {
        return false;
    }
    InterrogatorState& state = runtime.state;

    // Multi-tier Aggressor: SEQUENTIAL per-tier flow. Enter advances tier-by-tier
    // and submits on the last tier, gated on the CURRENT tier's required inputs
    // (Fold + EV, plus the tier-1 Equity-if-Called); the Bet Size pick never gates.
    // Enter advances/submits only from the math-input zone -- on a cluster icon Enter
    // activates that icon, so leave it unconsumed there. This also subsumes the
    // tutorial "Enter does nothing until this tier's inputs are filled" rule, since
    // an unfilled tier yields EnterAction::None for tutorial and gameplay alike.
    if (is_sequential(state)) {
        if (!focus_in_math_zone(state)) {
            return false;
        }
        switch (enter_action(state)) {
            case EnterAction::None:
                return true;  // this tier's required inputs unfilled: consumed no-op
            case EnterAction::Advance:
                advance_tier(runtime);
                return true;
            case EnterAction::Submit:
                do_submit(runtime);
                return true;
        }
        return true;  // exhaustive switch above; keeps -Wreturn-type quiet
    }

    // Caller / single-tier Aggressor: one screen, Enter submits ALL visible inputs
    // at once (overrides "Enter activates the focused element"). But Enter on a
    // focused cluster icon must NOT submit — leave it unconsumed so the icon's (Z11
    // no-op) activation owns it; only Enter from the math zone submits.
    if (!focus_in_math_zone(state)) {
        return false;
    }
    // Tutorial override: suppress until every visible input is filled, then submit.
    const backbone::ScreenStateSnapshot snap = backbone::read_screen_state();
    const bool tutorial_active =
        snap.tutorial_state.phase == backbone::TutorialPhase::Active;
    if (tutorial_active && !all_visible_inputs_filled(state)) {
        return true;  // consumed; submission suppressed until all filled
    }
    do_submit(runtime);
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
