#pragma once

#include "assets/tier_config.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace poker_trainer::assets {

// Identifiers for every PNG asset in the trainer. The enum value
// is also the index into kAssetEntries below.
//
// Tiers follow ARCHITECTURE Module 3's loading strategy. Tier 1 is the
// synchronous Root-screen set (Root background, dealer button, Home icon,
// front-facing Butler). Tier 2 is the background set fetched after Root
// renders (Mode/Game backgrounds, side-profile Butler, table, cards, chips,
// cluster + Post-Round glyphs, all-in marker). Tier 3 carries no PNGs (SFX
// and music only). Tier 4 is the on-demand Frog easter-egg set.
//
// The 4 Root UI buttons (PLAY / etc.) are theme-rendered ImGui widgets, not
// baked PNGs, so they are intentionally absent here. Seat positions are HUD
// text labels, not assets, so they are absent as well.
enum class AssetId : std::uint16_t {
    // --- Tier 1: synchronous initial load (Root screen) ---
    AppLogo = 0,
    BackgroundRoot,   // background_root.png (heavily-blurred Root variant)
    DealerButton,
    IconHome,         // promoted to Tier 1 (returns to Root from Mode/Post-Round)
    ButlerNeutral,    // front-facing Butler, pass expression
    ButlerRaised,     // front-facing Butler, fail expression

    // --- Tier 2: background load (after Root renders) ---
    BackgroundMode,   // background_mode.png (Mode Selection + Post-Round)
    BackgroundGame,   // background_game.png (Game screen)
    ButlerProfile,    // side-profile Butler, Game screen

    // --- Tier 2: Card faces (52 cards) ---
    // Order: Spades A-K, Hearts A-K, Diamonds A-K, Clubs A-K
    CardSpadeA, CardSpade2, CardSpade3, CardSpade4,
    CardSpade5, CardSpade6, CardSpade7, CardSpade8,
    CardSpade9, CardSpadeT, CardSpadeJ, CardSpadeQ,
    CardSpadeK,
    CardHeartA, CardHeart2, CardHeart3, CardHeart4,
    CardHeart5, CardHeart6, CardHeart7, CardHeart8,
    CardHeart9, CardHeartT, CardHeartJ, CardHeartQ,
    CardHeartK,
    CardDiamondA, CardDiamond2, CardDiamond3, CardDiamond4,
    CardDiamond5, CardDiamond6, CardDiamond7, CardDiamond8,
    CardDiamond9, CardDiamondT, CardDiamondJ, CardDiamondQ,
    CardDiamondK,
    CardClubA, CardClub2, CardClub3, CardClub4,
    CardClub5, CardClub6, CardClub7, CardClub8,
    CardClub9, CardClubT, CardClubJ, CardClubQ,
    CardClubK,

    // --- Tier 2: Card back ---
    CardBack,

    // --- Tier 2: Table felt ---
    TableFelt,

    // --- Tier 2: Chip denomination faces ---
    ChipWhite,
    ChipRed,
    ChipGreen,
    ChipBlack,
    ChipPurple,
    ChipYellow,
    ChipBrown,
    ChipGold,

    // --- Tier 2: Cluster + Post-Round glyphs ---
    IconShop,
    IconHelp,
    IconSettings,
    IconClose,         // The X icon (Game-screen cluster)
    IconExit,          // Exit door glyph (Post-Round)
    IconCopy,          // Copy glyph (Post-Round Scenario ID)
    IconShare,         // Share glyph (Post-Round Scenario ID)
    IconTomato,        // currency icon (Shop / Profile / Leaderboard)
    IconSidePotChip,   // side-pot stacked-chip icon (Post-Round stat-modal Overall row)
    IconOffline,       // offline sync indicator (cloud-with-slash), left of the cluster
    IconWarning,       // Service Outage Banner warning glyph
    IconTrophy,        // Leaderboard view header glyph

    // --- Tier 2: Side pot all-in marker (table-side) ---
    SidePotAllInMarker,

    // --- Tier 4: Frog easter egg (base + two expression overlays) ---
    FrogBase,
    FrogExpressionPass,
    FrogExpressionFail,
};

inline constexpr std::size_t kAssetCount = 87;

// Cross-check: the enum is contiguous from 0, so the last enumerator + 1 is
// the asset count. Keep this in lock-step with the kAssetEntries array size.
static_assert(static_cast<std::size_t>(AssetId::FrogExpressionFail) + 1 == kAssetCount,
              "kAssetCount must equal the number of AssetId enumerators");

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
    {"assets/images/tier1/background_root.png",            AssetTier::Tier1},
    {"assets/images/tier1/dealer_button.png",              AssetTier::Tier1},
    {"assets/images/tier1/icons/home.png",                 AssetTier::Tier1},
    {"assets/images/tier1/butler_neutral.png",             AssetTier::Tier1},
    {"assets/images/tier1/butler_raised.png",              AssetTier::Tier1},

    // --- Tier 2: Backgrounds + side-profile Butler ---
    {"assets/images/tier2/background_mode.png",            AssetTier::Tier2},
    {"assets/images/tier2/background_game.png",            AssetTier::Tier2},
    {"assets/images/tier2/butler_profile.png",             AssetTier::Tier2},

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

    // --- Tier 2: Cluster + Post-Round glyphs ---
    {"assets/images/tier2/icons/shop.png",                 AssetTier::Tier2},
    {"assets/images/tier2/icons/help.png",                 AssetTier::Tier2},
    {"assets/images/tier2/icons/settings.png",             AssetTier::Tier2},
    {"assets/images/tier2/icons/close.png",                AssetTier::Tier2},
    {"assets/images/tier2/icons/exit.png",                 AssetTier::Tier2},
    {"assets/images/tier2/icons/copy.png",                 AssetTier::Tier2},
    {"assets/images/tier2/icons/share.png",                AssetTier::Tier2},
    {"assets/images/tier2/icons/tomato.png",               AssetTier::Tier2},
    {"assets/images/tier2/icons/side_pot_chip.png",        AssetTier::Tier2},
    {"assets/images/tier2/icons/offline.png",              AssetTier::Tier2},
    {"assets/images/tier2/icons/warning.png",              AssetTier::Tier2},
    {"assets/images/tier2/icons/trophy.png",               AssetTier::Tier2},

    // --- Tier 2: Side pot all-in marker ---
    {"assets/images/tier2/side_pot_all_in_marker.png",     AssetTier::Tier2},

    // --- Tier 4: Frog easter egg ---
    {"assets/images/tier4/frog_base.png",                  AssetTier::Tier4},
    {"assets/images/tier4/frog_expression_pass.png",       AssetTier::Tier4},
    {"assets/images/tier4/frog_expression_fail.png",       AssetTier::Tier4},
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
