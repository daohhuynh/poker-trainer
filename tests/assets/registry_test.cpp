// Zone 02 asset registry tests.
//
// Covers the state machine (NotLoaded -> Loading -> Loaded / Unavailable), the
// get_texture handle's validity and contents, reset behavior, and that per-asset
// state is independent.

#include "assets/registry.hpp"

#include "assets/asset_paths.hpp"
#include "assets/texture.hpp"

#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

namespace {

namespace pa = poker_trainer::assets;

pa::TextureData make_texture(std::uint32_t w, std::uint32_t h, std::byte fill) {
    std::vector<std::byte> pixels(static_cast<std::size_t>(w) * h * 4u, fill);
    return pa::TextureData{w, h, std::move(pixels)};
}

TEST(AssetRegistry, DefaultStateIsNotLoaded) {
    const pa::AssetRegistry reg;
    EXPECT_EQ(reg.state(pa::AssetId::AppLogo), pa::AssetState::NotLoaded);
    EXPECT_FALSE(reg.is_loaded(pa::AssetId::AppLogo));
    EXPECT_FALSE(reg.is_unavailable(pa::AssetId::AppLogo));
    EXPECT_FALSE(reg.get_texture(pa::AssetId::AppLogo).valid());
}

TEST(AssetRegistry, MarkLoadingIsNeitherLoadedNorUnavailable) {
    pa::AssetRegistry reg;
    reg.mark_loading(pa::AssetId::DealerButton);
    EXPECT_EQ(reg.state(pa::AssetId::DealerButton), pa::AssetState::Loading);
    EXPECT_FALSE(reg.is_loaded(pa::AssetId::DealerButton));
    EXPECT_FALSE(reg.is_unavailable(pa::AssetId::DealerButton));
    EXPECT_FALSE(reg.get_texture(pa::AssetId::DealerButton).valid());
}

TEST(AssetRegistry, StoreTransitionsToLoadedAndExposesPixels) {
    pa::AssetRegistry reg;
    reg.mark_loading(pa::AssetId::ChipRed);
    reg.store(pa::AssetId::ChipRed, make_texture(8, 4, std::byte{0xAB}));

    EXPECT_EQ(reg.state(pa::AssetId::ChipRed), pa::AssetState::Loaded);
    EXPECT_TRUE(reg.is_loaded(pa::AssetId::ChipRed));
    EXPECT_FALSE(reg.is_unavailable(pa::AssetId::ChipRed));

    const pa::TextureHandle handle = reg.get_texture(pa::AssetId::ChipRed);
    ASSERT_TRUE(handle.valid());
    EXPECT_EQ(handle.id(), pa::AssetId::ChipRed);
    EXPECT_EQ(handle.width(), 8u);
    EXPECT_EQ(handle.height(), 4u);
    ASSERT_EQ(handle.pixels().size(), 8u * 4u * 4u);
    EXPECT_EQ(handle.pixels()[0], std::byte{0xAB});
    EXPECT_EQ(handle.pixels().back(), std::byte{0xAB});
}

TEST(AssetRegistry, MarkUnavailableReleasesDataAndInvalidatesHandle) {
    pa::AssetRegistry reg;
    reg.store(pa::AssetId::CardSpadeA, make_texture(2, 2, std::byte{0x10}));
    ASSERT_TRUE(reg.is_loaded(pa::AssetId::CardSpadeA));

    reg.mark_unavailable(pa::AssetId::CardSpadeA);
    EXPECT_EQ(reg.state(pa::AssetId::CardSpadeA), pa::AssetState::Unavailable);
    EXPECT_TRUE(reg.is_unavailable(pa::AssetId::CardSpadeA));
    EXPECT_FALSE(reg.is_loaded(pa::AssetId::CardSpadeA));
    EXPECT_FALSE(reg.get_texture(pa::AssetId::CardSpadeA).valid());
}

TEST(AssetRegistry, ResetReturnsToNotLoaded) {
    pa::AssetRegistry reg;
    reg.store(pa::AssetId::IconShop, make_texture(2, 2, std::byte{0x01}));
    reg.reset(pa::AssetId::IconShop);
    EXPECT_EQ(reg.state(pa::AssetId::IconShop), pa::AssetState::NotLoaded);
    EXPECT_FALSE(reg.get_texture(pa::AssetId::IconShop).valid());
}

TEST(AssetRegistry, ResetAllClearsEveryEntry) {
    pa::AssetRegistry reg;
    reg.store(pa::AssetId::IconShop, make_texture(2, 2, std::byte{0x01}));
    reg.mark_unavailable(pa::AssetId::IconHelp);
    reg.reset_all();
    EXPECT_EQ(reg.state(pa::AssetId::IconShop), pa::AssetState::NotLoaded);
    EXPECT_EQ(reg.state(pa::AssetId::IconHelp), pa::AssetState::NotLoaded);
}

TEST(AssetRegistry, PerAssetStateIsIndependent) {
    pa::AssetRegistry reg;
    reg.store(pa::AssetId::ChipWhite, make_texture(2, 2, std::byte{0x01}));
    reg.mark_unavailable(pa::AssetId::ChipBlack);
    reg.mark_loading(pa::AssetId::ChipGold);

    EXPECT_TRUE(reg.is_loaded(pa::AssetId::ChipWhite));
    EXPECT_TRUE(reg.is_unavailable(pa::AssetId::ChipBlack));
    EXPECT_EQ(reg.state(pa::AssetId::ChipGold), pa::AssetState::Loading);
    // An untouched asset stays NotLoaded.
    EXPECT_EQ(reg.state(pa::AssetId::ChipGreen), pa::AssetState::NotLoaded);
}

TEST(AssetRegistry, StoreOverwritesPreviousData) {
    pa::AssetRegistry reg;
    reg.store(pa::AssetId::TableFelt, make_texture(2, 2, std::byte{0x01}));
    reg.store(pa::AssetId::TableFelt, make_texture(4, 4, std::byte{0x02}));
    const pa::TextureHandle handle = reg.get_texture(pa::AssetId::TableFelt);
    ASSERT_TRUE(handle.valid());
    EXPECT_EQ(handle.width(), 4u);
    EXPECT_EQ(handle.height(), 4u);
    EXPECT_EQ(handle.pixels()[0], std::byte{0x02});
}

}  // namespace
