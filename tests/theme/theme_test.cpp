// Zone 06 theme system unit tests.
//
// Covers: set_theme swaps the active palette; get_color returns the correct
// per-theme value; the default is No Limit; the fixed chip/dealer tokens are
// identical across themes; every token is populated; the Ocean/Sage accents
// are visually distinct from the dealer-button colors; and the active theme
// survives a round-trip through the persistence contract's string token.

#include "theme/theme.hpp"

#include "theme/palettes.hpp"
#include "theme/theme_tokens.hpp"

#include "persistence/persistence_schema.hpp"
#include "settings/settings.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <gtest/gtest.h>
#include <imgui.h>

#include "color_eq.hpp"

namespace th = poker_trainer::theme;
namespace pt = poker_trainer;

namespace {

constexpr std::array<std::uint8_t, th::kThemeIdCount> kAllThemeIds{
    th::kThemeIdNoLimit,
    th::kThemeIdSlate,
    th::kThemeIdOcean,
    th::kThemeIdSage,
};

const th::Theme& theme_for(std::uint8_t id) {
    switch (id) {
        case th::kThemeIdSlate:
            return th::slate_theme();
        case th::kThemeIdOcean:
            return th::ocean_theme();
        case th::kThemeIdSage:
            return th::sage_theme();
        default:
            return th::no_limit_theme();
    }
}

ImVec4 token_value(std::uint8_t id, th::ColorToken token) {
    return theme_for(id).tokens[static_cast<std::size_t>(token)];
}

}  // namespace

// ----- set_theme / get_active_theme -----

TEST(SetTheme, SwapsActivePalette) {
    th::set_theme(th::kThemeIdOcean);
    EXPECT_EQ(th::get_active_theme().theme_id, th::kThemeIdOcean);
    EXPECT_EQ(th::get_active_theme().display_name, std::string_view{"Ocean"});

    th::set_theme(th::kThemeIdSage);
    EXPECT_EQ(th::get_active_theme().theme_id, th::kThemeIdSage);
    EXPECT_EQ(th::get_active_theme().display_name, std::string_view{"Sage"});
}

TEST(SetTheme, GetColorTracksActiveTheme) {
    // After each switch, get_color reflects the newly active palette.
    th::set_theme(th::kThemeIdOcean);
    th::test::expect_color_eq(th::get_color(th::ColorToken::BgRoot),
                              token_value(th::kThemeIdOcean, th::ColorToken::BgRoot));

    th::set_theme(th::kThemeIdNoLimit);
    th::test::expect_color_eq(th::get_color(th::ColorToken::BgRoot),
                              token_value(th::kThemeIdNoLimit, th::ColorToken::BgRoot));

    // The background genuinely differs between the two themes.
    EXPECT_FALSE(th::test::colors_equal(
        token_value(th::kThemeIdOcean, th::ColorToken::BgRoot),
        token_value(th::kThemeIdNoLimit, th::ColorToken::BgRoot)));
}

TEST(SetTheme, GetColorMatchesActivePaletteForEveryToken) {
    for (const std::uint8_t id : kAllThemeIds) {
        th::set_theme(id);
        for (std::size_t i = 0; i < th::kColorTokenCount; ++i) {
            const auto token = static_cast<th::ColorToken>(i);
            th::test::expect_color_eq(th::get_color(token), token_value(id, token));
        }
    }
}

// ----- default is No Limit -----
//
// The active-theme global persists across tests in one process, so a single
// "active defaults to No Limit before any set_theme" assertion would be
// order-dependent. Instead the default is pinned from every angle that the
// contract actually exposes: the persisted settings default, the load-time
// token fallback, the runtime invalid-id fallback, and the palette identity.

TEST(DefaultTheme, DisplaySettingsDefaultIsNoLimit) {
    const pt::settings::Settings s{};
    EXPECT_EQ(s.display.active_theme_id, th::kThemeIdNoLimit);
}

TEST(DefaultTheme, NoLimitPaletteIdentity) {
    EXPECT_EQ(th::no_limit_theme().theme_id, th::kThemeIdNoLimit);
    EXPECT_EQ(th::no_limit_theme().display_name, std::string_view{"No Limit"});
}

TEST(DefaultTheme, InvalidThemeIdFallsBackToNoLimit) {
    th::set_theme(static_cast<std::uint8_t>(200));
    EXPECT_EQ(th::get_active_theme().theme_id, th::kThemeIdNoLimit);

    th::set_theme(th::kThemeIdCount);  // first invalid id past the valid range
    EXPECT_EQ(th::get_active_theme().theme_id, th::kThemeIdNoLimit);
}

// ----- per-theme accent hex values (headline colors) -----

