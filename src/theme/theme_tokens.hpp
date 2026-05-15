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

// Token identifiers. Every color-bearing element in the trainer
// references one of these tokens. The enum value is also the index
// into Theme::tokens below.
enum class ColorToken : std::uint16_t {
    // --- Backgrounds ---
    BgRoot = 0,                  // Root screen base background
    BgGame = 1,                  // Game screen base background
    BgPostRound = 2,             // Post-Round Screen base background
    BgModalSurface = 3,          // Modal panel fill
    BgModalScrim = 4,            // Dimming layer behind modals
    BgTableFelt = 5,             // Poker table felt overlay tint

    // --- Text ---
    TextPrimary = 6,             // Default text color
    TextSecondary = 7,           // Subdued text (labels, captions)
    TextDisabled = 8,            // Greyed-out text
    TextOnAccent = 9,            // Text on accent-colored elements (buttons)

    // --- Borders and separators ---
    BorderDefault = 10,          // Default border for input fields, panels
    BorderFocus = 11,            // 2px focus indicator outline
    SeparatorLine = 12,          // Horizontal/vertical separator lines

    // --- Buttons ---
    ButtonBg = 13,               // Default button background
    ButtonBgHover = 14,          // Button on hover
    ButtonBgActive = 15,         // Button while being clicked
    ButtonBgDisabled = 16,       // Disabled button
    ButtonBgPrimary = 17,        // Primary call-to-action background
    ButtonBgDanger = 18,         // Destructive action background (delete, reset)

    // --- Inputs ---
    InputBg = 19,                // Text input field background
    InputBgFocused = 20,         // Text input when focused
    InputText = 21,              // Text inside input fields

    // --- State colors ---
    StatePass = 22,              // Success / pass color
    StateFail = 23,              // Failure / error color
    StateWarn = 24,              // Warning / caution color
    StateOvertime = 25,          // Overtime countdown red

    // --- HUD on Game screen ---
    HudPotText = 26,             // Pot amount text
    HudBlindsText = 27,          // Blinds text
    HudBetAmountText = 28,       // Floating bet amount text

    // --- Cluster icons ---
    ClusterIconTint = 29,        // Default cluster icon tint
    ClusterIconHover = 30,       // Cluster icon on hover

    // --- Offline indicator ---
    OfflineIndicator = 31,       // The sync-failing indicator

    // --- Outage banner ---
    OutageBannerBg = 32,         // Service Outage Banner background
    OutageBannerText = 33,       // Service Outage Banner text
    OutageBannerCountdown = 34,  // Countdown bar fill

    // --- Settings ---
    SettingsSidebarBg = 35,      // Settings modal sidebar background
    SettingsSectionHeader = 36,  // Section header text
    SettingsSliderTrack = 37,    // Slider track background
    SettingsSliderFill = 38,     // Slider fill before the handle
    SettingsSliderHandle = 39,   // Slider handle

    // --- Tutorial ---
    TutorialScrim = 40,          // Tutorial overlay dim layer
    TutorialCalloutBg = 41,      // Callout panel background
    TutorialCalloutBorder = 42,  // Callout panel border

    // --- Post-Round stat modal ---
    StatModalBg = 43,            // Stat modal background (65% opacity)
    StatModalTabBg = 44,         // Tier tab default background
    StatModalTabActive = 45,     // Active tier tab background
    TimeGradeUndertime = 46,     // Time grade row, under-time color
    TimeGradeOvertime = 47,      // Time grade row, over-time color

    // --- Again button (Post-Round) ---
    AgainButtonDefault = 48,     // AGAIN default state
    AgainButtonArmed = 49,       // AGAIN armed state
    AgainButtonCommit = 50,      // CONFIRM commit state

    // --- Fixed across all themes ---
    // These tokens exist for API uniformity but their values are
    // identical in all four palettes. They cannot be themed.
    ChipWhite = 51,
    ChipRed = 52,
    ChipGreen = 53,
    ChipBlack = 54,
    ChipPurple = 55,
    ChipYellow = 56,
    ChipBrown = 57,
    ChipGold = 58,
    DealerButtonBlue = 59,
    DealerButtonGreen = 60,

    // Sentinel.
    Count = 61,
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
