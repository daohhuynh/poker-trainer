// Zone 08 — chip decomposition + denomination-legend mapping unit tests.
//
// Pure logic only (no live ImGui frame): the greedy decomposition, the
// stake-tier selection, and the per-tier / Fixed-mode denomination sets.
//
// NOTE on ARCHITECTURE's worked chip examples: the $437 example IS greedy-
// consistent over the {1,5,25,100} set (4 black + 1 green + 2 red + 2 white) and
// is asserted below. ARCHITECTURE's "$40 -> 8 red" and "$50 -> 1 green + 5 red"
// examples are NOT greedy (greedy yields 1 green + 3 red, and 2 green); the prompt
// and ARCHITECTURE both state greedy-merge as the algorithm, so these tests assert
// greedy. The example inconsistency is surfaced in the build report.

#include "render/chips.hpp"

#include "settings/settings.hpp"
#include "theme/theme_tokens.hpp"

#include <span>
#include <vector>

#include <gtest/gtest.h>

namespace rnd = poker_trainer::render;
namespace th = poker_trainer::theme;
namespace st = poker_trainer::settings;

namespace {

std::vector<int> values(std::span<const rnd::Denomination> set) {
    std::vector<int> v;
    for (const rnd::Denomination& d : set) {
        v.push_back(d.value);
    }
    return v;
}

}  // namespace

TEST(StakeTier, SelectedByBigBlind) {
    EXPECT_EQ(rnd::active_stake_tier(1), rnd::StakeTier::Micro);
    EXPECT_EQ(rnd::active_stake_tier(2), rnd::StakeTier::Micro);   // live app blinds 1/2
    EXPECT_EQ(rnd::active_stake_tier(5), rnd::StakeTier::Micro);
    EXPECT_EQ(rnd::active_stake_tier(10), rnd::StakeTier::Mid);
    EXPECT_EQ(rnd::active_stake_tier(50), rnd::StakeTier::Mid);
    EXPECT_EQ(rnd::active_stake_tier(100), rnd::StakeTier::High);
    EXPECT_EQ(rnd::active_stake_tier(500), rnd::StakeTier::High);
    EXPECT_EQ(rnd::active_stake_tier(1000), rnd::StakeTier::Nosebleed);
    EXPECT_EQ(rnd::active_stake_tier(5000), rnd::StakeTier::Nosebleed);
}

TEST(DenominationSet, StakeScaledTierSets) {
    const auto scaled = st::ChipDenominationMode::StakeScaled;
    EXPECT_EQ(values(rnd::denomination_set(2, scaled)), (std::vector<int>{1, 5, 25, 100}));
    EXPECT_EQ(values(rnd::denomination_set(10, scaled)), (std::vector<int>{5, 25, 100, 500}));
    EXPECT_EQ(values(rnd::denomination_set(100, scaled)),
              (std::vector<int>{25, 100, 500, 1000, 5000}));
    EXPECT_EQ(values(rnd::denomination_set(1000, scaled)),
              (std::vector<int>{100, 500, 1000, 5000, 25000}));
}

TEST(DenominationSet, FixedModeIsConstantRegardlessOfBlind) {
    const auto fixed = st::ChipDenominationMode::Fixed;
    const std::vector<int> want{1, 5, 25, 100, 500};
    EXPECT_EQ(values(rnd::denomination_set(2, fixed)), want);
    EXPECT_EQ(values(rnd::denomination_set(1000, fixed)), want);
}

TEST(DenominationSet, ColorsFollowTheCardroomLadder) {
    const auto micro = rnd::denomination_set(2, st::ChipDenominationMode::StakeScaled);
    ASSERT_EQ(micro.size(), 4u);
    EXPECT_EQ(micro[0].color, th::ColorToken::ChipWhite);   // $1
    EXPECT_EQ(micro[1].color, th::ColorToken::ChipRed);     // $5
    EXPECT_EQ(micro[2].color, th::ColorToken::ChipGreen);   // $25
    EXPECT_EQ(micro[3].color, th::ColorToken::ChipBlack);   // $100
}

