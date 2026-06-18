#pragma once

#include "settings/settings.hpp"

#include "assets/asset_paths.hpp"
#include "theme/theme_tokens.hpp"

#include <cstdint>
#include <span>
#include <vector>

// Forward-declared so this header (and the pure chip logic below) stays free of
// Dear ImGui: the draw helpers take an ImDrawList* the render TU supplies. The
// unit tests include this header for the pure decomposition / tier math and
// never call the draw helpers, so a pointer to the incomplete type is enough.
struct ImDrawList;

// Zone 08 — chip-stack rendering convention (ARCHITECTURE Game Screen "Chip
// Stack Rendering"). The pure parts (denomination sets, stake-tier selection,
// greedy decomposition) are unit-tested; the draw helpers are render-only.
//
// Denomination -> chip-color mapping is the standard cardroom ladder, fixed
// across every theme (the chip_* tokens are theme-fixed): $1 white, $5 red,
// $25 green, $100 black, $500 purple, $1000 yellow, $5000 brown, $25000 gold.
// It is consistent with ARCHITECTURE's worked $437 example (4 black + 1 green +
// 2 red + 2 white over the {1,5,25,100} set).

namespace poker_trainer::render {

// A chip denomination and the theme-fixed color token its chip renders in.
struct Denomination {
    int value{0};
    theme::ColorToken color{theme::ColorToken::ChipWhite};

    bool operator==(const Denomination&) const noexcept = default;
};

// One single-denomination column within a cluster: which denomination, and how
// many chips stack in the column. Column height is linear in `count`.
struct ChipColumn {
    Denomination denom{};
    int count{0};

    bool operator==(const ChipColumn&) const noexcept = default;
};

// Stake-scaled denomination tiers, chosen by blind level (ARCHITECTURE's
// "Stake-scaled tier table"). The Fixed mode uses a single blind-independent set
// and is not one of these.
enum class StakeTier : std::uint8_t {
    Micro = 0,      // blinds 0.5/1 .. 2/5      -> {1, 5, 25, 100}
    Mid = 1,        // blinds 5/10 .. 25/50     -> {5, 25, 100, 500}
    High = 2,       // blinds 50/100 .. 200/500 -> {25, 100, 500, 1000, 5000}
    Nosebleed = 3,  // blinds 500/1000 and up   -> {100, 500, 1000, 5000, 25000}
};

// Select the stake tier from the big blind (ARCHITECTURE's blind ranges, keyed
// on the big-blind value: Micro <= 5, Mid <= 50, High <= 500, else Nosebleed).
[[nodiscard]] StakeTier active_stake_tier(int big_blind) noexcept;

// The chip-face asset for a denomination's theme-fixed color token (the eight
// cardroom chips: white/red/green/black/purple/yellow/brown/gold). Pure mapping
// (unit-tested); any non-chip token falls back to the white chip.
[[nodiscard]] assets::AssetId chip_asset_id(theme::ColorToken color) noexcept;

// The active denomination set, ascending by value (the legend's left-to-right
// order). Stake-scaled returns the active stake tier's set; Fixed returns the
// constant $1/$5/$25/$100/$500 set regardless of blind level.
[[nodiscard]] std::span<const Denomination> denomination_set(
    int big_blind, settings::ChipDenominationMode mode) noexcept;

// Greedy-merge `amount` (whole dollars, clamped at 0) into single-denomination
// columns over `set`, highest value first. Columns with a zero count are
// omitted. The result is ordered descending by denomination value — the
// left-to-right stack order ARCHITECTURE specifies. A remainder below the
// smallest denomination is dropped (it cannot be represented in chips).
[[nodiscard]] std::vector<ChipColumn> decompose(int amount,
                                                std::span<const Denomination> set);

// Left edge for a chip cluster of `columns` columns centered horizontally on
// `cx`, drawn at `scale` (1.0 = full size; < 1 for a receded far seat). Backs out
// half the scaled column span plus the scaled leading radius. Pure layout math (no
// ImGui); shared by the pot cluster and the per-seat opponent stacks so they
// center identically.
[[nodiscard]] float cluster_base_x(float cx, std::size_t columns, float scale = 1.0f) noexcept;

// ----- Render helpers (game render TU only; ImGui pulled in chips.cpp) -----

// Draw one chip cluster at `scale` (1.0 = full size; < 1 shrinks the chip radius,
// stack step, and column pitch together, for a perspective-receded far seat): the
// columns laid out left-to-right (descending denomination), each a stack of
// `count` filled disks growing upward from (base_x, base_y). Returns the cluster's
// total drawn width so callers can place an adjacent floating label. A null/empty
// column list draws nothing.
float draw_chip_cluster(ImDrawList* dl, float base_x, float base_y,
                        std::span<const ChipColumn> columns, float scale = 1.0f);

// Draw the denomination legend: a horizontal row of chip disks with the dollar
// value in text_secondary beneath each, ascending left-to-right. Returns the row
// width drawn.
float draw_denomination_legend(ImDrawList* dl, float x, float y,
                               std::span<const Denomination> set);

}  // namespace poker_trainer::render
