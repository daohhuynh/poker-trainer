#include "screens/custom_popup.hpp"

#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"

#include <cstdint>
#include <cstdio>
#include <optional>
#include <string_view>

#include <imgui.h>

#include "bridge/focus_registry.hpp"
#include "bridge/game_launch.hpp"
#include "theme/theme_tokens.hpp"

namespace poker_trainer::screens {

namespace {

// Keystroke-level digit filter for the two weight inputs: discard any character
// other than 0-9 (per ARCHITECTURE / the coupled-input keystroke rule).
int digit_char_filter(ImGuiInputTextCallbackData* data) {
    if (data->EventChar < 256 && accepts_text_char(static_cast<char>(data->EventChar))) {
        return 0;  // keep
    }
    return 1;  // discard
}

void format_weight(std::array<char, 4>& buf, std::uint8_t value) {
    std::snprintf(buf.data(), buf.size(), "%d", static_cast<int>(value));
}

// Populate the shared focus registry with the popup's eight focusables and their
// reconcile + dispatch behavior. Called once per open from render_custom_popup
// (which holds both `state` and `store`, the closures' captures). The entries
// capture by reference and read live values, so a single build per open suffices.
//
//   * Two weight INPUTS — text fields (grab ImGui keyboard focus when focused).
//     adjust nudges the coupled weight via step_weight; the dispatch handler
//     forwards only Up/Down to it (Left/Right stay with ImGui's text cursor).
//   * Two SLIDERS — non-text stops (yield ImGui keyboard capture). adjust takes
//     all four arrows (Left/Down -1, Right/Up +1, via the substrate's mapping).
//   * Save / Reset / Play / X — non-text stops. activate fires the same action as
//     the mouse path; Play/X defer the close via request_close (see the field doc).
void populate_popup_registry(CustomPopupState& state, CustomWeightsStore& store) {
    if (state.focus_registry == nullptr) {
        return;
    }
    bridge::FocusRegistry& registry = *state.focus_registry;
    registry.clear();

    const auto adjust_aggressor = [&state](int delta) {
        state.weights = step_weight(state.weights, WeightField::Aggressor, delta);
    };
    const auto adjust_caller = [&state](int delta) {
        state.weights = step_weight(state.weights, WeightField::Caller, delta);
    };

    registry.register_element(
        kFocusAggressorInput,
        bridge::FocusableEntry{.is_text_field = true, .adjust = adjust_aggressor});
    registry.register_element(
        kFocusAggressorSlider,
        bridge::FocusableEntry{.is_text_field = false, .adjust = adjust_aggressor});
    registry.register_element(
        kFocusCallerInput,
        bridge::FocusableEntry{.is_text_field = true, .adjust = adjust_caller});
    registry.register_element(
        kFocusCallerSlider,
        bridge::FocusableEntry{.is_text_field = false, .adjust = adjust_caller});
    registry.register_element(
        kFocusSave, bridge::FocusableEntry{.is_text_field = false, .activate = [&state, &store] {
                        save_weights(store, state.weights);  // persist; no launch, no close
                    }});
    registry.register_element(
        kFocusReset, bridge::FocusableEntry{.is_text_field = false, .activate = [&state, &store] {
                         state.weights = reset_to_saved(store);  // restore; no close
                     }});
    registry.register_element(
        kFocusPlay, bridge::FocusableEntry{.is_text_field = false, .activate = [&state] {
                        // Defer BOTH the launch and the close out of this closure:
                        // request_game_launch would clear()+repopulate the shared
                        // registry (via Z09's scenario_spawned) while this very entry's
                        // std::function is mid-invocation. Capture the launch config and
                        // request the dismiss; dispatch_popup_key closes first, then
                        // launches, once this closure is off the stack.
                        state.pending_launch = state.weights;  // launch after close
                        state.request_close = true;            // deferred dismiss (no persist)
                    }});
    registry.register_element(
        kFocusClose, bridge::FocusableEntry{.is_text_field = false, .activate = [&state] {
                         state.request_close = true;  // deferred dismiss (no launch)
                     }});
}

// Mouse-click into a popup focusable: snap focus_manager onto it and engage
// keyboard mode, exactly as Z09's focus_box_on_click. Required (not just polish)
// under the reconcile: without it a click into an input leaves focus_manager on
// the prior (non-text) stop, whose every-frame YieldKeyboard would ClearActiveID
// the input the user just clicked into. Snapping keeps the outline, ImGui's text
// focus, and focus_manager in lockstep.
void focus_popup_on_click(backbone::FocusableId id) {
    backbone::activate_keyboard_mode();
    backbone::snap_focus_to(id);
}

// Route a key to the focused popup element through the shared substrate dispatch.
// On a text input, Left/Right are withheld so they reach ImGui's text cursor (the
// coupled solver only nudges on Up/Down); sliders/buttons take their dispatch keys
// normally. The deferred close (request_close) is performed here, after
// dispatch_focus_key returns, so close's registry clear() never runs while a
// Play/X activate closure is still on the stack.
bool dispatch_popup_key(CustomPopupState& state, const backbone::KeyEvent& e) {
    if (e.type != backbone::KeyEventType::KeyDown) {
        return false;
    }
    if (state.focus_registry == nullptr) {
        return false;  // unwired (native tests)
    }
    const bridge::FocusRegistry& registry = *state.focus_registry;
    const backbone::FocusableId focused = bridge::active_focus_or_none();
    if (registry.is_text_field(focused) && (e.code == backbone::KeyCode::ArrowLeft ||
                                            e.code == backbone::KeyCode::ArrowRight)) {
        return false;  // text-cursor keys belong to ImGui's InputText
    }
    const bool handled = bridge::dispatch_focus_key(registry, focused, e.code);
    if (state.request_close) {
        // Close FULLY (pop focus context + clear registry) first, then fire any
        // deferred Play launch — so Z09's scenario_spawned setup lands on the
        // restored base focus context and repopulates the registry, exactly as the
        // preset paths do. The launch runs here, after dispatch_focus_key returned,
        // so no registry entry closure is on the stack when the registry is cleared.
        const std::optional<backbone::CustomConfig> launch =
            take_pending_launch_and_close(state);
        if (launch.has_value()) {
            bridge::request_game_launch(backbone::GameMode::Custom, *launch);
        }
    }
    return handled;
}

}  // namespace

CustomConfig open_custom_popup() {
    // Open the modal: push its focus context (default pointer 0 = Aggressor input)
    // as a focus trap. The live, coupled, multi-frame popup runs via
    // render_custom_popup, which Zone 05's main loop drives with the
    // CustomPopupState and the persistence-backed CustomWeightsStore it owns.
    // Returns the starting weights for this popup session.
    push_custom_popup_focus();
    return CustomConfig{50, 50};
}

void render_custom_popup(CustomPopupState& state, CustomWeightsStore& store) {
    if (!state.open) {
        return;
    }

    // Build the popup's eight registry entries once per open, and forget the prior
    // session's reconcile target so this frame re-syncs ImGui to whatever focus
    // (the modal trap's initial element) focus_manager now reports.
    if (!state.focus_registered) {
        populate_popup_registry(state, store);
        state.last_synced_focus = backbone::kNoFocus;
        state.focus_registered = true;
    }

    // Reconcile ImGui's keyboard focus to focus_manager via the shared substrate
    // (identical to Z09's render_math_inputs). A text input that just gained focus
    // grabs ImGui text focus (grab_keyboard_if_target, below); a slider/button (a
    // non-text stop) yields ImGui keyboard capture so arrows reach the dispatch
    // handler. begin_focus_reconcile applies the once-per-frame ClearActiveID. The
    // registry is null only in tests that never render; it then degrades to no
    // reconcile (and the rings, drawn from focus_manager, still appear in-browser).
    const bridge::FocusReconcile rec =
        state.focus_registry != nullptr
            ? bridge::begin_focus_reconcile(*state.focus_registry, state.last_synced_focus)
            : bridge::FocusReconcile{};
    const std::uint32_t ring_color =
        ImGui::ColorConvertFloat4ToU32(theme::get_color(theme::ColorToken::BorderFocus));

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2{vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f},
                            ImGuiCond_Appearing, ImVec2{0.5f, 0.5f});
    ImGui::SetNextWindowSize(ImVec2{vp->Size.x * 0.34f, vp->Size.y * 0.40f}, ImGuiCond_Appearing);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar;

