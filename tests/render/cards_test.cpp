// Zone 08 — card-face asset mapping unit test.
//
// Pure logic only (no live ImGui frame): card_asset_id maps an engine::Card to
// its card-face asset. The mapping underwrites the swap-in-zero-code invariant
// (right card -> right PNG), so a representative spread of suits and the
// boundary ranks (Ace, deuce, King) are pinned here. Asset order is Spades A-K,
// Hearts A-K, Diamonds A-K, Clubs A-K; within a suit the rank order is
// A, 2..9, T, J, Q, K (asset_paths.hpp).

#include "render/cards.hpp"

#include "assets/asset_paths.hpp"
#include "engine/scenario.hpp"

#include <gtest/gtest.h>

namespace rnd = poker_trainer::render;
namespace eng = poker_trainer::engine;
namespace ag = poker_trainer::assets;

TEST(CardAssetId, SuitBlocksAndRankOrder) {
    // Ace is the first card of each suit's block.
    EXPECT_EQ(rnd::card_asset_id(eng::Card{14, eng::Suit::Spades}), ag::AssetId::CardSpadeA);
    EXPECT_EQ(rnd::card_asset_id(eng::Card{14, eng::Suit::Hearts}), ag::AssetId::CardHeartA);
    EXPECT_EQ(rnd::card_asset_id(eng::Card{14, eng::Suit::Diamonds}), ag::AssetId::CardDiamondA);
    EXPECT_EQ(rnd::card_asset_id(eng::Card{14, eng::Suit::Clubs}), ag::AssetId::CardClubA);

    // The deuce follows the Ace; the King closes the block.
    EXPECT_EQ(rnd::card_asset_id(eng::Card{2, eng::Suit::Spades}), ag::AssetId::CardSpade2);
    EXPECT_EQ(rnd::card_asset_id(eng::Card{13, eng::Suit::Spades}), ag::AssetId::CardSpadeK);

    // A mid rank in a later block.
    EXPECT_EQ(rnd::card_asset_id(eng::Card{10, eng::Suit::Diamonds}), ag::AssetId::CardDiamondT);
    EXPECT_EQ(rnd::card_asset_id(eng::Card{11, eng::Suit::Clubs}), ag::AssetId::CardClubJ);
}
