#include "bridge/error_screen.hpp"

#include "theme/theme_tokens.hpp"

#include "imgui.h"

#include <emscripten/emscripten.h>

// Error-screen render body. Binding-adjacent (touches emscripten for the page
// reload), held to -Wall -Wextra -Werror in bridge_platform; compiled only into
// the wasm app.

namespace poker_trainer::bridge {

void render_error_screen() {
    const ImGuiIO& io = ImGui::GetIO();
    const float w = io.DisplaySize.x;
    const float h = io.DisplaySize.y;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(
        ImVec2(0.0f, 0.0f), ImVec2(w, h),
        ImGui::ColorConvertFloat4ToU32(
            theme::get_color(theme::ColorToken::BgPrimary)));

    const char* message = "Couldn't load. Check your connection and try again.";
    const ImVec2 text_size = ImGui::CalcTextSize(message);
    dl->AddText(ImVec2((w - text_size.x) * 0.5f, h * 0.40f),
                ImGui::ColorConvertFloat4ToU32(
                    theme::get_color(theme::ColorToken::TextPrimary)),
                message);

    // Retry button: themed ImGui widget hosted in a transparent, chrome-free,
    // full-canvas window so the button hit-test works.
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoFocusOnAppearing;

    ImGui::PushStyleColor(ImGuiCol_Text,
                          theme::get_color(theme::ColorToken::TextButton));
    ImGui::PushStyleColor(ImGuiCol_Button,
                          theme::get_color(theme::ColorToken::ButtonBg));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          theme::get_color(theme::ColorToken::ButtonBgHover));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          theme::get_color(theme::ColorToken::ButtonBgActive));

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    if (ImGui::Begin("##error_screen", nullptr, flags)) {
        const float button_w = w * 0.18f;
        const float button_h = h * 0.07f;
        ImGui::SetCursorPos(ImVec2((w - button_w) * 0.5f, h * 0.52f));
        if (ImGui::Button("Retry", ImVec2(button_w, button_h))) {
            emscripten_run_script("window.location.reload()");
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(4);
}

}  // namespace poker_trainer::bridge
