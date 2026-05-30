// Sage palette.
//
// Warm green-tinted. The accent is a lighter, brighter, more saturated sage
// green than the fixed dealer_button_green, so the dealer button never reads
// as a UI button.

#include "theme/palettes.hpp"

#include <array>

#include <imgui.h>

namespace poker_trainer::theme {

const Theme& sage_theme() noexcept {
    static constexpr PaletteSpec kSpec{
        .bg_primary = rgba8(19, 30, 22),           // dark forest green
        .bg_modal = rgba8(42, 51, 40),             // medium olive-grey
        .bg_input = rgba8(28, 39, 28),
        .bg_input_focused = rgba8(35, 49, 33),
        .btn_default = rgba8(54, 74, 51),
        .btn_hover = rgba8(69, 94, 64),
        .btn_active = rgba8(85, 113, 78),
        .btn_armed = rgba8(40, 58, 38),
        .btn_disabled = rgba8(42, 51, 31),
        .text_primary = rgba8(236, 237, 224),      // warm off-white
        .text_secondary = rgba8(163, 168, 143),
        .text_disabled = rgba8(106, 111, 88),
        .text_button = rgba8(240, 241, 230),
        .text_input = rgba8(236, 237, 224),
        .accent_primary = rgba8(156, 203, 91),     // bright sage green (distinct
                                                   // from dealer_button_green)
        .accent_secondary = rgba8(224, 210, 160),  // warm cream
        .state_pass = rgba8(95, 181, 74),          // vivid green
        .state_fail = rgba8(199, 106, 69),         // muted red-orange
        .state_warn = rgba8(217, 162, 58),
        .border_default = rgba8(62, 82, 57),
        .separator = rgba8(236, 237, 224, 0.12f),
        .scrim = rgba8(6, 10, 6, 0.55f),
        .felt = rgba8(18, 40, 20, 0.22f),
        .settings_sidebar_bg = rgba8(23, 36, 23),
    };
    static constexpr std::array<ImVec4, kColorTokenCount> kTokens =
        build_palette(kSpec);
    static constexpr Theme kTheme{
        kTokens.data(),
        kThemeIdSage,
        kThemeDisplayNames[kThemeIdSage],
    };
    return kTheme;
}

}  // namespace poker_trainer::theme
