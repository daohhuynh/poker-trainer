// Z06 ImGui style bridge implementation.
//
// Maps the active theme's semantic ColorTokens onto Dear ImGui's style color
// slots. Only slots the trainer actually relies on are set; the keyboard focus
// indicator is intentionally not mapped here because it is custom-drawn by the
// rendering layer from focus_manager state, not by ImGui's nav highlight.

#include "theme/imgui_style_refresh.hpp"

#include "theme/theme.hpp"
#include "theme/theme_tokens.hpp"

#include <imgui.h>

namespace poker_trainer::theme {

void apply_theme_to_imgui_style(ImGuiStyle& style) noexcept {
    const auto set = [&style](ImGuiCol slot, ColorToken token) noexcept {
        style.Colors[slot] = get_color(token);
    };

    // Text.
    set(ImGuiCol_Text, ColorToken::TextPrimary);
    set(ImGuiCol_TextDisabled, ColorToken::TextDisabled);

    // Window / popup surfaces map to the modal panel fill.
    set(ImGuiCol_WindowBg, ColorToken::BgModalSurface);
    set(ImGuiCol_ChildBg, ColorToken::BgModalSurface);
    set(ImGuiCol_PopupBg, ColorToken::BgModalSurface);
    set(ImGuiCol_TitleBg, ColorToken::BgModalSurface);
    set(ImGuiCol_TitleBgActive, ColorToken::BgModalSurface);
    set(ImGuiCol_TitleBgCollapsed, ColorToken::BgModalSurface);
    set(ImGuiCol_ScrollbarBg, ColorToken::BgModalSurface);
    set(ImGuiCol_ModalWindowDimBg, ColorToken::BgModalScrim);

    // Borders and separators.
    set(ImGuiCol_Border, ColorToken::BorderDefault);
    set(ImGuiCol_Separator, ColorToken::SeparatorLine);

    // Input frames (checkbox, slider, text input backgrounds).
    set(ImGuiCol_FrameBg, ColorToken::InputBg);
    set(ImGuiCol_FrameBgHovered, ColorToken::InputBgFocused);
    set(ImGuiCol_FrameBgActive, ColorToken::InputBgFocused);

    // Buttons.
    set(ImGuiCol_Button, ColorToken::ButtonBg);
    set(ImGuiCol_ButtonHovered, ColorToken::ButtonBgHover);
    set(ImGuiCol_ButtonActive, ColorToken::ButtonBgActive);

    // Headers (collapsing headers, selectables, menu items) reuse buttons.
    set(ImGuiCol_Header, ColorToken::ButtonBg);
    set(ImGuiCol_HeaderHovered, ColorToken::ButtonBgHover);
    set(ImGuiCol_HeaderActive, ColorToken::ButtonBgActive);

    // Accent-driven widgets.
    set(ImGuiCol_CheckMark, ColorToken::ButtonBgPrimary);
    set(ImGuiCol_SliderGrab, ColorToken::SettingsSliderHandle);
    set(ImGuiCol_SliderGrabActive, ColorToken::ButtonBgPrimary);
    set(ImGuiCol_ScrollbarGrab, ColorToken::ButtonBg);
}

void refresh_active_theme_style() noexcept {
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    apply_theme_to_imgui_style(ImGui::GetStyle());
}

}  // namespace poker_trainer::theme
