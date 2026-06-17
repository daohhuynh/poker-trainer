#include "bridge/tier_schedule.hpp"

#include "assets/asset_paths.hpp"
#include "assets/tier_config.hpp"
#include "audio/audio_paths.hpp"
#include "backbone/screen_state.hpp"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstddef>

namespace br = poker_trainer::bridge;
namespace as = poker_trainer::assets;
namespace au = poker_trainer::audio;
namespace bb = poker_trainer::backbone;

// ----- SFX tier split (Module 3: swoosh pair in Tier 2, the rest in Tier 3) ----

TEST(SfxLoadTier, SwooshPairLoadsInTierTwo) {
    EXPECT_EQ(br::sfx_load_tier(au::SfxId::ModalSwooshOpen), as::AssetTier::Tier2);
    EXPECT_EQ(br::sfx_load_tier(au::SfxId::ModalSwooshClose), as::AssetTier::Tier2);
}

TEST(SfxLoadTier, EveryOtherSampleLoadsInTierThree) {
    EXPECT_EQ(br::sfx_load_tier(au::SfxId::CardDeal), as::AssetTier::Tier3);
    EXPECT_EQ(br::sfx_load_tier(au::SfxId::ButtonClickConfirmation), as::AssetTier::Tier3);
    EXPECT_EQ(br::sfx_load_tier(au::SfxId::ChipPush), as::AssetTier::Tier3);
    EXPECT_EQ(br::sfx_load_tier(au::SfxId::SidePotSplit), as::AssetTier::Tier3);
    EXPECT_EQ(br::sfx_load_tier(au::SfxId::FrogToggle), as::AssetTier::Tier3);
    EXPECT_EQ(br::sfx_load_tier(au::SfxId::SlideIn), as::AssetTier::Tier3);
    EXPECT_EQ(br::sfx_load_tier(au::SfxId::SlideOut), as::AssetTier::Tier3);
}

TEST(SfxLoadTier, EverySampleMapsToTierTwoOrThree) {
    // No SFX leaks into a tier that carries none (Tier 1 / Tier 4). Exactly the
    // two swoosh samples are Tier 2.
    std::size_t tier2 = 0;
    std::size_t tier3 = 0;
    for (std::size_t i = 0; i < au::kSfxCount; ++i) {
        const as::AssetTier tier = br::sfx_load_tier(static_cast<au::SfxId>(i));
        ASSERT_TRUE(tier == as::AssetTier::Tier2 || tier == as::AssetTier::Tier3);
        if (tier == as::AssetTier::Tier2) {
            ++tier2;
        } else {
            ++tier3;
        }
    }
    EXPECT_EQ(tier2, 2u);
    EXPECT_EQ(tier3, au::kSfxCount - 2u);
}

// ----- Game-launch required-asset gating (Tier-2 navigation guard) -----

TEST(GameLaunchRequiredAsset, TableAndBackgroundAreRequired) {
    EXPECT_TRUE(br::is_game_launch_required_asset(as::AssetId::BackgroundGame));
    EXPECT_TRUE(br::is_game_launch_required_asset(as::AssetId::TableFelt));
    EXPECT_TRUE(br::is_game_launch_required_asset(as::AssetId::CardBack));
}

TEST(GameLaunchRequiredAsset, AllFiftyTwoCardFacesAreRequired) {
    std::size_t required_cards = 0;
    for (auto v = static_cast<std::uint16_t>(as::AssetId::CardSpadeA);
         v <= static_cast<std::uint16_t>(as::AssetId::CardClubK); ++v) {
        EXPECT_TRUE(br::is_game_launch_required_asset(static_cast<as::AssetId>(v)));
        ++required_cards;
    }
    EXPECT_EQ(required_cards, 52u);
}

TEST(GameLaunchRequiredAsset, NonTableAssetsAreNotGated) {
    // Tier-1 boot assets, Shop-only glyphs, and the Tier-4 Frog set never block
    // a Game launch.
    EXPECT_FALSE(br::is_game_launch_required_asset(as::AssetId::AppLogo));
    EXPECT_FALSE(br::is_game_launch_required_asset(as::AssetId::BackgroundRoot));
    EXPECT_FALSE(br::is_game_launch_required_asset(as::AssetId::IconTomato));
    EXPECT_FALSE(br::is_game_launch_required_asset(as::AssetId::ButlerProfile));
    EXPECT_FALSE(br::is_game_launch_required_asset(as::AssetId::FrogBase));
}

// ----- Tier-3 trigger edge (Root -> Mode Selection) -----

TEST(RootToModeTransition, FiresOnlyOnTheRootToModeEdge) {
    EXPECT_TRUE(br::is_root_to_mode_transition(bb::ScreenId::Root,
                                               bb::ScreenId::ModeSelection));
}

TEST(RootToModeTransition, DoesNotFireOnOtherTransitions) {
    EXPECT_FALSE(br::is_root_to_mode_transition(bb::ScreenId::Root,
                                                bb::ScreenId::Root));
    EXPECT_FALSE(br::is_root_to_mode_transition(bb::ScreenId::Root,
                                                bb::ScreenId::Game));
    EXPECT_FALSE(br::is_root_to_mode_transition(bb::ScreenId::ModeSelection,
                                                bb::ScreenId::Game));
    // Returning to Mode from Game is not the Root-origin edge.
    EXPECT_FALSE(br::is_root_to_mode_transition(bb::ScreenId::Game,
                                                bb::ScreenId::ModeSelection));
}

// ----- Orchestration relies on Zone 02's retry schedule + per-tier disposition --
//
// The retry transport (immediate / 2s / 10s) and the fatal-failure disposition
// are owned and exhaustively tested by Zone 02 (tier_loader_test). These guard
// assertions document the contract the Z05 orchestration is wired against, so a
// silent change to either contract trips a Z05 test too.

TEST(TierDispositionContract, RetryScheduleIsImmediateThenTwoThenTenSeconds) {
    ASSERT_EQ(as::kMaxRetries, 3u);
    EXPECT_EQ(as::kRetryBackoff[0], std::chrono::milliseconds{0});
    EXPECT_EQ(as::kRetryBackoff[1], std::chrono::milliseconds{2000});
    EXPECT_EQ(as::kRetryBackoff[2], std::chrono::milliseconds{10000});
}

TEST(TierDispositionContract, PerTierFatalFailurePolicyMatchesModuleThree) {
    EXPECT_EQ(as::tier_config(as::AssetTier::Tier1).fatal_failure_policy,
              as::FatalFailurePolicy::ErrorScreenImmediate);
    EXPECT_EQ(as::tier_config(as::AssetTier::Tier2).fatal_failure_policy,
              as::FatalFailurePolicy::ErrorScreenOnUse);
    EXPECT_EQ(as::tier_config(as::AssetTier::Tier3).fatal_failure_policy,
              as::FatalFailurePolicy::Silent);
    EXPECT_EQ(as::tier_config(as::AssetTier::Tier4).fatal_failure_policy,
              as::FatalFailurePolicy::Silent);
}
