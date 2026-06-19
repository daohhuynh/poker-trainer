#include "modal/confirm_modal.hpp"

#include "modal/modal_base.hpp"
#include "modal/modals.hpp"

#include "theme/theme_tokens.hpp"

#include <functional>

#include <imgui.h>

#include "bridge/focus_registry.hpp"

// Zone 11 — uniform confirmation modal render. No icon-pill header; the body text
// provides context. Yes (red, state_fail) on the left, No (grey, default focus) on
// the right (ARCHITECTURE leave-site: "red Yes (left), grey No (right, default
// keyboard focus)"); focus order is No -> Yes -> X (default No). Escape == No is
// handled by the ModalLayer key handler (modal_base.cpp).

namespace poker_trainer::modal {

void render_confirm_modal(const ConfirmSpec& spec) {
    const std::uint32_t ring =
        ImGui::ColorConvertFloat4ToU32(theme::get_color(theme::ColorToken::BorderFocus));

    const bool visible = modal_begin_centered("##confirm_modal", 0.32f, 0.22f);
    bool do_yes = false;
    bool do_close = false;
    if (visible) {
        const bool x_clicked = modal_draw_x_close(kConfirmClose);

        ImGui::Dummy(ImVec2{0.0f, 4.0f});
        ImGui::TextWrapped("%s", spec.body.c_str());
        ImGui::Dummy(ImVec2{0.0f, 12.0f});

        // Yes (left, red).
        ImGui::PushStyleColor(ImGuiCol_Button, theme::get_color(theme::ColorToken::StateFail));
        const bool yes_clicked = ImGui::Button("Yes");
        ImGui::PopStyleColor();
        bridge::draw_focus_ring(kConfirmYes, ring);

        ImGui::SameLine();
        const bool no_clicked = ImGui::Button("No");  // grey default-themed button
        bridge::draw_focus_ring(kConfirmNo, ring);

        if (yes_clicked) {
            do_yes = true;
        } else if (no_clicked || x_clicked || modal_click_outside_dismissed()) {
            do_close = true;
        }
    }
    modal_end();

    if (do_yes) {
        // Capture the action before close_modal clears the runtime's ConfirmSpec
        // (spec is a reference into it), then run it after the modal is closed.
        const std::function<void()> on_yes = spec.on_yes;
        close_modal();
        if (on_yes) {
            on_yes();
        }
    } else if (do_close) {
        close_modal();
    }
}

}  // namespace poker_trainer::modal