TEST(PaletteValues, AccentPrimaryHexPerTheme) {
    // accent_primary maps onto ButtonBgPrimary (and BorderFocus, etc.).
    th::test::expect_color_eq(
        token_value(th::kThemeIdNoLimit, th::ColorToken::ButtonBgPrimary),
        th::rgba8(239, 180, 46));
    th::test::expect_color_eq(
        token_value(th::kThemeIdSlate, th::ColorToken::ButtonBgPrimary),
        th::rgba8(191, 160, 70));
    th::test::expect_color_eq(
        token_value(th::kThemeIdOcean, th::ColorToken::ButtonBgPrimary),
        th::rgba8(63, 169, 224));
    th::test::expect_color_eq(
        token_value(th::kThemeIdSage, th::ColorToken::ButtonBgPrimary),
        th::rgba8(156, 203, 91));
}

TEST(PaletteValues, AccentSecondaryHexPerTheme) {
    // accent_secondary was appended to the contract post-seal; verify each
    // theme's value matches its ARCHITECTURE description.
    th::test::expect_color_eq(
        token_value(th::kThemeIdNoLimit, th::ColorToken::AccentSecondary),
        th::rgba8(168, 123, 74));   // muted warm bronze
    th::test::expect_color_eq(
        token_value(th::kThemeIdSlate, th::ColorToken::AccentSecondary),
        th::rgba8(153, 127, 77));   // subdued bronze
    th::test::expect_color_eq(
        token_value(th::kThemeIdOcean, th::ColorToken::AccentSecondary),
        th::rgba8(111, 194, 194));  // pale teal
    th::test::expect_color_eq(
        token_value(th::kThemeIdSage, th::ColorToken::AccentSecondary),
        th::rgba8(224, 210, 160));  // warm cream

    // It is theme-controlled (distinct from accent_primary) and not fixed.
    for (const std::uint8_t id : kAllThemeIds) {
        EXPECT_FALSE(th::test::colors_equal(
            token_value(id, th::ColorToken::AccentSecondary),
            token_value(id, th::ColorToken::ButtonBgPrimary)));
    }
}

TEST(PaletteValues, BorderFocusEqualsAccentPrimary) {
    for (const std::uint8_t id : kAllThemeIds) {
        th::test::expect_color_eq(
            token_value(id, th::ColorToken::BorderFocus),
            token_value(id, th::ColorToken::ButtonBgPrimary));
    }
}

TEST(PaletteValues, EveryTokenIsPopulatedInEveryTheme) {
    // build_palette zero-initializes the array, so an all-zero token would
    // mean a token was never assigned. Catches an unmapped token.
    const ImVec4 zero{0.0f, 0.0f, 0.0f, 0.0f};
    for (const std::uint8_t id : kAllThemeIds) {
        for (std::size_t i = 0; i < th::kColorTokenCount; ++i) {
            const auto token = static_cast<th::ColorToken>(i);
            EXPECT_FALSE(th::test::colors_equal(token_value(id, token), zero))
                << "theme " << static_cast<int>(id) << " token " << i
                << " is all-zero (unmapped)";
        }
    }
}

TEST(PaletteValues, OutageAndOfflineTokensMatchSpec) {
    // ARCHITECTURE: outage banner uses bg_modal background + text_primary
    // message + a white countdown bar; the offline indicator is text_secondary.
    const ImVec4 white{1.0f, 1.0f, 1.0f, 1.0f};
    for (const std::uint8_t id : kAllThemeIds) {
        th::test::expect_color_eq(
            token_value(id, th::ColorToken::OutageBannerBg),
            token_value(id, th::ColorToken::BgModalSurface));
        th::test::expect_color_eq(
            token_value(id, th::ColorToken::OutageBannerText),
            token_value(id, th::ColorToken::TextPrimary));
        th::test::expect_color_eq(
            token_value(id, th::ColorToken::OutageBannerCountdown), white);
        th::test::expect_color_eq(
            token_value(id, th::ColorToken::OfflineIndicator),
            token_value(id, th::ColorToken::TextSecondary));
    }
}

// ----- get_color out-of-range guard -----

TEST(GetColor, OutOfRangeTokenReturnsFallback) {
    th::set_theme(th::kThemeIdOcean);
    const auto out_of_range = static_cast<th::ColorToken>(th::kColorTokenCount);
    th::test::expect_color_eq(th::get_color(out_of_range), th::kFallbackColor);
}

// ----- fixed tokens -----

TEST(FixedTokens, IdenticalAcrossAllThemes) {
    for (const th::ColorToken token : th::kFixedAcrossThemeTokens) {
        const ImVec4 reference = token_value(th::kThemeIdNoLimit, token);
        for (const std::uint8_t id : kAllThemeIds) {
            EXPECT_TRUE(th::test::colors_equal(token_value(id, token), reference))
                << "fixed token " << static_cast<std::size_t>(token)
                << " differs in theme " << static_cast<int>(id);
        }
    }
}

TEST(FixedTokens, MatchTheSharedChipAndDealerConstants) {
    EXPECT_TRUE(th::test::colors_equal(
        token_value(th::kThemeIdNoLimit, th::ColorToken::ChipWhite), th::kChipWhite));
    EXPECT_TRUE(th::test::colors_equal(
        token_value(th::kThemeIdNoLimit, th::ColorToken::ChipGold), th::kChipGold));
    EXPECT_TRUE(th::test::colors_equal(
        token_value(th::kThemeIdNoLimit, th::ColorToken::DealerButtonBlue),
        th::kDealerButtonBlue));
    EXPECT_TRUE(th::test::colors_equal(
        token_value(th::kThemeIdNoLimit, th::ColorToken::DealerButtonGreen),
        th::kDealerButtonGreen));
}