TEST(Decompose, ArchitectureWorkedExample437OverMicroSet) {
    const auto set = rnd::denomination_set(2, st::ChipDenominationMode::StakeScaled);
    const std::vector<rnd::ChipColumn> cols = rnd::decompose(437, set);
    ASSERT_EQ(cols.size(), 4u);
    // Descending denomination order: 4x$100, 1x$25, 2x$5, 2x$1 = $437.
    EXPECT_EQ(cols[0].denom.value, 100);
    EXPECT_EQ(cols[0].count, 4);
    EXPECT_EQ(cols[1].denom.value, 25);
    EXPECT_EQ(cols[1].count, 1);
    EXPECT_EQ(cols[2].denom.value, 5);
    EXPECT_EQ(cols[2].count, 2);
    EXPECT_EQ(cols[3].denom.value, 1);
    EXPECT_EQ(cols[3].count, 2);
}

TEST(Decompose, PureSingleDenominationColumn) {
    const auto set = rnd::denomination_set(2, st::ChipDenominationMode::StakeScaled);
    const std::vector<rnd::ChipColumn> cols = rnd::decompose(1000, set);
    ASSERT_EQ(cols.size(), 1u);
    EXPECT_EQ(cols[0].denom.value, 100);
    EXPECT_EQ(cols[0].count, 10);  // ARCHITECTURE: $1000 in pure $100s = 10 black
}

TEST(Decompose, GreedyOverMidSet) {
    const auto set = rnd::denomination_set(10, st::ChipDenominationMode::StakeScaled);  // {5,25,100,500}
    const std::vector<rnd::ChipColumn> cols = rnd::decompose(50, set);
    ASSERT_EQ(cols.size(), 1u);
    EXPECT_EQ(cols[0].denom.value, 25);
    EXPECT_EQ(cols[0].count, 2);  // greedy: 2x$25 (not the non-greedy 1 green + 5 red)
}

TEST(Decompose, DescendingOrderAndExactSum) {
    const auto set = rnd::denomination_set(2, st::ChipDenominationMode::StakeScaled);
    const std::vector<rnd::ChipColumn> cols = rnd::decompose(189, set);  // 1x100,3x25,2x5,4x1
    int sum = 0;
    int prev = 1'000'000;
    for (const rnd::ChipColumn& c : cols) {
        EXPECT_LT(c.denom.value, prev);  // strictly descending
        prev = c.denom.value;
        sum += c.denom.value * c.count;
    }
    EXPECT_EQ(sum, 189);
}

TEST(Decompose, NonPositiveAndSubMinRemainder) {
    const auto micro = rnd::denomination_set(2, st::ChipDenominationMode::StakeScaled);
    EXPECT_TRUE(rnd::decompose(0, micro).empty());
    EXPECT_TRUE(rnd::decompose(-50, micro).empty());
    const auto mid = rnd::denomination_set(10, st::ChipDenominationMode::StakeScaled);  // min $5
    EXPECT_TRUE(rnd::decompose(3, mid).empty());  // 3 < smallest denomination ($5)
}

// chip_asset_id maps each theme-fixed chip color token to its chip-face asset.
// The mapping underwrites the swap-in-zero-code invariant (right chip color ->
// right PNG), so the eight cardroom denominations are pinned here.
TEST(ChipAssetId, EachLadderColorMapsToItsChipFace) {
    namespace ag = poker_trainer::assets;
    EXPECT_EQ(rnd::chip_asset_id(th::ColorToken::ChipWhite), ag::AssetId::ChipWhite);
    EXPECT_EQ(rnd::chip_asset_id(th::ColorToken::ChipRed), ag::AssetId::ChipRed);
    EXPECT_EQ(rnd::chip_asset_id(th::ColorToken::ChipGreen), ag::AssetId::ChipGreen);
    EXPECT_EQ(rnd::chip_asset_id(th::ColorToken::ChipBlack), ag::AssetId::ChipBlack);
    EXPECT_EQ(rnd::chip_asset_id(th::ColorToken::ChipPurple), ag::AssetId::ChipPurple);
    EXPECT_EQ(rnd::chip_asset_id(th::ColorToken::ChipYellow), ag::AssetId::ChipYellow);
    EXPECT_EQ(rnd::chip_asset_id(th::ColorToken::ChipBrown), ag::AssetId::ChipBrown);
    EXPECT_EQ(rnd::chip_asset_id(th::ColorToken::ChipGold), ag::AssetId::ChipGold);
}
