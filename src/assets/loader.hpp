#pragma once

// Zone 02 PNG loader.
//
// decode_png turns a buffer of encoded PNG bytes (as delivered by the fetch
// seam) into CPU-side RGBA8 TextureData. PNG decoding is done with stb_image;
// the single STB_IMAGE_IMPLEMENTATION lives in loader.cpp. Decoding is forced
// to four channels so every decoded texture is uniform RGBA8, regardless of the
// source PNG's channel count.

#include "assets/texture.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace poker_trainer::assets {

// Why a decode attempt failed. A decode failure is an expected outcome (a
// truncated download, a corrupt file), not an exceptional one, so it is
// reported by value rather than thrown.
enum class DecodeError : std::uint8_t {
    EmptyInput,    // the byte span was empty
    DecodeFailed,  // stb_image could not parse the data as an image
};

// Decode encoded PNG bytes into an RGBA8 texture. On success the returned
// TextureData owns a width*height*4 pixel buffer. Not noexcept: it allocates
// the decoded pixel buffer.
[[nodiscard]] std::expected<TextureData, DecodeError> decode_png(
    std::span<const std::byte> encoded);

}  // namespace poker_trainer::assets
