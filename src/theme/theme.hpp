#pragma once

// Z06 public theme API.
//
// The active theme is the single source of truth for every color in the
// application. set_theme() swaps it (an O(1) struct copy plus one ImGui style
// refresh); get_active_theme() and get_color() (declared in theme_tokens.hpp)
// read it. Theme persistence is mediated here as well: the active theme is
// stored as a string token ("no_limit" | "slate" | "ocean" | "sage") so the
// on-disk representation is stable regardless of the numeric theme id.

#include "theme/theme_tokens.hpp"

#include <cstdint>
#include <string_view>

namespace poker_trainer::theme {

// Set the active theme by id (one of the kThemeId* values from
// theme_tokens.hpp). An out-of-range id falls back to No Limit, the default.
// Copies the selected Theme into the active slot and refreshes the ImGui
// style (a no-op when no ImGui context exists yet, e.g. at early startup).
void set_theme(std::uint8_t theme_id) noexcept;

// The currently active theme. Defaults to No Limit before any set_theme call.
[[nodiscard]] const Theme& get_active_theme() noexcept;

// get_color(ColorToken) is declared in theme_tokens.hpp (Phase 0 contract)
// and implemented in theme.cpp; it returns the active theme's value for the
// given token.

// ----- Persistence mapping -----
//
// The persisted form of the active theme is a stable string token, per
// ARCHITECTURE ("stored in IDBFS as a string token"). These helpers convert
// between that token and the numeric theme id carried by
// DisplaySettings::active_theme_id. They do not touch IDBFS or Zone 04; the
// caller owns the actual read/write.

// The persistence string token for a theme id ("no_limit" | "slate" |
// "ocean" | "sage"). An out-of-range id yields the No Limit token.
[[nodiscard]] std::string_view theme_id_to_token(std::uint8_t theme_id) noexcept;

// The theme id for a persistence string token. An unrecognized token yields
// kThemeIdNoLimit (the default), so an absent or corrupt saved value falls
// back to No Limit on load.
[[nodiscard]] std::uint8_t theme_id_from_token(std::string_view token) noexcept;

}  // namespace poker_trainer::theme
