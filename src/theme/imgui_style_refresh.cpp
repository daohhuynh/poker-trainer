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

    // Text. ImGui's "disabled" text slot also draws InputTextWithHint
    // placeholder text, so it maps to the placeholder token; genuinely
    // disabled controls are dimmed via an opacity multiplier at draw time.
    set(ImGuiCol_Text, ColorToken::TextPrimary);
    set(ImGuiCol_TextDisabled, ColorToken::TextPlaceholder);

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

    // Input frames (checkbox, slider, text input backgrounds). Hover and
    // focus do not change the fill — the focus cue is the border_focus
    // outline — so all three frame states use the same input fill.
    set(ImGuiCol_FrameBg, ColorToken::InputBg);
    set(ImGuiCol_FrameBgHovered, ColorToken::InputBg);
    set(ImGuiCol_FrameBgActive, ColorToken::InputBg);

    // Buttons.
    set(ImGuiCol_Button, ColorToken::ButtonBg);
    set(ImGuiCol_ButtonHovered, ColorToken::ButtonBgHover);
    set(ImGuiCol_ButtonActive, ColorToken::ButtonBgActive);

    // Headers (collapsing headers, selectables, menu items) reuse buttons.
    set(ImGuiCol_Header, ColorToken::ButtonBg);
    set(ImGuiCol_HeaderHovered, ColorToken::ButtonBgHover);
    set(ImGuiCol_HeaderActive, ColorToken::ButtonBgActive);

    // Accent-driven widgets. The slider handle rests at bg_button_default and
    // brightens to accent_primary while being dragged; checkmarks use the
    // accent (Token Application Rules: slider handle in bg_button_default,
    // accent for the active/selected highlight).
    set(ImGuiCol_CheckMark, ColorToken::AccentPrimary);
    set(ImGuiCol_SliderGrab, ColorToken::ButtonBg);
    set(ImGuiCol_SliderGrabActive, ColorToken::AccentPrimary);
    set(ImGuiCol_ScrollbarGrab, ColorToken::ButtonBg);
}

void refresh_active_theme_style() noexcept {
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    apply_theme_to_imgui_style(ImGui::GetStyle());
}

}  // namespace poker_trainer::theme
