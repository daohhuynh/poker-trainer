#include "render/chips.hpp"

#include "render/render_constants.hpp"

#include "theme/theme_tokens.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <span>
#include <vector>

#include <imgui.h>

#include "assets/asset_paths.hpp"
#include "bridge/asset_image.hpp"

namespace poker_trainer::render {

namespace {

// The cardroom denomination ladder. Each denomination renders in its theme-fixed
// chip color. Sets below are sub-spans of this ascending table.
inline constexpr std::array<Denomination, 8> kLadder = {{
    {1, theme::ColorToken::ChipWhite},
    {5, theme::ColorToken::ChipRed},
    {25, theme::ColorToken::ChipGreen},
    {100, theme::ColorToken::ChipBlack},
    {500, theme::ColorToken::ChipPurple},
    {1000, theme::ColorToken::ChipYellow},
    {5000, theme::ColorToken::ChipBrown},
    {25000, theme::ColorToken::ChipGold},
}};

// Stake-scaled tier sets (ascending), as sub-ranges of kLadder.
inline constexpr std::array<Denomination, 4> kMicroSet = {
    {kLadder[0], kLadder[1], kLadder[2], kLadder[3]}};  // 1, 5, 25, 100
inline constexpr std::array<Denomination, 4> kMidSet = {
    {kLadder[1], kLadder[2], kLadder[3], kLadder[4]}};  // 5, 25, 100, 500
inline constexpr std::array<Denomination, 5> kHighSet = {
    {kLadder[2], kLadder[3], kLadder[4], kLadder[5], kLadder[6]}};  // 25..5000
inline constexpr std::array<Denomination, 5> kNosebleedSet = {
    {kLadder[3], kLadder[4], kLadder[5], kLadder[6], kLadder[7]}};  // 100..25000

// Fixed mode: constant $1/$5/$25/$100/$500 regardless of blind level.
inline constexpr std::array<Denomination, 5> kFixedSet = {
    {kLadder[0], kLadder[1], kLadder[2], kLadder[3], kLadder[4]}};

[[nodiscard]] ImU32 token_u32(theme::ColorToken token) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(token));
}

}  // namespace

assets::AssetId chip_asset_id(theme::ColorToken color) noexcept {
    switch (color) {
        case theme::ColorToken::ChipWhite:  return assets::AssetId::ChipWhite;
        case theme::ColorToken::ChipRed:    return assets::AssetId::ChipRed;
        case theme::ColorToken::ChipGreen:  return assets::AssetId::ChipGreen;
        case theme::ColorToken::ChipBlack:  return assets::AssetId::ChipBlack;
        case theme::ColorToken::ChipPurple: return assets::AssetId::ChipPurple;
        case theme::ColorToken::ChipYellow: return assets::AssetId::ChipYellow;
        case theme::ColorToken::ChipBrown:  return assets::AssetId::ChipBrown;
        case theme::ColorToken::ChipGold:   return assets::AssetId::ChipGold;
        default:                            return assets::AssetId::ChipWhite;
    }
}

StakeTier active_stake_tier(int big_blind) noexcept {
    if (big_blind <= 5) {
        return StakeTier::Micro;
    }
    if (big_blind <= 50) {
        return StakeTier::Mid;
    }
    if (big_blind <= 500) {
        return StakeTier::High;
    }
    return StakeTier::Nosebleed;
}

std::span<const Denomination> denomination_set(int big_blind,
                                               settings::ChipDenominationMode mode) noexcept {
    if (mode == settings::ChipDenominationMode::Fixed) {
        return kFixedSet;
    }
    switch (active_stake_tier(big_blind)) {
        case StakeTier::Micro:
            return kMicroSet;
        case StakeTier::Mid:
            return kMidSet;
        case StakeTier::High:
            return kHighSet;
        case StakeTier::Nosebleed:
            return kNosebleedSet;
    }
    return kMicroSet;  // unreachable; the switch is exhaustive
}

