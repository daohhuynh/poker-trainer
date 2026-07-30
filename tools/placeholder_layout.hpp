#pragma once

// The declared pixel dimensions of every shipped PNG, shared by the generator
// (tools/gen_placeholders.cpp) and the decode test (tests/assets/loader_test.cpp)
// so the two never disagree about how large an asset should be.
//
// These were the generated stand-ins' sizes until real art landed; they now
// record the real art's. The decode test asserts the shipped PNGs match, which
// catches a truncated or mis-exported asset.
//
// Three files must agree on these numbers and there is no compile-time link
// between them, so change them together:
//   - this header                    (C++ generator + decode test)
//   - tools/rasterize_assets.py      (SVG -> PNG export, 84 vector assets)
//   - tools/derive_backgrounds.py    (the 3 photo-derived backgrounds)

#include "assets/asset_paths.hpp"

#include <cstdint>

namespace poker_trainer::assets::placeholder {

struct Size {
    std::uint32_t width;
    std::uint32_t height;
};

[[nodiscard]] constexpr Size size_for(AssetId id) noexcept {
    using A = AssetId;

    // Card faces (52) and the card back share the 5:7 playing-card ratio.
    if ((id >= A::CardSpadeA && id <= A::CardClubK) || id == A::CardBack) {
        return {400, 560};
    }
    if (id >= A::ChipWhite && id <= A::ChipGold) {
        return {512, 512};
    }
    // Cluster + Post-Round glyphs (Shop..SidePotChip) are square icons.
    if (id >= A::IconShop && id <= A::IconSidePotChip) {
        return {256, 256};
    }

    switch (id) {
        case A::AppLogo:
            // Portrait monogram (P + t + chip), not the old wide wordmark.
            return {420, 1024};
        case A::DealerButton:
            return {512, 512};
        case A::TableFelt:
            // Not 16:9 — the felt is authored in the renderer's foreshortened
            // racetrack projection and this is the aspect of that shape's bounding
            // box (tools/gen_table_felt.py prints it). Reshaping the table changes
            // this number; keep it in step with rasterize_assets.py.
            return {2048, 1577};
        case A::IconHome:
            return {256, 256};
        // The three room backgrounds are stored at a resolution matched to their
        // blur radius rather than all at full size: a heavy blur destroys the
        // detail that resolution would preserve, so background_root needs far
        // fewer pixels than background_game. See tools/derive_backgrounds.py.
        case A::BackgroundRoot:
            return {720, 405};
        case A::BackgroundMode:
            // Same parameters as Root: the two menus share one background.
            return {720, 405};
        case A::BackgroundGame:
            // Not photo-derived: a generated pool of light, which upscales cleanly.
            return {480, 270};
        case A::SidePotAllInMarker:
            return {512, 512};
        case A::ButlerProfile:
            // Taller than the front views: the side profile has legs and runs off
            // the bottom of the Game screen.
            return {1024, 1800};
        case A::ButlerNeutral:
        case A::ButlerRaised:
            return {1024, 1536};
        case A::FrogBase:
        case A::FrogExpressionPass:
        case A::FrogExpressionFail:
            return {1024, 1024};
        default:
            return {256, 256};
    }
}

}  // namespace poker_trainer::assets::placeholder
