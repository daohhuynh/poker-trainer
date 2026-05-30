#pragma once

// Placeholder dimensions, shared by the generator (tools/gen_placeholders.cpp)
// and the decode test (tests/assets/loader_test.cpp) so the two never disagree
// about how large a generated stand-in PNG should be. The spec pins no exact
// pixel dimensions for these assets; the values here are coherent placeholder
// sizes (correct aspect ratios for cards/backgrounds) that real art overwrites
// without any code change.

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
        return {60, 84};
    }
    if (id >= A::ChipWhite && id <= A::ChipGold) {
        return {96, 96};
    }
    if (id >= A::IconShop && id <= A::IconClose) {
        return {48, 48};
    }
    if (id >= A::PositionUTG && id <= A::PositionBB) {
        return {48, 48};
    }
    if (id >= A::RootBackgroundNoLimit && id <= A::RootBackgroundSage) {
        return {256, 144};  // 16:9
    }

    switch (id) {
        case A::AppLogo:
            return {320, 120};
        case A::DealerButton:
            return {200, 200};
        case A::TableFelt:
            return {320, 180};  // 16:9
        case A::SidePotAllInMarker:
            return {64, 64};
        case A::ButlerSideProfile:
        case A::ButlerFrontNeutral:
        case A::ButlerFrontRaised:
        case A::FrogSideProfile:
        case A::FrogFrontNeutral:
        case A::FrogFrontRaised:
        case A::FrogExpressionPass:
        case A::FrogExpressionFail:
        case A::FrogExpressionOvertime:
        case A::FrogExpressionPerfect:
            return {160, 240};  // 2:3 portrait character frame
        default:
            return {64, 64};
    }
}

}  // namespace poker_trainer::assets::placeholder
