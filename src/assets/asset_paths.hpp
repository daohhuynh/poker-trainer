#pragma once

#include "assets/tier_config.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace poker_trainer::assets {

// Identifiers for every PNG asset in the trainer. The enum value
// is also the index into kAssetEntries below.
enum class AssetId : std::uint16_t {
    // --- Tier 1 ---
    AppLogo = 0,
    RootBackgroundNoLimit = 1,
    RootBackgroundSlate = 2,
    RootBackgroundOcean = 3,
    RootBackgroundSage = 4,
    DealerButton = 5,

    // --- Tier 2: Butler (side profile, Game screen) ---
    ButlerSideProfile = 6,

    // --- Tier 2: Card faces (52 cards) ---
    // Order: Spades A-K, Hearts A-K, Diamonds A-K, Clubs A-K
    CardSpadeA = 7, CardSpade2 = 8, CardSpade3 = 9, CardSpade4 = 10,
    CardSpade5 = 11, CardSpade6 = 12, CardSpade7 = 13, CardSpade8 = 14,
    CardSpade9 = 15, CardSpadeT = 16, CardSpadeJ = 17, CardSpadeQ = 18,
    CardSpadeK = 19,
    CardHeartA = 20, CardHeart2 = 21, CardHeart3 = 22, CardHeart4 = 23,
    CardHeart5 = 24, CardHeart6 = 25, CardHeart7 = 26, CardHeart8 = 27,
    CardHeart9 = 28, CardHeartT = 29, CardHeartJ = 30, CardHeartQ = 31,
    CardHeartK = 32,
    CardDiamondA = 33, CardDiamond2 = 34, CardDiamond3 = 35, CardDiamond4 = 36,
    CardDiamond5 = 37, CardDiamond6 = 38, CardDiamond7 = 39, CardDiamond8 = 40,
    CardDiamond9 = 41, CardDiamondT = 42, CardDiamondJ = 43, CardDiamondQ = 44,
    CardDiamondK = 45,
    CardClubA = 46, CardClub2 = 47, CardClub3 = 48, CardClub4 = 49,
    CardClub5 = 50, CardClub6 = 51, CardClub7 = 52, CardClub8 = 53,
    CardClub9 = 54, CardClubT = 55, CardClubJ = 56, CardClubQ = 57,
    CardClubK = 58,

    // --- Tier 2: Card back ---
    CardBack = 59,

    // --- Tier 2: Table felt ---
    TableFelt = 60,

    // --- Tier 2: Chip denomination faces ---
    ChipWhite = 61,
    ChipRed = 62,
    ChipGreen = 63,
    ChipBlack = 64,
    ChipPurple = 65,
    ChipYellow = 66,
    ChipBrown = 67,
    ChipGold = 68,

    // --- Tier 2: Cluster icons ---
    IconShop = 69,
    IconHelp = 70,
    IconSettings = 71,
    IconHome = 72,
    IconClose = 73,  // The X icon

    // --- Tier 2: Front-facing Butler (Post-Round Screen) ---
    ButlerFrontNeutral = 74,
    ButlerFrontRaised = 75,

    // --- Tier 3: Position indicators ---
    PositionUTG = 76,
    PositionHJ = 77,
    PositionCO = 78,
    PositionBTN = 79,
    PositionSB = 80,
    PositionBB = 81,

    // --- Tier 3: Side pot all-in marker ---
    SidePotAllInMarker = 82,

    // --- Tier 4: Frog easter egg ---
    FrogSideProfile = 83,
    FrogFrontNeutral = 84,
    FrogFrontRaised = 85,
    FrogExpressionPass = 86,
    FrogExpressionFail = 87,
    FrogExpressionOvertime = 88,
    FrogExpressionPerfect = 89,
};

inline constexpr std::size_t kAssetCount = 90;

// Metadata for a single asset.
struct AssetEntry {
    std::string_view path;
    AssetTier tier;
};

