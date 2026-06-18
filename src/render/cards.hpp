#pragma once

#include "engine/scenario.hpp"

#include "assets/asset_paths.hpp"
#include "theme/theme_tokens.hpp"

#include <cstdint>

struct ImDrawList;

// Zone 08 — community + hole card rendering (ARCHITECTURE Game Screen: community
// cards top-middle fanned face-up, hole cards bottom-middle fanned face-up).
//
// Each card renders its card-face PNG through the shared texture-bind seam
// (bridge::draw_asset_image), so dropping real art in later is a file swap with no
// code change. When the art is unavailable the procedural fallback draws: a
// rounded rect with a fixed-token fill, rank label and a suit letter colored
// red/black. The fallback colors are the THEME-FIXED chip tokens (ChipWhite fill,
// ChipRed / ChipBlack pips), not literals and not theme-tinted — honoring both
// CLAUDE.md sec.5 (no hardcoded colors) and the invariant that card faces render
// as authored (theme tints never apply to them). The suit letter (S/H/D/C) is
// used instead of a Unicode pip because the default ImGui font is ASCII-only.

namespace poker_trainer::render {

// The card-face asset for a card. Asset order is Spades A-K, Hearts A-K,
// Diamonds A-K, Clubs A-K (asset_paths.hpp); within a suit the rank order is
// A, 2..9, T, J, Q, K. Pure mapping (unit-tested).
[[nodiscard]] assets::AssetId card_asset_id(engine::Card card) noexcept;

// Rank glyph: "2".."9", "T", "J", "Q", "K", "A".
[[nodiscard]] const char* rank_label(std::uint8_t rank) noexcept;

// Suit initial used on the placeholder face: 'C' / 'D' / 'H' / 'S'.
[[nodiscard]] char suit_letter(engine::Suit suit) noexcept;

// True for the red suits (Hearts, Diamonds).
[[nodiscard]] bool suit_is_red(engine::Suit suit) noexcept;

// The theme-fixed color token a suit's pips render in (ChipRed for red suits,
// ChipBlack for black). Fixed across themes, matching the authored-art rule.
[[nodiscard]] theme::ColorToken suit_color_token(engine::Suit suit) noexcept;

// ----- Render (game render TU only) -----

// Draw one face-up card with its top-left at (x, y), at `scale` x the base card
// size (the hero's hole cards are drawn larger/closer than the board in the
// first-person view).
void draw_card(ImDrawList* dl, float x, float y, engine::Card card, float scale = 1.0f);

// Draw a face-up fan of `count` cards centered horizontally at center_x, with the
// top edge at top_y, at `scale` x the base card size. Used for both the community
// board (scale 1) and the hero hole cards (scale > 1).
void draw_card_fan(ImDrawList* dl, float center_x, float top_y,
                   const engine::Card* cards, std::uint8_t count, float scale = 1.0f);

}  // namespace poker_trainer::render
