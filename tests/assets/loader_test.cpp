// Zone 02 PNG loader tests.
//
// Decodes every committed placeholder through decode_png and asserts it is a
// valid PNG of the dimensions the generator wrote (the placeholder_layout.hpp
// single source of truth). Also covers the two rejection paths: empty input and
// non-image bytes. This doubles as proof that the hand-rolled placeholder PNG
// writer emits files stb_image accepts.

#include "assets/loader.hpp"

#include "assets/asset_paths.hpp"
#include "assets/texture.hpp"
#include "tools/placeholder_layout.hpp"

#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "asset_test_util.hpp"

namespace {

namespace pa = poker_trainer::assets;
namespace pt = poker_trainer::assets::test;

TEST(DecodePng, EveryPlaceholderDecodesToExpectedDimensions) {
    for (std::size_t i = 0; i < pa::kAssetCount; ++i) {
        const auto id = static_cast<pa::AssetId>(i);
        const std::filesystem::path file = pt::asset_file(id);
        ASSERT_TRUE(std::filesystem::exists(file))
            << "missing placeholder: " << file << " (run gen_placeholders)";

        const std::vector<std::byte> bytes = pt::read_file_bytes(file);
        const auto decoded = pa::decode_png(bytes);
        ASSERT_TRUE(decoded.has_value()) << "decode failed for " << file;

        const pa::placeholder::Size expected = pa::placeholder::size_for(id);
        EXPECT_EQ(decoded->width(), expected.width) << file;
        EXPECT_EQ(decoded->height(), expected.height) << file;
        // RGBA8: four bytes per pixel.
        EXPECT_EQ(decoded->pixels().size(),
                  static_cast<std::size_t>(expected.width) * expected.height * 4u)
            << file;
        EXPECT_EQ(decoded->pixels().size(), decoded->expected_byte_count()) << file;
    }
}

TEST(DecodePng, EmptyInputIsRejected) {
    const std::vector<std::byte> empty;
    const auto decoded = pa::decode_png(empty);
    ASSERT_FALSE(decoded.has_value());
    EXPECT_EQ(decoded.error(), pa::DecodeError::EmptyInput);
}

TEST(DecodePng, NonImageBytesAreRejected) {
    const std::vector<std::byte> junk{std::byte{'n'}, std::byte{'o'}, std::byte{'t'},
                                      std::byte{'p'}, std::byte{'n'}, std::byte{'g'}};
    const auto decoded = pa::decode_png(junk);
    ASSERT_FALSE(decoded.has_value());
    EXPECT_EQ(decoded.error(), pa::DecodeError::DecodeFailed);
}

TEST(DecodePng, DecodedTextureForcesFourChannels) {
    // The app logo placeholder is an opaque fill; force-to-RGBA must still yield
    // a four-channel buffer.
    const auto bytes = pt::read_asset_bytes(pa::AssetId::AppLogo);
    const auto decoded = pa::decode_png(bytes);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(pa::TextureData::kChannels, 4u);
    EXPECT_EQ(decoded->pixels().size(), decoded->expected_byte_count());
}

}  // namespace
