#include "screens/custom_popup.hpp"

#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"

#include <cstdio>
#include <string_view>

#include <imgui.h>

#include "bridge/game_launch.hpp"

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

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2{vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f},
                            ImGuiCond_Appearing, ImVec2{0.5f, 0.5f});
    ImGui::SetNextWindowSize(ImVec2{vp->Size.x * 0.34f, vp->Size.y * 0.40f}, ImGuiCond_Appearing);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar;

    // ImGui widgets read the themed style (Z06 maps tokens -> ImGui color slots),
    // so input fields, sliders, and buttons pick up bg_input / accent_primary /
    // bg_button_default automatically; no literal colors here.
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

        // --- Aggressor row: label + integer input (0-100) + % ---
        format_weight(state.aggressor_buf, state.weights.aggressor_weight);
        ImGui::TextUnformatted("Aggressor");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(vp->Size.x * 0.06f);
        if (ImGui::InputText("##aggr_input", state.aggressor_buf.data(), state.aggressor_buf.size(),
                             ImGuiInputTextFlags_CallbackCharFilter, &digit_char_filter)) {
            state.weights = solve_from(
                WeightField::Aggressor,
                parse_clamped_weight(std::string_view{state.aggressor_buf.data()}));
        }
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
        }

        // --- Caller row + slider (identical structure) ---
        format_weight(state.caller_buf, state.weights.caller_weight);
        ImGui::TextUnformatted("Caller");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(vp->Size.x * 0.06f);
        if (ImGui::InputText("##caller_input", state.caller_buf.data(), state.caller_buf.size(),
                             ImGuiInputTextFlags_CallbackCharFilter, &digit_char_filter)) {
            state.weights = solve_from(
                WeightField::Caller, parse_clamped_weight(std::string_view{state.caller_buf.data()}));
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("%");

        {
            int caller = state.weights.caller_weight;
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::SliderInt("##caller_slider", &caller, 0, 100)) {
                state.weights = solve_from(WeightField::Caller, caller);
            }
        }

        // --- Action row: Save / Reset / Play ---
        if (ImGui::Button("Save")) {
            // Persist current weights. Does not launch, does not close.
            save_weights(store, state.weights);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            // Restore last-saved (or 50/50). Does not launch, does not close.
            state.weights = reset_to_saved(store);
        }
        ImGui::SameLine();
        if (ImGui::Button("Play")) {
            // Launch with current weights WITHOUT persisting (Custom launch path:
            // request_game_launch(Custom, currentWeights)).
            bridge::request_game_launch(backbone::GameMode::Custom, state.weights);
            dismissed = true;
        }

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
        close_custom_popup(state);  // X / click-outside / Play -> dismiss (no persist).
    }
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
}

}  // namespace poker_trainer::screens