std::vector<ChipColumn> decompose(int amount, std::span<const Denomination> set) {
    std::vector<ChipColumn> columns;
    if (amount <= 0 || set.empty()) {
        return columns;
    }
    // Greedy from the highest denomination down (descending stack order).
    std::vector<Denomination> sorted(set.begin(), set.end());
    std::ranges::sort(sorted, [](const Denomination& a, const Denomination& b) {
        return a.value > b.value;
    });
    int remaining = amount;
    for (const Denomination& d : sorted) {
        if (d.value <= 0) {
            continue;
        }
        const int count = remaining / d.value;
        if (count > 0) {
            columns.push_back(ChipColumn{d, count});
            remaining -= count * d.value;
        }
    }
    return columns;
}

float cluster_base_x(float cx, std::size_t columns, float scale) noexcept {
    const float pitch = kChipColumnPitch * scale;
    const float radius = kChipRadius * scale;
    const float span = columns > 0 ? static_cast<float>(columns - 1) * pitch : 0.0f;
    return cx - span * 0.5f - radius;
}

float draw_chip_cluster(ImDrawList* dl, float base_x, float base_y,
                        std::span<const ChipColumn> columns, float scale) {
    if (dl == nullptr || columns.empty()) {
        return 0.0f;
    }
    const float radius = kChipRadius * scale;
    const float step = kChipStackStep * scale;
    const float pitch = kChipColumnPitch * scale;
    const ImU32 outline = token_u32(theme::ColorToken::BorderDefault);
    float x = base_x + radius;
    for (const ChipColumn& col : columns) {
        const assets::AssetId chip = chip_asset_id(col.denom.color);
        const ImU32 fill = token_u32(col.denom.color);
        // Cap the drawn chip count so a huge stack stays on-screen; the height is
        // still linear in count up to the cap (a visual guard, not a model change).
        const int drawn = std::min(col.count, kMaxChipsPerColumn);
        for (int i = 0; i < drawn; ++i) {
            const float cy = base_y - static_cast<float>(i) * step;
            // Chip-denomination PNG via the shared texture-bind seam (a column is a
            // stack of these); disk fallback when the art is unavailable.
            if (!bridge::draw_asset_image(dl, ImVec2{x - radius, cy - radius},
                                          ImVec2{x + radius, cy + radius}, chip)) {
                dl->AddCircleFilled(ImVec2{x, cy}, radius, fill, kChipSegments);
                dl->AddCircle(ImVec2{x, cy}, radius, outline, kChipSegments, 1.0f);
            }
        }
        x += pitch;
    }
    return (x - pitch + radius) - base_x;
}

float draw_denomination_legend(ImDrawList* dl, float x, float y,
                               std::span<const Denomination> set) {
    if (dl == nullptr || set.empty()) {
        return 0.0f;
    }
    const ImU32 outline = token_u32(theme::ColorToken::BorderDefault);
    const ImU32 label = token_u32(theme::ColorToken::TextSecondary);
    float cx = x + kChipRadius;
    for (const Denomination& d : set) {
        const ImVec2 center{cx, y + kChipRadius};
        // Legend chip-face PNG via the shared seam; disk fallback when unavailable.
        if (!bridge::draw_asset_image(dl, ImVec2{cx - kChipRadius, y},
                                      ImVec2{cx + kChipRadius, y + kChipRadius * 2.0f},
                                      chip_asset_id(d.color))) {
            dl->AddCircleFilled(center, kChipRadius, token_u32(d.color), kChipSegments);
            dl->AddCircle(center, kChipRadius, outline, kChipSegments, 1.0f);
        }
        const std::string text = std::format("${}", d.value);
        const ImVec2 ts = ImGui::CalcTextSize(text.c_str());
        dl->AddText(ImVec2{cx - ts.x * 0.5f, y + kChipRadius * 2.0f + 2.0f}, label, text.c_str());
        cx += kLegendSlotPitch;
    }
    return (cx - kLegendSlotPitch + kChipRadius) - x;
}

}  // namespace poker_trainer::render
