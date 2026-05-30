#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

// Forward declaration of ImVec4 to avoid pulling Dear ImGui into every
// consumer of this header. The actual definition is in imgui.h, which
// palette implementation files in Z06 include.
struct ImVec4;

namespace poker_trainer::theme {

// Token identifiers. Every color-bearing element in the trainer references
// one of these tokens. The set is the closed, semantic palette of
// ARCHITECTURE's "Token Palette": tokens name what an element *means*, never a
// specific UI widget, so a single token re-skins consistently across every
// surface that uses it. Element-to-token assignments live in ARCHITECTURE's
// "Token Application Rules", not in the enum. The enum value is also the index
// into Theme::tokens below.
enum class ColorToken : std::uint16_t {
    // --- Backgrounds ---
    BgPrimary = 0,               // bg_primary: global screen background tint
                                 // (one theme, no per-screen override)
    BgModalSurface = 1,          // bg_modal: modal panel fill
    BgModalTranslucent = 2,      // bg_modal_translucent: stat modal at 65% opacity
    BgModalScrim = 3,            // dimming layer behind modals (ImGui dim bg)
    BgTableFelt = 4,             // poker table felt overlay tint
    InputBg = 5,                 // bg_input: input box fill

    // --- Buttons (background states) ---
    ButtonBg = 6,                // bg_button_default
    ButtonBgHover = 7,           // bg_button_hover
    ButtonBgActive = 8,          // bg_button_active
    AgainButtonArmed = 9,        // bg_button_armed: Again button's armed state

    // --- Text ---
    TextPrimary = 10,            // text_primary: main body text
    TextSecondary = 11,          // text_secondary: subdued text (labels, captions)
    TextButton = 12,             // text_button: button text
    TextPlaceholder = 13,        // text_placeholder: empty-input placeholder text
    InputText = 14,              // text_input: text inside input fields

    // --- Accents ---
    AccentPrimary = 15,          // accent_primary: the theme's defining accent
                                 // (selected/active highlight, focus outline,
                                 // loading arc, active tier tab)
    AccentSecondary = 16,        // accent_secondary: complementary accent
                                 // (Leaderboard row highlight/tint at ~30%)

    // --- State colors ---
    StatePass = 17,              // state_pass: success / undertime green
    StateFail = 18,              // state_fail: failure / overtime / destructive red

    // --- Borders and separators ---
    BorderDefault = 19,          // border_default: input / panel borders
    BorderFocus = 20,            // border_focus: focus outline (renders as accent_primary)
    SeparatorLine = 21,          // separator: stat-modal row separators (low opacity)

    // --- Fixed across all themes ---
    // These tokens exist for API uniformity but their values are
    // identical in all four palettes. They cannot be themed.
    ChipWhite = 22,
    ChipRed = 23,
    ChipGreen = 24,
    ChipBlack = 25,
    ChipPurple = 26,
    ChipYellow = 27,
    ChipBrown = 28,
    ChipGold = 29,
    DealerButtonBlue = 30,
    DealerButtonGreen = 31,

    // Sentinel.
    Count = 32,
};

inline constexpr std::size_t kColorTokenCount =
    static_cast<std::size_t>(ColorToken::Count);

// Theme palette holder. One instance per theme; the four palette
// implementations in Z06 produce one of these each. The active
// theme pointer is held by Z06 and queried by every rendering
// consumer via get_color().
struct Theme {
    // Non-owning pointer to a kColorTokenCount-element ImVec4 array.
    // The pointee is owned by the palette .cpp file in Z06 (which
    // includes imgui.h), giving ImVec4 its complete definition there.
    // theme_tokens.hpp itself never needs ImVec4 to be a complete type.
    // Indexed by ColorToken: tokens[static_cast<size_t>(ColorToken::X)]
    // gives the ImVec4 for token X under this theme.
    const ImVec4* tokens;

    // Theme identifier (one of the kThemeId* values below).
    std::uint8_t theme_id;

    // Display name for the Settings theme picker.
    std::string_view display_name;
};

// Theme identifiers. The Settings theme dropdown lists themes in
// this exact order.
inline constexpr std::uint8_t kThemeIdNoLimit = 0;
inline constexpr std::uint8_t kThemeIdSlate = 1;
inline constexpr std::uint8_t kThemeIdOcean = 2;
inline constexpr std::uint8_t kThemeIdSage = 3;
inline constexpr std::uint8_t kThemeIdCount = 4;

// Display names for each theme, for the Settings theme picker.
inline constexpr std::array<std::string_view, kThemeIdCount> kThemeDisplayNames = {
    "No Limit",
    "Slate",
    "Ocean",
    "Sage",
};

// The set of tokens whose values are fixed across all themes.
// Palette implementations must populate these tokens with the same
// values in all four palettes. The integration test asserts this.
inline constexpr std::array<ColorToken, 10> kFixedAcrossThemeTokens = {
    ColorToken::ChipWhite,
    ColorToken::ChipRed,
    ColorToken::ChipGreen,
    ColorToken::ChipBlack,
    ColorToken::ChipPurple,
    ColorToken::ChipYellow,
    ColorToken::ChipBrown,
    ColorToken::ChipGold,
    ColorToken::DealerButtonBlue,
    ColorToken::DealerButtonGreen,
};

// Returns the active theme's color for a given token. Implemented
// in Z06 (palette switching logic). Phase 0 declares the signature
// only; the implementation is part of Z06's wave.
//
// Returned by const reference to avoid copying ImVec4 (16 bytes).
[[nodiscard]] const ImVec4& get_color(ColorToken token) noexcept;

}  // namespace poker_trainer::theme
