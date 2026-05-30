#pragma once

// Zone 02 CPU-side texture types.
//
// The asset pipeline decodes PNGs to RGBA8 pixel data on the CPU and hands the
// render layer a lightweight handle. Zone 02 never creates GPU textures: there
// is no GL context in the native test build, and GPU upload plus ImTextureID
// binding are the render layer's responsibility (Z05/Z08). TextureData owns the
// decoded bytes; TextureHandle is a non-owning view the render layer uploads.

#include "assets/asset_paths.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace poker_trainer::assets {

// A decoded, CPU-resident RGBA8 image. Owns its pixel buffer. Move-only: a
// texture's pixels are allocated once at decode time and never silently copied.
class TextureData {
public:
    static constexpr std::uint32_t kChannels = 4;  // always RGBA8

    TextureData() = default;
    TextureData(std::uint32_t width, std::uint32_t height,
                std::vector<std::byte> pixels) noexcept
        : width_{width}, height_{height}, pixels_{std::move(pixels)} {}

    TextureData(const TextureData&) = delete;
    TextureData& operator=(const TextureData&) = delete;
    TextureData(TextureData&&) noexcept = default;
    TextureData& operator=(TextureData&&) noexcept = default;
    ~TextureData() = default;

    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }

    // Total bytes the pixel buffer should hold for a well-formed RGBA8 image.
    [[nodiscard]] std::size_t expected_byte_count() const noexcept {
        return static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) *
               static_cast<std::size_t>(kChannels);
    }

    [[nodiscard]] bool empty() const noexcept { return pixels_.empty(); }
    [[nodiscard]] std::span<const std::byte> pixels() const noexcept { return pixels_; }

private:
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::vector<std::byte> pixels_{};
};

// A lightweight, copyable, non-owning view of a registry-resident texture: the
// opaque handle the render layer receives from AssetRegistry::get_texture. The
// viewed pixels stay valid as long as the owning AssetRegistry keeps the asset
// loaded. A default-constructed handle, or one for an asset that is not loaded,
// reports valid()==false; callers must check valid() before reading pixels.
class TextureHandle {
public:
    TextureHandle() = default;
    TextureHandle(AssetId id, const TextureData& data) noexcept : id_{id}, data_{&data} {}

    [[nodiscard]] bool valid() const noexcept { return data_ != nullptr; }
    [[nodiscard]] AssetId id() const noexcept { return id_; }
    [[nodiscard]] std::uint32_t width() const noexcept { return data_ != nullptr ? data_->width() : 0u; }
    [[nodiscard]] std::uint32_t height() const noexcept { return data_ != nullptr ? data_->height() : 0u; }

    [[nodiscard]] std::span<const std::byte> pixels() const noexcept {
        return data_ != nullptr ? data_->pixels() : std::span<const std::byte>{};
    }

private:
    AssetId id_{};
    const TextureData* data_ = nullptr;
};

}  // namespace poker_trainer::assets