    // ImGui widgets read the themed style (Z06 maps tokens -> ImGui color slots),
    // so input fields, sliders, and buttons pick up bg_input / accent_primary /
    // bg_button_default automatically; no literal colors here. The 2px border_focus
    // ring around the focused element is drawn by the substrate (draw_focus_ring),
    // from the same focus_manager source as Z09.
    bool dismissed = false;
    if (ImGui::Begin("##custom_popup", nullptr, flags)) {
        // X close (top-right of the popup): dismiss without launching.
        const float close_w = ImGui::GetContentRegionAvail().x;
        ImGui::Dummy(ImVec2{
            close_w - ImGui::CalcTextSize("X").x - ImGui::GetStyle().FramePadding.x * 2.0f, 0.0f});
        ImGui::SameLine();
        if (ImGui::Button("X")) {
            dismissed = true;
        }
        bridge::draw_focus_ring(kFocusClose, ring_color);

        // --- Aggressor row: label + integer input (0-100) + % ---
        format_weight(state.aggressor_buf, state.weights.aggressor_weight);
        ImGui::TextUnformatted("Aggressor");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(vp->Size.x * 0.06f);
        bridge::grab_keyboard_if_target(rec, kFocusAggressorInput);
        if (ImGui::InputText("##aggr_input", state.aggressor_buf.data(), state.aggressor_buf.size(),
                             ImGuiInputTextFlags_CallbackCharFilter, &digit_char_filter)) {
            state.weights = solve_from(
                WeightField::Aggressor,
                parse_clamped_weight(std::string_view{state.aggressor_buf.data()}));
        }
        if (ImGui::IsItemClicked()) {
            focus_popup_on_click(kFocusAggressorInput);
        }
        bridge::draw_focus_ring(kFocusAggressorInput, ring_color);
        ImGui::SameLine();
        ImGui::TextUnformatted("%");

        // Aggressor slider (full modal width).
        // SEAM(visual-pass): per Color Tint Theme — Token Application Rules the
        // sliders should read as track=border_default, fill=accent_primary,
        // handle=bg_button_default, with the redundant centered value dropped (the %
        // field already shows it). ImGui's SliderInt has no native fill bar, so this
        // needs a custom token-colored draw and is deferred to the visual pass.
        {
            int aggr = state.weights.aggressor_weight;
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::SliderInt("##aggr_slider", &aggr, 0, 100)) {
                state.weights = solve_from(WeightField::Aggressor, aggr);
            }
            if (ImGui::IsItemClicked()) {
                focus_popup_on_click(kFocusAggressorSlider);
            }
            bridge::draw_focus_ring(kFocusAggressorSlider, ring_color);
        }