TEST(FixedTokens, TutorialScrimIsIdenticalAcrossThemes) {
    // ARCHITECTURE pins the tutorial grey lens as fixed across themes even
    // though it is not in kFixedAcrossThemeTokens.
    const ImVec4 reference =
        token_value(th::kThemeIdNoLimit, th::ColorToken::TutorialScrim);
    for (const std::uint8_t id : kAllThemeIds) {
        EXPECT_TRUE(th::test::colors_equal(
            token_value(id, th::ColorToken::TutorialScrim), reference));
    }
}

// ----- accent vs dealer-button distinctness -----

TEST(AccentDistinctness, OceanAccentDiffersFromDealerButtonBlue) {
    const ImVec4 accent =
        token_value(th::kThemeIdOcean, th::ColorToken::ButtonBgPrimary);
    EXPECT_FALSE(th::test::colors_equal(accent, th::kDealerButtonBlue));
    EXPECT_GT(th::test::rgb_distance_255(accent, th::kDealerButtonBlue), 48.0f);
}

TEST(AccentDistinctness, SageAccentDiffersFromDealerButtonGreen) {
    const ImVec4 accent =
        token_value(th::kThemeIdSage, th::ColorToken::ButtonBgPrimary);
    EXPECT_FALSE(th::test::colors_equal(accent, th::kDealerButtonGreen));
    EXPECT_GT(th::test::rgb_distance_255(accent, th::kDealerButtonGreen), 48.0f);
}

// ----- persistence string-token mapping -----

TEST(PersistenceMapping, TokensAreTheExpectedStrings) {
    EXPECT_EQ(th::theme_id_to_token(th::kThemeIdNoLimit), std::string_view{"no_limit"});
    EXPECT_EQ(th::theme_id_to_token(th::kThemeIdSlate), std::string_view{"slate"});
    EXPECT_EQ(th::theme_id_to_token(th::kThemeIdOcean), std::string_view{"ocean"});
    EXPECT_EQ(th::theme_id_to_token(th::kThemeIdSage), std::string_view{"sage"});
}

TEST(PersistenceMapping, IdTokenRoundTrips) {
    for (const std::uint8_t id : kAllThemeIds) {
        EXPECT_EQ(th::theme_id_from_token(th::theme_id_to_token(id)), id);
    }
}

TEST(PersistenceMapping, OutOfRangeIdYieldsNoLimitToken) {
    EXPECT_EQ(th::theme_id_to_token(static_cast<std::uint8_t>(99)),
              std::string_view{"no_limit"});
}

TEST(PersistenceMapping, UnknownTokenFallsBackToNoLimit) {
    EXPECT_EQ(th::theme_id_from_token("rainbow"), th::kThemeIdNoLimit);
    EXPECT_EQ(th::theme_id_from_token(""), th::kThemeIdNoLimit);
    EXPECT_EQ(th::theme_id_from_token("NO_LIMIT"), th::kThemeIdNoLimit);  // case-sensitive
}

// ----- round-trip through the persistence contract -----

TEST(PersistenceRoundTrip, ThemeSurvivesAppStateSettingsBlob) {
    for (const std::uint8_t id : kAllThemeIds) {
        th::set_theme(id);

        // Save path: the active theme id is serialized to its stable string
        // token and carried in the AppState settings blob (the persistence
        // contract field that holds serialized settings). No Zone 04 calls.
        const std::string_view token =
            th::theme_id_to_token(th::get_active_theme().theme_id);
        pt::persistence::AppState saved{};
        saved.settings_blob.assign(token.begin(), token.end());

        // Load path: a fresh copy of the persisted state, decode the token,
        // resolve back to an id, and reapply.
        const pt::persistence::AppState loaded = saved;
        const std::string read_token(loaded.settings_blob.begin(),
                                     loaded.settings_blob.end());
        const std::uint8_t restored = th::theme_id_from_token(read_token);
        th::set_theme(restored);

        EXPECT_EQ(th::get_active_theme().theme_id, id);
    }
}

TEST(PersistenceRoundTrip, ThemeSurvivesDisplaySettingsField) {
    for (const std::uint8_t id : kAllThemeIds) {
        pt::settings::Settings to_save{};
        to_save.display.active_theme_id = id;

        // Round-trip the contract field through the stable string token.
        const std::string_view token =
            th::theme_id_to_token(to_save.display.active_theme_id);
        pt::settings::Settings loaded{};
        loaded.display.active_theme_id = th::theme_id_from_token(token);

        EXPECT_EQ(loaded.display.active_theme_id, id);

        th::set_theme(loaded.display.active_theme_id);
        EXPECT_EQ(th::get_active_theme().theme_id, id);
    }
}
