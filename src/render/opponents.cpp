#include "render/opponents.hpp"

#include "render/chips.hpp"
#include "render/hud.hpp"
#include "render/layout.hpp"
#include "render/render_constants.hpp"

#include "engine/scenario.hpp"
#include "theme/theme_tokens.hpp"

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <imgui.h>

#include "assets/asset_paths.hpp"
#include "bridge/asset_image.hpp"

namespace poker_trainer::render {

namespace {

[[nodiscard]] ImU32 token_u32(theme::ColorToken token) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(token));
}

// Centered text at `scale` of the base font size, so a receded far seat's label /
// amount shrinks with its chips (perspective). scale == 1 is the unscaled size.
void centered_text(ImDrawList* dl, float cx, float cy, theme::ColorToken color,
                   const std::string& text, float scale = 1.0f) {
    ImFont* font = ImGui::GetFont();
    const float size = ImGui::GetFontSize() * scale;
    const ImVec2 sz = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str());
    dl->AddText(font, size, ImVec2{cx - sz.x * 0.5f, cy - sz.y * 0.5f}, token_u32(color),
                text.c_str());
}

}  // namespace

const char* position_abbrev(engine::Position position) noexcept {
    switch (position) {
        case engine::Position::UnderTheGun: return "UTG";
        case engine::Position::Hijack: return "HJ";
        case engine::Position::Cutoff: return "CO";
        case engine::Position::Button: return "BTN";
        case engine::Position::SmallBlind: return "SB";
        case engine::Position::BigBlind: return "BB";
    }
    return "";
}

int opponent_display_stack(const engine::ScenarioState& scenario, engine::Position seat) noexcept {
    // splitmix64-style mix of (id, seat) -> a round big-blind multiple. Purely
    // cosmetic and reproducible per id; the graded math uses scenario.effective_stack.
    std::uint64_t x = scenario.id.value +
                      0x9E3779B97F4A7C15ull * (static_cast<std::uint64_t>(seat) + 1ull);
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    x ^= x >> 31;
    // Varied stake depths around the hero's: short (15 BB) to deep (300 BB), in the
    // engine's 5-BB steps so the amounts stay round.
    constexpr int kMinBb = 15;
    constexpr int kMaxBb = 300;
    constexpr int kStepBb = 5;
    constexpr int kSteps = (kMaxBb - kMinBb) / kStepBb + 1;
    const int bb = kMinBb + kStepBb * static_cast<int>(x % static_cast<std::uint64_t>(kSteps));
    const int unit = scenario.big_blind > 0 ? scenario.big_blind : 1;
    return bb * unit;
}

OpponentChipState opponent_chip_state(engine::ScenarioType type) noexcept {
    return type == engine::ScenarioType::Caller ? OpponentChipState::PushedForward
                                                : OpponentChipState::Empty;
}

int active_opponent_slot() noexcept {
    // The seat directly across from the hero — slot 4 is the far top-center seat in
    // seat_spot's rim mapping — so a Caller's pushed chips read straight down toward
    // the pot.
    return 4;
}

void draw_opponent_seats(ImDrawList* dl, const GameLayout& layout,
                         const engine::ScenarioState& scenario,
                         std::span<const Denomination> denom_set, bool cash_mode, bool show_hud) {
    if (dl == nullptr) {
        return;
    }
    for (std::uint8_t i = 0; i < engine::kPositionCount; ++i) {
        const auto seat = static_cast<engine::Position>(i);
        const int slot = seat_slot(seat, scenario.position);
        const SeatSpot spot = seat_spot(layout, slot);
        const Pt c = spot.pos;
        const float sc = spot.scale;  // perspective size scale (smaller the farther away)

        // The hero seat (slot 0) shows its position label only; its hole cards
        // render dead-center at the bottom nearest the camera (drawn by game_screen).
        if (slot == 0) {
            centered_text(dl, c.x, c.y, theme::ColorToken::TextSecondary, position_abbrev(seat));
            continue;
        }

        // Opponent stack: this seat's OWN display stack (varied per seat; the graded
        // implied-odds math uses scenario.effective_stack) as a greedy chip cluster on
        // the felt (the SAME decomposition the pot uses), scaled by the seat's depth so
        // far seats recede, with a floating HUD amount above it. Both honor Show/Hide
        // HUD — the opponent stack is a HUD element; the chips the user counts when the
        // HUD is off are the pot and the faced bet (ARCHITECTURE Settings: Show/hide
        // HUD). The position label always shows.
        if (show_hud) {
            const int seat_stack = opponent_display_stack(scenario, seat);
            const std::vector<ChipColumn> cols = decompose(seat_stack, denom_set);
            draw_chip_cluster(dl, cluster_base_x(c.x, cols.size(), sc), c.y, cols, sc);

            int tallest = 0;
            for (const ChipColumn& col : cols) {
                tallest = std::max(tallest, std::min(col.count, kMaxChipsPerColumn));
            }
            const float stack_top =
                c.y - (static_cast<float>(tallest) * kChipStackStep + kChipRadius) * sc;
            const std::string amount = format_amount(seat_stack, cash_mode, scenario.big_blind);
            centered_text(dl, c.x, stack_top - 8.0f * sc, theme::ColorToken::TextPrimary, amount, sc);
        }
        centered_text(dl, c.x, c.y + (kChipRadius + 8.0f) * sc, theme::ColorToken::TextSecondary,
                      position_abbrev(seat), sc);
    }
}

void draw_all_in_marker(ImDrawList* dl, const GameLayout& layout, int slot) {
    if (dl == nullptr) {
        return;
    }
    const SeatSpot spot = seat_spot(layout, slot);
    const Pt c = spot.pos;
    const float marker_r = 26.0f * spot.scale;  // recede with the seat
    // All-in marker glyph (side_pot_all_in_marker.png) via the shared seam; a
    // colored ring is the fallback when the art is unavailable.
    if (!bridge::draw_asset_image(dl, ImVec2{c.x - marker_r, c.y - marker_r},
                                  ImVec2{c.x + marker_r, c.y + marker_r},
                                  assets::AssetId::SidePotAllInMarker)) {
        dl->AddCircle(ImVec2{c.x, c.y}, marker_r, token_u32(theme::ColorToken::AccentPrimary), 32,
                      3.0f);
    }
    centered_text(dl, c.x, c.y + marker_r + 6.0f, theme::ColorToken::StateFail, "ALL-IN",
                  spot.scale);
}

}  // namespace poker_trainer::render