        // --- Caller row + slider (identical structure) ---
        format_weight(state.caller_buf, state.weights.caller_weight);
        ImGui::TextUnformatted("Caller");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(vp->Size.x * 0.06f);
        bridge::grab_keyboard_if_target(rec, kFocusCallerInput);
        if (ImGui::InputText("##caller_input", state.caller_buf.data(), state.caller_buf.size(),
                             ImGuiInputTextFlags_CallbackCharFilter, &digit_char_filter)) {
            state.weights = solve_from(
                WeightField::Caller, parse_clamped_weight(std::string_view{state.caller_buf.data()}));
        }
        if (ImGui::IsItemClicked()) {
            focus_popup_on_click(kFocusCallerInput);
        }
        bridge::draw_focus_ring(kFocusCallerInput, ring_color);
        ImGui::SameLine();
        ImGui::TextUnformatted("%");

        {
            int caller = state.weights.caller_weight;
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::SliderInt("##caller_slider", &caller, 0, 100)) {
                state.weights = solve_from(WeightField::Caller, caller);
            }
            if (ImGui::IsItemClicked()) {
                focus_popup_on_click(kFocusCallerSlider);
            }
            bridge::draw_focus_ring(kFocusCallerSlider, ring_color);
        }

        // --- Action row: Save / Reset / Play ---
        if (ImGui::Button("Save")) {
            // Persist current weights. Does not launch, does not close.
            save_weights(store, state.weights);
            focus_popup_on_click(kFocusSave);
        }
        bridge::draw_focus_ring(kFocusSave, ring_color);
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            // Restore last-saved (or 50/50). Does not launch, does not close.
            state.weights = reset_to_saved(store);
            focus_popup_on_click(kFocusReset);
        }
        bridge::draw_focus_ring(kFocusReset, ring_color);
        ImGui::SameLine();
        if (ImGui::Button("Play")) {
            // Launch with current weights WITHOUT persisting (Custom launch path).
            // Defer the launch: the dismiss block below closes the popup FULLY first,
            // then fires it — so Z09's scenario_spawned setup starts from the same
            // clean focus context + registry the preset Mode buttons produce. Firing
            // request_game_launch here (before close) would let close_custom_popup's
            // clear() stomp the registry Z09 just populated, and pop_focus_context
            // discard the Game focus list Z09 registered into the live popup context.
            state.pending_launch = state.weights;  // launched after close (below)
            dismissed = true;
        }
        bridge::draw_focus_ring(kFocusPlay, ring_color);

        // Click-outside dismissal — but never the click that opened the popup.
        // While the opening-frame guard is raised, keep it raised until the opening
        // mouse button is released, then arm outside-dismiss for any subsequent
        // click. "Outside" is tested via ImGui::GetIO().WantCaptureMouse: it is true
        // whenever the cursor is over the modal window OR an ImGui item is active,
        // so a click on the slider / inputs / buttons (which makes that widget the
        // active item) is correctly NOT treated as outside. IsWindowHovered cannot
        // be used here — it returns false while any item is active, so it misreads
        // an in-modal widget click as outside and self-dismisses the popup. The
        // bridge applies the same WantCaptureMouse gate to the event router
        // (platform.cpp::router_should_see_mouse), so screen handlers beneath the
        // modal never see these clicks either. See CustomPopupState::just_opened for
        // the opening-frame guard.
        if (state.just_opened) {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                state.just_opened = false;
            }
        } else if (!ImGui::GetIO().WantCaptureMouse &&
                   ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            dismissed = true;
        }
    }
    ImGui::End();

    if (dismissed) {
        // X / click-outside / Play -> dismiss (no persist). Close FULLY first, then
        // fire any deferred Play launch (pending_launch) so the Game screen starts
        // from the restored base focus context + a freshly repopulated registry —
        // the same clean slate the preset Mode buttons give it.
        const std::optional<backbone::CustomConfig> launch =
            take_pending_launch_and_close(state);
        if (launch.has_value()) {
            bridge::request_game_launch(backbone::GameMode::Custom, *launch);
        }
        return;  // registry cleared + context popped; nothing left to reconcile this frame
    }

    // Record the element ImGui is now reconciled to (after any click this frame
    // moved both the outline and ImGui's text focus together), so the next frame
    // acts only on a fresh change -- never re-grabbing focus already in sync, which
    // would trap the caret. Mirrors render_math_inputs.
    state.last_synced_focus = bridge::active_focus_or_none();
}

