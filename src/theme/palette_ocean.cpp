// Ocean palette.
//
// Deep blue-tinted. The accent is a lighter, brighter, more saturated ocean
// blue than the fixed dealer_button_blue, so the dealer button never reads as
// a UI button.

#include "theme/palettes.hpp"

#include <array>

#include <imgui.h>

namespace poker_trainer::theme {

const Theme& ocean_theme() noexcept {
    static constexpr PaletteSpec kSpec{
        .bg_primary = rgba8(15, 26, 42),           // dark navy
        .bg_modal = rgba8(30, 48, 72),             // medium slate-blue
        .bg_input = rgba8(22, 39, 58),
        .bg_input_focused = rgba8(27, 47, 70),
        .btn_default = rgba8(38, 74, 110),
        .btn_hover = rgba8(47, 92, 136),
        .btn_active = rgba8(58, 111, 162),
        .btn_armed = rgba8(27, 50, 75),
        .btn_disabled = rgba8(30, 49, 71),
        .text_primary = rgba8(228, 236, 244),      // off-white
        .text_secondary = rgba8(147, 166, 188),
        .text_disabled = rgba8(90, 110, 132),
        .text_button = rgba8(234, 241, 248),
        .text_input = rgba8(228, 236, 244),
        .accent_primary = rgba8(63, 169, 224),     // bright ocean blue (distinct
                                                   // from dealer_button_blue)
        .state_pass = rgba8(70, 168, 126),         // cool green
        .state_fail = rgba8(200, 90, 74),          // warm red
        .state_warn = rgba8(216, 162, 58),
        .border_default = rgba8(46, 74, 102),
        .separator = rgba8(228, 236, 244, 0.12f),
        .scrim = rgba8(4, 8, 14, 0.55f),
        .felt = rgba8(12, 32, 52, 0.22f),
        .settings_sidebar_bg = rgba8(19, 36, 58),
    };
    static constexpr std::array<ImVec4, kColorTokenCount> kTokens =
        build_palette(kSpec);
    static constexpr Theme kTheme{
        kTokens.data(),
        kThemeIdOcean,
        kThemeDisplayNames[kThemeIdOcean],
    };
    return kTheme;
}

}  // namespace poker_trainer::theme
