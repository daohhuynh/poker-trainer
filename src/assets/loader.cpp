#include "assets/loader.hpp"

#include "assets/texture.hpp"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

// stb_image is the approved PNG decoder (CLAUDE.md section 3). The single
// translation-unit implementation lives here, in the asset loader's .cpp, per
// the same section. The header is included via the SYSTEM include path so the
// implementation's own diagnostics never trip the project -Werror baseline.
//
// STBI_NO_STDIO drops the file-based entry points: Zone 02 only ever decodes
// from in-memory buffers handed over by the fetch seam. STBI_ONLY_PNG limits
// the build to the one format the trainer ships. STBI_ASSERT is routed away
// from <cassert> so a malformed image can never abort the process — decode
// failures surface as a DecodeError return value instead.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ASSERT(x) ((void)0)
#include <stb_image.h>

namespace poker_trainer::assets {

std::expected<TextureData, DecodeError> decode_png(std::span<const std::byte> encoded) {
    if (encoded.empty()) {
        return std::unexpected{DecodeError::EmptyInput};
    }

    // stb_image counts in int; guard against a buffer larger than it can index.
    if (encoded.size() > static_cast<std::size_t>(INT_MAX)) {
        return std::unexpected{DecodeError::DecodeFailed};
    }

    int width = 0;
    int height = 0;
    int source_channels = 0;
    const auto* data = reinterpret_cast<const stbi_uc*>(encoded.data());
    const int length = static_cast<int>(encoded.size());

    // Force 4-channel RGBA output so every decoded texture is uniform regardless
    // of the source PNG's channel count.
    stbi_uc* pixels =
        stbi_load_from_memory(data, length, &width, &height, &source_channels,
                              static_cast<int>(TextureData::kChannels));
    if (pixels == nullptr || width <= 0 || height <= 0) {
        if (pixels != nullptr) {
            stbi_image_free(pixels);
        }
        return std::unexpected{DecodeError::DecodeFailed};
    }

    const auto pixel_width = static_cast<std::uint32_t>(width);
    const auto pixel_height = static_cast<std::uint32_t>(height);
    const std::size_t byte_count = static_cast<std::size_t>(pixel_width) *
                                   static_cast<std::size_t>(pixel_height) *
                                   static_cast<std::size_t>(TextureData::kChannels);

    // Copy stb_image's malloc'd buffer into an owned, RAII-managed vector so the
    // pixel lifetime follows TextureData rather than stb_image's allocator.
    const auto* begin = reinterpret_cast<const std::byte*>(pixels);
    std::vector<std::byte> owned(begin, begin + byte_count);
    stbi_image_free(pixels);

    return TextureData{pixel_width, pixel_height, std::move(owned)};
}

}  // namespace poker_trainer::assets