void install_custom_popup_handlers(CustomPopupState& state) {
    // Modal-layer Escape capture: dismiss the popup without launching. Sits above
    // the Mode Selection screen's Escape handler so the screen never sees Escape
    // while the popup is open (Notes — Escape Key Behavior; the modal captures it).
    backbone::register_key_handler(
        [&state] { return state.open; },
        [&state](const backbone::KeyEvent& e) {
            if (e.type == backbone::KeyEventType::KeyDown && e.code == backbone::KeyCode::Escape) {
                close_custom_popup(state);
                return true;
            }
            return false;
        },
        backbone::HandlerPriority::ModalLayer, "custom_popup.escape");

    // Modal-layer focus dispatch: route arrows (slider/input adjust) and
    // Space/Enter (Save/Reset/Play/X activate) to the focused element through the
    // shared substrate (dispatch_focus_key over the registry built in
    // render_custom_popup). Open-gated and beside Escape, above the Mode Selection
    // screen handlers. This is the popup's ONLY dispatch; it adds no per-element
    // routing of its own. The arrow keys reach here even while an InputText is
    // active because the keyboard router gate exempts them
    // (bridge/input_routing.hpp); dispatch_popup_key withholds Left/Right from a
    // focused text input so the ImGui text cursor still moves.
    backbone::register_key_handler(
        [&state] { return state.open; },
        [&state](const backbone::KeyEvent& e) { return dispatch_popup_key(state, e); },
        backbone::HandlerPriority::ModalLayer, "custom_popup.dispatch");
}

}  // namespace poker_trainer::screens
