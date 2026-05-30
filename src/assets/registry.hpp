#pragma once

// Zone 02 asset registry.
//
// The registry is the single store of per-asset load state and decoded texture
// data. It is a plain owned object, not a global singleton: Zone 05 (the boot /
// main-loop owner) constructs one AssetRegistry and passes a reference to the
// render zones and to the TierLoader. This keeps asset state explicitly owned,
// per CLAUDE.md section 10 ("state lives in objects with explicit ownership").
//
// State transitions are driven by the TierLoader:
//   NotLoaded --mark_loading--> Loading --store--------> Loaded
//                                       \--mark_unavailable--> Unavailable
// reset()/reset_all() return entries to NotLoaded (e.g. before a re-load).

#include "assets/asset_paths.hpp"
#include "assets/texture.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace poker_trainer::assets {

// The lifecycle state of one asset in the registry.
enum class AssetState : std::uint8_t {
    NotLoaded,    // never requested, or reset
    Loading,      // a fetch is in flight
    Loaded,       // decoded pixel data is available via get_texture
    Unavailable,  // all load attempts failed; the asset will not be retried
};

class AssetRegistry {
public:
    AssetRegistry() = default;

    // --- TierLoader-driven transitions ---

    // Mark the asset as having an in-flight fetch.
    void mark_loading(AssetId id) noexcept;

    // Store decoded pixel data and transition the asset to Loaded.
    void store(AssetId id, TextureData data) noexcept;

    // Mark the asset as permanently failed and release any held pixel data.
    void mark_unavailable(AssetId id) noexcept;

    // Return a single asset (or all assets) to NotLoaded, releasing pixel data.
    void reset(AssetId id) noexcept;
    void reset_all() noexcept;

    // --- Queries (the Zone 02 exports) ---

    [[nodiscard]] AssetState state(AssetId id) const noexcept;
    [[nodiscard]] bool is_loaded(AssetId id) const noexcept;
    [[nodiscard]] bool is_unavailable(AssetId id) const noexcept;

    // A handle viewing the asset's decoded pixels. If the asset is not Loaded
    // the returned handle reports valid()==false. The handle stays valid until
    // the asset is reset, re-stored, or marked unavailable.
    [[nodiscard]] TextureHandle get_texture(AssetId id) const noexcept;

private:
    struct Entry {
        AssetState state = AssetState::NotLoaded;
        TextureData data{};
    };

    std::array<Entry, kAssetCount> entries_{};
};

}  // namespace poker_trainer::assets
