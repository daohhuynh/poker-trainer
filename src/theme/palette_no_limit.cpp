// No Limit palette (default).
//
// Warm high-limit private cardroom: aged leather in dim light, brass and
// amber. The defining accent is a bright warm amber-gold, deliberately
// brighter and more saturated than the fixed chip_gold tone so gold UI
// surfaces never read as a play chip.

#include "theme/palettes.hpp"

#include <array>

#include <imgui.h>

namespace poker_trainer::theme {

const Theme& no_limit_theme() noexcept {
    static constexpr PaletteSpec kSpec{
        .bg_primary = rgba8(26, 18, 16),           // deep warm brown, near-black
        .bg_modal = rgba8(42, 31, 27),             // medium warm brown-grey
        .bg_input = rgba8(36, 26, 23),
        .btn_default = rgba8(58, 44, 37),
        .btn_hover = rgba8(74, 56, 47),
        .btn_active = rgba8(90, 69, 58),
        .btn_armed = rgba8(46, 34, 28),
        .text_primary = rgba8(237, 227, 211),      // warm cream off-white
        .text_secondary = rgba8(168, 154, 136),    // muted warm grey
        .text_button = rgba8(242, 233, 218),
        .text_input = rgba8(237, 227, 211),
        .text_placeholder = rgba8(110, 98, 88),    // muted warm grey, dimmer
        .accent_primary = rgba8(239, 180, 46),     // bright warm amber-gold
        .accent_secondary = rgba8(168, 123, 74),   // muted warm bronze
        .state_pass = rgba8(107, 162, 87),         // warm-calibrated green
        .state_fail = rgba8(192, 88, 74),          // warm, slightly muted red
        .border_default = rgba8(74, 58, 48),
        .separator = rgba8(237, 227, 211, 0.12f),
        .scrim = rgba8(12, 8, 7, 0.55f),
        .felt = rgba8(40, 22, 16, 0.22f),
    };
    static constexpr std::array<ImVec4, kColorTokenCount> kTokens =
        build_palette(kSpec);
    static constexpr Theme kTheme{
        kTokens.data(),
        kThemeIdNoLimit,
        kThemeDisplayNames[kThemeIdNoLimit],
    };
    return kTheme;
}

}  // namespace poker_trainer::theme