// Per-asset metadata indexed by AssetId. The path field gives the
// asset-root-relative path to the PNG file. The tier field tags
// the asset for the tier loader.
inline constexpr std::array<AssetEntry, kAssetCount> kAssetEntries = {{
    // --- Tier 1 ---
    {"assets/images/tier1/app_logo.png",                   AssetTier::Tier1},
    {"assets/images/tier1/root_bg_no_limit.png",           AssetTier::Tier1},
    {"assets/images/tier1/root_bg_slate.png",              AssetTier::Tier1},
    {"assets/images/tier1/root_bg_ocean.png",              AssetTier::Tier1},
    {"assets/images/tier1/root_bg_sage.png",               AssetTier::Tier1},
    {"assets/images/tier1/dealer_button.png",              AssetTier::Tier1},

    // --- Tier 2: Butler side profile ---
    {"assets/images/tier2/butler_side_profile.png",        AssetTier::Tier2},

    // --- Tier 2: Cards (52) ---
    {"assets/images/tier2/cards/spade_a.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/spade_2.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/spade_3.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/spade_4.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/spade_5.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/spade_6.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/spade_7.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/spade_8.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/spade_9.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/spade_t.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/spade_j.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/spade_q.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/spade_k.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/heart_a.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/heart_2.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/heart_3.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/heart_4.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/heart_5.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/heart_6.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/heart_7.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/heart_8.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/heart_9.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/heart_t.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/heart_j.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/heart_q.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/heart_k.png",   AssetTier::Tier2},
    {"assets/images/tier2/cards/diamond_a.png", AssetTier::Tier2},
    {"assets/images/tier2/cards/diamond_2.png", AssetTier::Tier2},
    {"assets/images/tier2/cards/diamond_3.png", AssetTier::Tier2},
    {"assets/images/tier2/cards/diamond_4.png", AssetTier::Tier2},
    {"assets/images/tier2/cards/diamond_5.png", AssetTier::Tier2},
    {"assets/images/tier2/cards/diamond_6.png", AssetTier::Tier2},
    {"assets/images/tier2/cards/diamond_7.png", AssetTier::Tier2},
    {"assets/images/tier2/cards/diamond_8.png", AssetTier::Tier2},
    {"assets/images/tier2/cards/diamond_9.png", AssetTier::Tier2},
    {"assets/images/tier2/cards/diamond_t.png", AssetTier::Tier2},
    {"assets/images/tier2/cards/diamond_j.png", AssetTier::Tier2},
    {"assets/images/tier2/cards/diamond_q.png", AssetTier::Tier2},
    {"assets/images/tier2/cards/diamond_k.png", AssetTier::Tier2},
    {"assets/images/tier2/cards/club_a.png",    AssetTier::Tier2},
    {"assets/images/tier2/cards/club_2.png",    AssetTier::Tier2},
    {"assets/images/tier2/cards/club_3.png",    AssetTier::Tier2},
    {"assets/images/tier2/cards/club_4.png",    AssetTier::Tier2},
    {"assets/images/tier2/cards/club_5.png",    AssetTier::Tier2},
    {"assets/images/tier2/cards/club_6.png",    AssetTier::Tier2},
    {"assets/images/tier2/cards/club_7.png",    AssetTier::Tier2},
    {"assets/images/tier2/cards/club_8.png",    AssetTier::Tier2},
    {"assets/images/tier2/cards/club_9.png",    AssetTier::Tier2},
    {"assets/images/tier2/cards/club_t.png",    AssetTier::Tier2},
    {"assets/images/tier2/cards/club_j.png",    AssetTier::Tier2},
    {"assets/images/tier2/cards/club_q.png",    AssetTier::Tier2},
    {"assets/images/tier2/cards/club_k.png",    AssetTier::Tier2},

    // --- Tier 2: Card back ---
    {"assets/images/tier2/cards/back.png",                 AssetTier::Tier2},

    // --- Tier 2: Table felt ---
    {"assets/images/tier2/table_felt.png",                 AssetTier::Tier2},

    // --- Tier 2: Chip faces ---
    {"assets/images/tier2/chips/chip_white.png",           AssetTier::Tier2},
    {"assets/images/tier2/chips/chip_red.png",             AssetTier::Tier2},
    {"assets/images/tier2/chips/chip_green.png",           AssetTier::Tier2},
    {"assets/images/tier2/chips/chip_black.png",           AssetTier::Tier2},
    {"assets/images/tier2/chips/chip_purple.png",          AssetTier::Tier2},
    {"assets/images/tier2/chips/chip_yellow.png",          AssetTier::Tier2},
    {"assets/images/tier2/chips/chip_brown.png",           AssetTier::Tier2},
    {"assets/images/tier2/chips/chip_gold.png",            AssetTier::Tier2},

    // --- Tier 2: Cluster icons ---
    {"assets/images/tier2/icons/shop.png",                 AssetTier::Tier2},
    {"assets/images/tier2/icons/help.png",                 AssetTier::Tier2},
    {"assets/images/tier2/icons/settings.png",             AssetTier::Tier2},
    {"assets/images/tier2/icons/home.png",                 AssetTier::Tier2},
    {"assets/images/tier2/icons/close.png",                AssetTier::Tier2},

    // --- Tier 2: Front-facing Butler ---
    {"assets/images/tier2/butler_front_neutral.png",       AssetTier::Tier2},
    {"assets/images/tier2/butler_front_raised.png",        AssetTier::Tier2},

    // --- Tier 3: Position indicators ---
    {"assets/images/tier3/positions/utg.png",              AssetTier::Tier3},
    {"assets/images/tier3/positions/hj.png",               AssetTier::Tier3},
    {"assets/images/tier3/positions/co.png",               AssetTier::Tier3},
    {"assets/images/tier3/positions/btn.png",              AssetTier::Tier3},
    {"assets/images/tier3/positions/sb.png",               AssetTier::Tier3},
    {"assets/images/tier3/positions/bb.png",               AssetTier::Tier3},

    // --- Tier 3: Side pot all-in marker ---
    {"assets/images/tier3/side_pot_all_in_marker.png",     AssetTier::Tier3},

    // --- Tier 4: Frog easter egg ---
    {"assets/images/tier4/frog_side_profile.png",          AssetTier::Tier4},
    {"assets/images/tier4/frog_front_neutral.png",         AssetTier::Tier4},
    {"assets/images/tier4/frog_front_raised.png",          AssetTier::Tier4},
    {"assets/images/tier4/frog_expression_pass.png",       AssetTier::Tier4},
    {"assets/images/tier4/frog_expression_fail.png",       AssetTier::Tier4},
    {"assets/images/tier4/frog_expression_overtime.png",   AssetTier::Tier4},
    {"assets/images/tier4/frog_expression_perfect.png",    AssetTier::Tier4},
}};

// Helper to look up the entry for a given AssetId.
[[nodiscard]] constexpr const AssetEntry& asset_entry(AssetId id) noexcept {
    return kAssetEntries[static_cast<std::size_t>(id)];
}

// Helper to look up just the path.
[[nodiscard]] constexpr std::string_view asset_path(AssetId id) noexcept {
    return kAssetEntries[static_cast<std::size_t>(id)].path;
}

// Helper to look up just the tier.
[[nodiscard]] constexpr AssetTier asset_tier(AssetId id) noexcept {
    return kAssetEntries[static_cast<std::size_t>(id)].tier;
}

}  // namespace poker_trainer::assets
