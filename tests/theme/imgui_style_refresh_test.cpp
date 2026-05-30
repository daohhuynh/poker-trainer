// Zone 06 ImGui style-refresh unit tests.
//
// Asserts that apply_theme_to_imgui_style writes the active theme's tokens
// into the expected ImGui color slots, that a theme switch changes those
// slots, and that refresh_active_theme_style drives ImGui::GetStyle() when a
// context exists and is a safe no-op when none does. No pixels are rendered.

#include "theme/imgui_style_refresh.hpp"

#include "theme/theme.hpp"
#include "theme/theme_tokens.hpp"

#include <gtest/gtest.h>
#include <imgui.h>

#include "color_eq.hpp"

namespace th = poker_trainer::theme;

namespace {

// apply_theme_to_imgui_style operates on a caller-provided style, so no ImGui
// context is required to test the mapping. ImGuiStyle default-constructs to
// the ImGui dark theme, which apply_* then overwrites.
ImGuiStyle styled_for(std::uint8_t theme_id) {
    th::set_theme(theme_id);
    ImGuiStyle style{};
    th::apply_theme_to_imgui_style(style);
    return style;
}

}  // namespace

TEST(ApplyThemeToImGuiStyle, WritesExpectedSlots) {
    th::set_theme(th::kThemeIdOcean);
    ImGuiStyle style{};
    th::apply_theme_to_imgui_style(style);

    const auto slot_is = [&style](ImGuiCol slot, th::ColorToken token) {
        th::test::expect_color_eq(style.Colors[slot], th::get_color(token));
    };

    // Every slot the bridge maps is asserted, so the full mapping is locked.
    slot_is(ImGuiCol_Text, th::ColorToken::TextPrimary);
    slot_is(ImGuiCol_TextDisabled, th::ColorToken::TextDisabled);
    slot_is(ImGuiCol_WindowBg, th::ColorToken::BgModalSurface);
    slot_is(ImGuiCol_ChildBg, th::ColorToken::BgModalSurface);
    slot_is(ImGuiCol_PopupBg, th::ColorToken::BgModalSurface);
    slot_is(ImGuiCol_TitleBg, th::ColorToken::BgModalSurface);
    slot_is(ImGuiCol_TitleBgActive, th::ColorToken::BgModalSurface);
    slot_is(ImGuiCol_TitleBgCollapsed, th::ColorToken::BgModalSurface);
    slot_is(ImGuiCol_ScrollbarBg, th::ColorToken::BgModalSurface);
    slot_is(ImGuiCol_ModalWindowDimBg, th::ColorToken::BgModalScrim);
    slot_is(ImGuiCol_Border, th::ColorToken::BorderDefault);
    slot_is(ImGuiCol_Separator, th::ColorToken::SeparatorLine);
    slot_is(ImGuiCol_FrameBg, th::ColorToken::InputBg);
    slot_is(ImGuiCol_FrameBgHovered, th::ColorToken::InputBgFocused);
    slot_is(ImGuiCol_FrameBgActive, th::ColorToken::InputBgFocused);
    slot_is(ImGuiCol_Button, th::ColorToken::ButtonBg);
    slot_is(ImGuiCol_ButtonHovered, th::ColorToken::ButtonBgHover);
    slot_is(ImGuiCol_ButtonActive, th::ColorToken::ButtonBgActive);
    slot_is(ImGuiCol_Header, th::ColorToken::ButtonBg);
    slot_is(ImGuiCol_HeaderHovered, th::ColorToken::ButtonBgHover);
    slot_is(ImGuiCol_HeaderActive, th::ColorToken::ButtonBgActive);
    slot_is(ImGuiCol_CheckMark, th::ColorToken::ButtonBgPrimary);
    slot_is(ImGuiCol_SliderGrab, th::ColorToken::SettingsSliderHandle);
    slot_is(ImGuiCol_SliderGrabActive, th::ColorToken::ButtonBgPrimary);
    slot_is(ImGuiCol_ScrollbarGrab, th::ColorToken::ButtonBg);
}

TEST(ApplyThemeToImGuiStyle, ThemeSwitchChangesSlots) {
    const ImGuiStyle no_limit = styled_for(th::kThemeIdNoLimit);
    const ImGuiStyle sage = styled_for(th::kThemeIdSage);

    EXPECT_FALSE(th::test::colors_equal(no_limit.Colors[ImGuiCol_Button],
                                        sage.Colors[ImGuiCol_Button]));
    EXPECT_FALSE(th::test::colors_equal(no_limit.Colors[ImGuiCol_CheckMark],
                                        sage.Colors[ImGuiCol_CheckMark]));
    EXPECT_FALSE(th::test::colors_equal(no_limit.Colors[ImGuiCol_WindowBg],
                                        sage.Colors[ImGuiCol_WindowBg]));
}

TEST(RefreshActiveThemeStyle, DrivesGlobalStyleWhenContextExists) {
    ImGuiContext* ctx = ImGui::CreateContext();
    ASSERT_NE(ctx, nullptr);

    // set_theme triggers refresh_active_theme_style internally; the global
    // style should reflect the active theme afterward.
    th::set_theme(th::kThemeIdOcean);
    const ImGuiStyle& global = ImGui::GetStyle();
    th::test::expect_color_eq(global.Colors[ImGuiCol_Button],
                              th::get_color(th::ColorToken::ButtonBg));
    th::test::expect_color_eq(global.Colors[ImGuiCol_CheckMark],
                              th::get_color(th::ColorToken::ButtonBgPrimary));

    // An explicit refresh after a switch updates the global style too.
    th::set_theme(th::kThemeIdSage);
    th::refresh_active_theme_style();
    th::test::expect_color_eq(ImGui::GetStyle().Colors[ImGuiCol_WindowBg],
                              th::get_color(th::ColorToken::BgModalSurface));

    ImGui::DestroyContext(ctx);
}

TEST(RefreshActiveThemeStyle, NoOpWithoutContext) {
    // No current context: refresh must not crash or touch global state.
    ASSERT_EQ(ImGui::GetCurrentContext(), nullptr);
    th::set_theme(th::kThemeIdSlate);
    th::refresh_active_theme_style();  // must be a safe no-op
    SUCCEED();
}
