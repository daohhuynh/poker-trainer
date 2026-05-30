#include "assets/registry.hpp"

#include "assets/asset_paths.hpp"
#include "assets/texture.hpp"

#include <cstddef>
#include <utility>

namespace poker_trainer::assets {

namespace {

// AssetId enumerators are contiguous indices [0, kAssetCount); the helper keeps
// the static_cast in one place and documents the invariant.
[[nodiscard]] std::size_t index_of(AssetId id) noexcept {
    return static_cast<std::size_t>(id);
}

}  // namespace

void AssetRegistry::mark_loading(AssetId id) noexcept {
    Entry& entry = entries_[index_of(id)];
    entry.state = AssetState::Loading;
}

void AssetRegistry::store(AssetId id, TextureData data) noexcept {
    Entry& entry = entries_[index_of(id)];
    entry.data = std::move(data);
    entry.state = AssetState::Loaded;
}

void AssetRegistry::mark_unavailable(AssetId id) noexcept {
    Entry& entry = entries_[index_of(id)];
    entry.data = TextureData{};
    entry.state = AssetState::Unavailable;
}

void AssetRegistry::reset(AssetId id) noexcept {
    Entry& entry = entries_[index_of(id)];
    entry.data = TextureData{};
    entry.state = AssetState::NotLoaded;
}

void AssetRegistry::reset_all() noexcept {
    for (Entry& entry : entries_) {
        entry.data = TextureData{};
        entry.state = AssetState::NotLoaded;
    }
}

AssetState AssetRegistry::state(AssetId id) const noexcept {
    return entries_[index_of(id)].state;
}

bool AssetRegistry::is_loaded(AssetId id) const noexcept {
    return entries_[index_of(id)].state == AssetState::Loaded;
}

bool AssetRegistry::is_unavailable(AssetId id) const noexcept {
    return entries_[index_of(id)].state == AssetState::Unavailable;
}

TextureHandle AssetRegistry::get_texture(AssetId id) const noexcept {
    const Entry& entry = entries_[index_of(id)];
    if (entry.state != AssetState::Loaded) {
        return TextureHandle{};
    }
    return TextureHandle{id, entry.data};
}

}  // namespace poker_trainer::assets
