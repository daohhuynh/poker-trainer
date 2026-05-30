// Z06 theme core: the active-theme single source of truth.
//
// The active theme is process-global state, which the Phase 0 contract
// mandates: theme_tokens.hpp declares get_color() as a free function with no
// theme parameter, so a single active theme must be queryable globally. The
// application is single-threaded (browser main thread), so the active pointer
// needs no synchronization. All access is funneled through this file.

#include "theme/theme.hpp"

#include "theme/imgui_style_refresh.hpp"
#include "theme/palettes.hpp"
#include "theme/theme_tokens.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <imgui.h>

namespace poker_trainer::theme {

namespace {

// The active theme. Null until first use, then lazily defaulted to No Limit
// (ARCHITECTURE: "If no theme has been saved yet, No Limit is the default").
const Theme* g_active_theme = nullptr;

// Resolve a theme id to its palette. An out-of-range id falls back to the
// default theme rather than indexing out of bounds.
const Theme& select_theme(std::uint8_t theme_id) noexcept {
    switch (theme_id) {
        case kThemeIdSlate:
            return slate_theme();
        case kThemeIdOcean:
            return ocean_theme();
        case kThemeIdSage:
            return sage_theme();
        case kThemeIdNoLimit:
        default:
            return no_limit_theme();
    }
}

const Theme& active() noexcept {
    if (g_active_theme == nullptr) {
        g_active_theme = &no_limit_theme();
    }
    return *g_active_theme;
}

// Persistence string tokens, indexed by theme id. Order matches the
// kThemeId* values and kThemeDisplayNames in theme_tokens.hpp.
constexpr std::array<std::string_view, kThemeIdCount> kThemeTokens{
    "no_limit",
    "slate",
    "ocean",
    "sage",
};

}  // namespace

void set_theme(std::uint8_t theme_id) noexcept {
    g_active_theme = &select_theme(theme_id);
    refresh_active_theme_style();
}

const Theme& get_active_theme() noexcept {
    return active();
}

const ImVec4& get_color(ColorToken token) noexcept {
    const auto index = static_cast<std::size_t>(token);
    if (index >= kColorTokenCount) {
        return kFallbackColor;
    }
    return active().tokens[index];
}

std::string_view theme_id_to_token(std::uint8_t theme_id) noexcept {
    if (theme_id >= kThemeIdCount) {
        return kThemeTokens[kThemeIdNoLimit];
    }
    return kThemeTokens[theme_id];
}

std::uint8_t theme_id_from_token(std::string_view token) noexcept {
    for (std::size_t i = 0; i < kThemeIdCount; ++i) {
        if (kThemeTokens[i] == token) {
            return static_cast<std::uint8_t>(i);
        }
    }
    return kThemeIdNoLimit;
}

}  // namespace poker_trainer::theme
