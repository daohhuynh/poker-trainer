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

// Seat-readout shadow offset, as a fraction of the type size (so it tracks the
// canvas scale like the type does).
inline constexpr float kSeatTextShadowFrac = 0.08f;

// Text centered on (cx, cy) at an explicit pixel size, over an offset shadow in
// bg_primary. The seat readouts are the only text drawn straight onto the table,
// and a seat near the rim can land over bright room art rather than dark felt, so
// the shadow is what keeps them legible there. bg_primary is the darkest token and
// is dark in all four themes, so this stays a token reference, not a literal.
void centered_text(ImDrawList* dl, float cx, float cy, theme::ColorToken color,
                   const std::string& text, float px) {
    ImFont* font = ImGui::GetFont();
    const ImVec2 sz = font->CalcTextSizeA(px, FLT_MAX, 0.0f, text.c_str());
    const ImVec2 pos{cx - sz.x * 0.5f, cy - sz.y * 0.5f};
    const float offset = std::max(1.0f, px * kSeatTextShadowFrac);
    dl->AddText(font, px, ImVec2{pos.x + offset, pos.y + offset},
                token_u32(theme::ColorToken::BgPrimary), text.c_str());
    dl->AddText(font, px, pos, token_u32(color), text.c_str());
}

// The seat's chip columns recede at the full perspective scale (kFarSeatScale at
// the far rim, 1.0 nearest the camera). Its stack amount and position letters are
// text the user has to READ across the felt, so they take a compressed share of
// that perspective instead: the far seats still read as smaller, but the glyphs
// never drop to the size where they stop resolving against the felt.
inline constexpr float kSeatTextFarScale = 0.82f;

[[nodiscard]] float seat_text_scale(float seat_scale) noexcept {
    const float depth = (seat_scale - kFarSeatScale) / (1.0f - kFarSeatScale);
    return kSeatTextFarScale + depth * (1.0f - kSeatTextFarScale);
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
        // Chip geometry takes the felt-relative multiplier as well; type does not,
        // since readout_font_size already scales type off the canvas.
        const float chip_sc = sc * layout.chip_scale;
        const float text_sc = seat_text_scale(sc);
        const float label_px = readout_font_size(kReadoutRelSecondary) * text_sc;

        // The hero seat (slot 0) shows its position label only; its hole cards
        // render dead-center at the bottom nearest the camera (drawn by game_screen).
        // Anchored just BELOW the seat point so the enlarged label clears the hole
        // cards, whose bottom edge sits on it.
        if (slot == 0) {
            centered_text(dl, c.x, c.y + label_px * 0.5f, theme::ColorToken::TextPrimary,
                          position_abbrev(seat), label_px);
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
            draw_chip_cluster(dl, cluster_base_x(c.x, cols, chip_sc), c.y, cols, chip_sc);

            int tallest = 0;
            for (const ChipColumn& col : cols) {
                tallest = std::max(tallest, std::min(col.count, kMaxChipsPerColumn));
            }
            const float stack_top =
                c.y - (static_cast<float>(tallest) * kChipStackStep + kChipRadius) * chip_sc;
            const std::string amount = format_amount(seat_stack, cash_mode, scenario.big_blind);
            // Clear the amount off the top chip by half its own (now larger) height
            // plus a gap, rather than by a chip-scaled offset that shrank as the type
            // did not — the far seats' numbers used to sit on their chips.
            const float amount_px = readout_font_size(kReadoutRelPrimary) * text_sc;
            centered_text(dl, c.x, stack_top - amount_px * 0.5f - 4.0f,
                          theme::ColorToken::TextPrimary, amount, amount_px);
        }
        // The position letter takes text_primary, not the subdued token: it is always
        // on (not HUD-gated) and, read across the felt from a receded seat, it is the
        // lowest-contrast element on the table.
        centered_text(dl, c.x, c.y + kChipRadius * chip_sc + label_px * 0.5f + 4.0f,
                      theme::ColorToken::TextPrimary, position_abbrev(seat), label_px);
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
    const float px = readout_font_size(kReadoutRelSecondary) * seat_text_scale(spot.scale);
    centered_text(dl, c.x, c.y + marker_r + 6.0f + px * 0.5f, theme::ColorToken::StateFail,
                  "ALL-IN", px);
}

}  // namespace poker_trainer::render
