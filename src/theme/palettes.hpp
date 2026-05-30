#pragma once

// Z06-internal palette infrastructure.
//
// Each of the four palette_*.cpp files specifies a PaletteSpec (a small set
// of named design colors taken from ARCHITECTURE.md's "Theme Library") and
// hands it to build_palette(), which expands the spec into the full
// kColorTokenCount-element ImVec4 array indexed by ColorToken. Keeping the
// token-to-design-color mapping in one constexpr function (rather than
// hand-writing 61 entries four times) means every theme shares an identical
// semantic structure and only the concrete hex values differ per theme.
//
// The fixed chip/dealer-button colors live here as shared constants so that
// build_palette writes the same values into every palette, satisfying the
// kFixedAcrossThemeTokens invariant by construction.

#include "theme/theme_tokens.hpp"

#include <array>
#include <cstddef>

#include <imgui.h>

namespace poker_trainer::theme {

// Construct an ImVec4 from 8-bit sRGB channels plus an optional alpha.
// Alpha defaults to fully opaque. Values are stored as ImGui's 0..1 floats.
[[nodiscard]] constexpr ImVec4 rgba8(int r, int g, int b, float a = 1.0f) noexcept {
    return ImVec4{
        static_cast<float>(r) / 255.0f,
        static_cast<float>(g) / 255.0f,
        static_cast<float>(b) / 255.0f,
        a,
    };
}

// The named design colors a single theme is built from. These mirror the
// semantic token names in ARCHITECTURE.md's Token Palette. build_palette()
// fans these out across the 61 concrete ColorToken slots.
struct PaletteSpec {
    ImVec4 bg_primary;          // main screen background tint
    ImVec4 bg_modal;            // modal panel fill
    ImVec4 bg_input;            // input field fill
    ImVec4 bg_input_focused;    // input field fill while focused
    ImVec4 btn_default;         // default button background
    ImVec4 btn_hover;           // button hover background
    ImVec4 btn_active;          // button pressed background
    ImVec4 btn_armed;           // Again button armed (darkened) background
    ImVec4 btn_disabled;        // disabled button background
    ImVec4 text_primary;        // main body text
    ImVec4 text_secondary;      // subdued text (labels, captions)
    ImVec4 text_disabled;       // greyed-out text / placeholder
    ImVec4 text_button;         // text on buttons / accent surfaces
    ImVec4 text_input;          // text inside input fields
    ImVec4 accent_primary;      // theme's defining accent
    ImVec4 accent_secondary;    // complementary accent (Leaderboard, etc.)
    ImVec4 state_pass;          // success / pass green
    ImVec4 state_fail;          // failure / error red
    ImVec4 state_warn;          // warning / caution amber
    ImVec4 border_default;      // default border for inputs / panels
    ImVec4 separator;           // separator lines (low opacity)
    ImVec4 scrim;               // modal dimming layer (translucent)
    ImVec4 felt;                // poker-table felt overlay tint (translucent)
    ImVec4 settings_sidebar_bg; // settings modal sidebar background
};

// ----- Tokens fixed across all four themes -----
//
// Poker chip denomination colors and dealer-button base colors. ARCHITECTURE
// freezes these across every theme: re-skinning chips would break the
// chip-counting drill's skill transfer, and the dealer button is intentionally
// outside both the play-chip and the theme palettes so it never reads as a UI
// button. build_palette writes these verbatim into every palette.
inline constexpr ImVec4 kChipWhite{rgba8(244, 241, 234)};
inline constexpr ImVec4 kChipRed{rgba8(178, 58, 51)};
inline constexpr ImVec4 kChipGreen{rgba8(44, 122, 75)};
inline constexpr ImVec4 kChipBlack{rgba8(26, 26, 29)};
inline constexpr ImVec4 kChipPurple{rgba8(110, 74, 158)};
inline constexpr ImVec4 kChipYellow{rgba8(232, 163, 23)};
inline constexpr ImVec4 kChipBrown{rgba8(122, 90, 68)};
inline constexpr ImVec4 kChipGold{rgba8(201, 162, 39)};
inline constexpr ImVec4 kDealerButtonBlue{rgba8(47, 110, 158)};
inline constexpr ImVec4 kDealerButtonGreen{rgba8(107, 142, 78)};

// The tutorial grey-lens overlay is fixed across all themes for consistent
// contrast (ARCHITECTURE: "rendered at 60% opacity using a neutral dark color,
// not theme-controlled").
inline constexpr ImVec4 kTutorialScrim{rgba8(10, 10, 12, 0.60f)};

// The service-outage banner's countdown bar is a fixed white progress bar
// across all themes (ARCHITECTURE: "a white progress bar renders" along the
// bottom edge of the banner).
inline constexpr ImVec4 kOutageCountdownBar{rgba8(255, 255, 255)};

// Returned by get_color() for an out-of-range token query. Bright magenta so
// misuse is visible on screen rather than silently rendering a plausible
// color. It lives here, alongside the other theme-layer color constants, so
// that theme.cpp carries no literal color values of its own.
inline constexpr ImVec4 kFallbackColor{1.0f, 0.0f, 1.0f, 1.0f};

// Expand a PaletteSpec into the full ColorToken-indexed array. Every one of
// the kColorTokenCount tokens receives a concrete value; the fixed chip and
// dealer-button tokens are written from the shared constants above.
[[nodiscard]] constexpr std::array<ImVec4, kColorTokenCount> build_palette(
    const PaletteSpec& s) noexcept {
    std::array<ImVec4, kColorTokenCount> t{};

    auto at = [&t](ColorToken token) -> ImVec4& {
        return t[static_cast<std::size_t>(token)];
    };

    // Backgrounds. The three screen backgrounds share bg_primary; there are
    // no per-screen theme overrides (ARCHITECTURE: one global theme).
    at(ColorToken::BgRoot) = s.bg_primary;
    at(ColorToken::BgGame) = s.bg_primary;
    at(ColorToken::BgPostRound) = s.bg_primary;
    at(ColorToken::BgModalSurface) = s.bg_modal;
    at(ColorToken::BgModalScrim) = s.scrim;
    at(ColorToken::BgTableFelt) = s.felt;

    // Text.
    at(ColorToken::TextPrimary) = s.text_primary;
    at(ColorToken::TextSecondary) = s.text_secondary;
    at(ColorToken::TextDisabled) = s.text_disabled;
    at(ColorToken::TextOnAccent) = s.text_button;

    // Borders and separators. The focus outline renders as accent_primary.
    at(ColorToken::BorderDefault) = s.border_default;
    at(ColorToken::BorderFocus) = s.accent_primary;
    at(ColorToken::SeparatorLine) = s.separator;

    // Buttons. The primary call-to-action uses the accent; the danger button
    // uses the failure red.
    at(ColorToken::ButtonBg) = s.btn_default;
    at(ColorToken::ButtonBgHover) = s.btn_hover;
    at(ColorToken::ButtonBgActive) = s.btn_active;
    at(ColorToken::ButtonBgDisabled) = s.btn_disabled;
    at(ColorToken::ButtonBgPrimary) = s.accent_primary;
    at(ColorToken::ButtonBgDanger) = s.state_fail;

    // Inputs.
    at(ColorToken::InputBg) = s.bg_input;
    at(ColorToken::InputBgFocused) = s.bg_input_focused;
    at(ColorToken::InputText) = s.text_input;

    // State colors. Overtime reuses the failure red per ARCHITECTURE (the
    // Visual Countdown text turns state_fail while in overtime).
    at(ColorToken::StatePass) = s.state_pass;
    at(ColorToken::StateFail) = s.state_fail;
    at(ColorToken::StateWarn) = s.state_warn;
    at(ColorToken::StateOvertime) = s.state_fail;

    // HUD text on the Game screen.
    at(ColorToken::HudPotText) = s.text_primary;
    at(ColorToken::HudBlindsText) = s.text_secondary;
    at(ColorToken::HudBetAmountText) = s.text_primary;

    // Cluster icons sit on the screen background: glyph in text_primary,
    // hover fill in the button hover color.
    at(ColorToken::ClusterIconTint) = s.text_primary;
    at(ColorToken::ClusterIconHover) = s.btn_hover;

    // Offline sync indicator glyph renders in text_secondary (ARCHITECTURE:
    // "The icon color is text_secondary"); it is informational, not a warning.
    at(ColorToken::OfflineIndicator) = s.text_secondary;

    // Outage banner: modal-fill background and primary text, with a fixed
    // white countdown bar (ARCHITECTURE: bg_modal background, text_primary
    // message, "a white progress bar" along the bottom edge).
    at(ColorToken::OutageBannerBg) = s.bg_modal;
    at(ColorToken::OutageBannerText) = s.text_primary;
    at(ColorToken::OutageBannerCountdown) = kOutageCountdownBar;

    // Settings: sidebar fill, section headers, and the slider widget.
    at(ColorToken::SettingsSidebarBg) = s.settings_sidebar_bg;
    at(ColorToken::SettingsSectionHeader) = s.text_primary;
    at(ColorToken::SettingsSliderTrack) = s.border_default;
    at(ColorToken::SettingsSliderFill) = s.accent_primary;
    at(ColorToken::SettingsSliderHandle) = s.btn_default;

    // Tutorial overlay (fixed scrim), callout panel, and callout border.
    at(ColorToken::TutorialScrim) = kTutorialScrim;
    at(ColorToken::TutorialCalloutBg) = s.bg_modal;
    at(ColorToken::TutorialCalloutBorder) = s.border_default;

    // Post-Round stat modal. The background is the modal fill at 65% opacity.
    at(ColorToken::StatModalBg) =
        ImVec4{s.bg_modal.x, s.bg_modal.y, s.bg_modal.z, 0.65f};
    at(ColorToken::StatModalTabBg) = s.btn_default;
    at(ColorToken::StatModalTabActive) = s.accent_primary;
    at(ColorToken::TimeGradeUndertime) = s.state_pass;
    at(ColorToken::TimeGradeOvertime) = s.state_fail;

    // Again button: default, armed (darkened), and commit (accent) states.
    at(ColorToken::AgainButtonDefault) = s.btn_default;
    at(ColorToken::AgainButtonArmed) = s.btn_armed;
    at(ColorToken::AgainButtonCommit) = s.accent_primary;

    // Complementary accent (post-seal-appended token). Theme-controlled;
    // consumers like the Leaderboard apply it at reduced opacity (~30%).
    at(ColorToken::AccentSecondary) = s.accent_secondary;

    // Fixed across all themes.
    at(ColorToken::ChipWhite) = kChipWhite;
    at(ColorToken::ChipRed) = kChipRed;
    at(ColorToken::ChipGreen) = kChipGreen;
    at(ColorToken::ChipBlack) = kChipBlack;
    at(ColorToken::ChipPurple) = kChipPurple;
    at(ColorToken::ChipYellow) = kChipYellow;
    at(ColorToken::ChipBrown) = kChipBrown;
    at(ColorToken::ChipGold) = kChipGold;
    at(ColorToken::DealerButtonBlue) = kDealerButtonBlue;
    at(ColorToken::DealerButtonGreen) = kDealerButtonGreen;

    return t;
}

// Per-theme accessors. Each returns a Theme whose tokens pointer references a
// function-local static array with static storage duration, so the pointer
// stays valid for the lifetime of the program. Defined in the matching
// palette_*.cpp.
[[nodiscard]] const Theme& no_limit_theme() noexcept;
[[nodiscard]] const Theme& slate_theme() noexcept;
[[nodiscard]] const Theme& ocean_theme() noexcept;
[[nodiscard]] const Theme& sage_theme() noexcept;

}  // namespace poker_trainer::theme
