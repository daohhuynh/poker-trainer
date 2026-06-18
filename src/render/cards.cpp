#include "render/cards.hpp"

#include "render/render_constants.hpp"

#include "engine/scenario.hpp"
#include "theme/theme_tokens.hpp"

#include <cstdint>

#include <imgui.h>

#include "assets/asset_paths.hpp"
#include "bridge/asset_image.hpp"

namespace poker_trainer::render {

namespace {

[[nodiscard]] ImU32 token_u32(theme::ColorToken token) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(token));
}

// First enumerator of each suit's 13-card asset block (asset_paths.hpp order:
// Spades A-K, Hearts A-K, Diamonds A-K, Clubs A-K).
[[nodiscard]] assets::AssetId suit_block_base(engine::Suit suit) noexcept {
    switch (suit) {
        case engine::Suit::Spades:   return assets::AssetId::CardSpadeA;
        case engine::Suit::Hearts:   return assets::AssetId::CardHeartA;
        case engine::Suit::Diamonds: return assets::AssetId::CardDiamondA;
        case engine::Suit::Clubs:    return assets::AssetId::CardClubA;
    }
    return assets::AssetId::CardSpadeA;
}

}  // namespace

assets::AssetId card_asset_id(engine::Card card) noexcept {
    // Within a suit the asset order is A, 2..9, T, J, Q, K, so the Ace (rank 14)
    // is offset 0 and ranks 2..13 are offsets 1..12.
    const std::uint16_t offset =
        card.rank == engine::kMaxRank ? 0u : static_cast<std::uint16_t>(card.rank - 1u);
    const auto base = static_cast<std::uint16_t>(suit_block_base(card.suit));
    return static_cast<assets::AssetId>(static_cast<std::uint16_t>(base + offset));
}

const char* rank_label(std::uint8_t rank) noexcept {
    switch (rank) {
        case 2: return "2";
        case 3: return "3";
        case 4: return "4";
        case 5: return "5";
        case 6: return "6";
        case 7: return "7";
        case 8: return "8";
        case 9: return "9";
        case 10: return "T";
        case 11: return "J";
        case 12: return "Q";
        case 13: return "K";
        case 14: return "A";
        default: return "?";
    }
}

char suit_letter(engine::Suit suit) noexcept {
    switch (suit) {
        case engine::Suit::Clubs: return 'C';
        case engine::Suit::Diamonds: return 'D';
        case engine::Suit::Hearts: return 'H';
        case engine::Suit::Spades: return 'S';
    }
    return '?';
}

bool suit_is_red(engine::Suit suit) noexcept {
    return suit == engine::Suit::Hearts || suit == engine::Suit::Diamonds;
}

theme::ColorToken suit_color_token(engine::Suit suit) noexcept {
    return suit_is_red(suit) ? theme::ColorToken::ChipRed : theme::ColorToken::ChipBlack;
}

void draw_card(ImDrawList* dl, float x, float y, engine::Card card, float scale) {
    if (dl == nullptr) {
        return;
    }
    const float w = kCardWidth * scale;
    const float h = kCardHeight * scale;
    const ImVec2 tl{x, y};
    const ImVec2 br{x + w, y + h};

    // Card-face PNG via the shared texture-bind seam; real art renders today as a
    // placeholder and on swap-in with no code change.
    if (bridge::draw_asset_image(dl, tl, br, card_asset_id(card))) {
        return;
    }

    // Procedural fallback (art unavailable / native test build).
    dl->AddRectFilled(tl, br, token_u32(theme::ColorToken::ChipWhite), kCardRounding * scale);
    dl->AddRect(tl, br, token_u32(theme::ColorToken::BorderDefault), kCardRounding * scale, 0, 1.5f);

    const ImU32 pip = token_u32(suit_color_token(card.suit));
    const char* rank = rank_label(card.rank);
    const char suit[2] = {suit_letter(card.suit), '\0'};

    const ImVec2 rank_sz = ImGui::CalcTextSize(rank);
    dl->AddText(ImVec2{x + (w - rank_sz.x) * 0.5f, y + h * 0.18f}, pip, rank);
    const ImVec2 suit_sz = ImGui::CalcTextSize(suit);
    dl->AddText(ImVec2{x + (w - suit_sz.x) * 0.5f, y + h * 0.52f}, pip, suit);
}

void draw_card_fan(ImDrawList* dl, float center_x, float top_y, const engine::Card* cards,
                   std::uint8_t count, float scale) {
    if (dl == nullptr || cards == nullptr || count == 0) {
        return;
    }
    const float step = kCardFanStep * scale;
    const float span = step * static_cast<float>(count - 1) + kCardWidth * scale;
    float x = center_x - span * 0.5f;
    for (std::uint8_t i = 0; i < count; ++i) {
        draw_card(dl, x, top_y, cards[i], scale);
        x += step;
    }
}

}  // namespace poker_trainer::render
