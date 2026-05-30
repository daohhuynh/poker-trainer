// Slate palette.
//
// Dark neutral with a faint cool tint, designed to recede into the high-limit
// room. The accent is a muted gold aligned with the dealer's monocle without
// being garish.

#include "theme/palettes.hpp"

#include <array>

#include <imgui.h>

namespace poker_trainer::theme {

const Theme& slate_theme() noexcept {
    static constexpr PaletteSpec kSpec{
        .bg_primary = rgba8(22, 24, 28),           // deep charcoal, faint cool tint
        .bg_modal = rgba8(38, 42, 48),             // warmer medium-grey
        .bg_input = rgba8(30, 34, 39),
        .bg_input_focused = rgba8(36, 42, 49),
        .btn_default = rgba8(46, 52, 60),
        .btn_hover = rgba8(58, 66, 76),
        .btn_active = rgba8(72, 81, 92),
        .btn_armed = rgba8(35, 40, 46),
        .btn_disabled = rgba8(36, 41, 47),
        .text_primary = rgba8(230, 232, 235),      // off-white
        .text_secondary = rgba8(154, 160, 168),
        .text_disabled = rgba8(98, 105, 116),
        .text_button = rgba8(236, 238, 241),
        .text_input = rgba8(230, 232, 235),
        .accent_primary = rgba8(191, 160, 70),     // muted gold (monocle)
        .state_pass = rgba8(79, 164, 92),          // clean medium green
        .state_fail = rgba8(192, 80, 77),          // clean medium red
        .state_warn = rgba8(208, 162, 58),
        .border_default = rgba8(60, 67, 76),
        .separator = rgba8(230, 232, 235, 0.12f),
        .scrim = rgba8(4, 6, 9, 0.55f),
        .felt = rgba8(20, 30, 40, 0.20f),
        .settings_sidebar_bg = rgba8(27, 30, 35),
    };
    static constexpr std::array<ImVec4, kColorTokenCount> kTokens =
        build_palette(kSpec);
    static constexpr Theme kTheme{
        kTokens.data(),
        kThemeIdSlate,
        kThemeDisplayNames[kThemeIdSlate],
    };
    return kTheme;
}

}  // namespace poker_trainer::theme
