#include "render/hud.hpp"

#include "engine/scenario.hpp"
#include "theme/theme_tokens.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <format>
#include <string>

#include <imgui.h>

namespace poker_trainer::render {

namespace {

[[nodiscard]] ImU32 token_u32(theme::ColorToken token) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(token));
}

// Left-aligned readout at an explicit pixel size (the atlas size is canvas-
// independent, so every readout passes its own size rather than inheriting one).
void text_at(ImDrawList* dl, float x, float y, float px, theme::ColorToken color,
             const char* text) {
    dl->AddText(ImGui::GetFont(), px, ImVec2{x, y}, token_u32(color), text);
}

// Horizontally-centered readout at an explicit pixel size, top-anchored at `y`.
void centered_text_at(ImDrawList* dl, float cx, float y, float px, theme::ColorToken color,
                      const char* text) {
    ImFont* font = ImGui::GetFont();
    const ImVec2 sz = font->CalcTextSizeA(px, FLT_MAX, 0.0f, text);
    dl->AddText(font, px, ImVec2{cx - sz.x * 0.5f, y}, token_u32(color), text);
}

}  // namespace

float canvas_ui_scale() {
    const ImVec2 canvas = ImGui::GetMainViewport()->Size;
    return canvas_ui_scale_for(canvas.x, canvas.y);
}

float readout_font_size(float rel) {
    return std::max(kReadoutBaseSize * rel * canvas_ui_scale(), ImGui::GetFontSize());
}

float readout_line_height(float rel) {
    return readout_font_size(rel) + ImGui::GetStyle().ItemSpacing.y;
}

std::string format_amount(int dollars, bool cash_mode, int big_blind) {
    if (cash_mode || big_blind <= 0) {
        return std::format("${}", dollars);
    }
    const double bb = static_cast<double>(dollars) / static_cast<double>(big_blind);
    if (bb == std::floor(bb)) {
        return std::format("{} BB", static_cast<long long>(bb));
    }
    return std::format("{:.1f} BB", bb);
}

float draw_pot_size(ImDrawList* dl, float x, float y, const engine::ScenarioState& scenario,
                    bool cash_mode, bool show_hud) {
    const float line = readout_line_height(kReadoutRelPrimary);
    if (dl == nullptr || !show_hud) {
        return line;
    }
    const std::string text =
        std::format("Pot {}", format_amount(scenario.pot, cash_mode, scenario.big_blind));
    text_at(dl, x, y, readout_font_size(kReadoutRelPrimary), theme::ColorToken::TextPrimary,
            text.c_str());
    return line;
}

float draw_blinds(ImDrawList* dl, float x, float y, const engine::ScenarioState& scenario,
                  bool cash_mode, bool show_hud) {
    const float line = readout_line_height(kReadoutRelPrimary);
    if (dl == nullptr || !show_hud) {
        return line;
    }
    const std::string sb = format_amount(scenario.small_blind, cash_mode, scenario.big_blind);
    const std::string bb = format_amount(scenario.big_blind, cash_mode, scenario.big_blind);
    const std::string text = std::format("Blinds {} / {}", sb, bb);
    text_at(dl, x, y, readout_font_size(kReadoutRelPrimary), theme::ColorToken::TextSecondary,
            text.c_str());
    return line;
}

float draw_to_call(ImDrawList* dl, float x, float y, const engine::ScenarioState& scenario,
                   bool cash_mode, bool show_hud) {
    const float line = readout_line_height(kReadoutRelPrimary);
    // Caller scenarios only: faced_bet is the amount the user is calling (0 for the
    // Aggressor, who faces no bet). HUD-gated exactly like the pot total and the
    // floating call number, so toggling the HUD off hides this too and never leaves
    // the call amount sitting in the corner during the chip-counting drill.
    if (dl == nullptr || !show_hud || scenario.type != engine::ScenarioType::Caller) {
        return line;
    }
    const std::string text =
        std::format("To Call: {}", format_amount(scenario.faced_bet, cash_mode, scenario.big_blind));
    text_at(dl, x, y, readout_font_size(kReadoutRelPrimary), theme::ColorToken::TextPrimary,
            text.c_str());
    return line;
}

void draw_floating_bet(ImDrawList* dl, float anchor_x, float anchor_y, int bet_dollars,
                       bool cash_mode, int big_blind, bool show_hud) {
    if (dl == nullptr || !show_hud) {
        return;
    }
    const std::string text = format_amount(bet_dollars, cash_mode, big_blind);
    const float px = readout_font_size(kReadoutRelPrimary);
    text_at(dl, anchor_x, anchor_y - px * 0.5f, px, theme::ColorToken::TextPrimary, text.c_str());
}

namespace {

// The opponent fold % for the tier currently on screen, as a whole-number percent.
// Clamps the tier index defensively; the value comes straight from the engine truth
// (tiers[t].fold_probability), so the on-table and top-left readouts always agree.
[[nodiscard]] int opp_fold_pct(const engine::ScenarioState& scenario,
                               std::uint8_t viewed_tier) noexcept {
    const auto idx =
        std::min(static_cast<std::size_t>(viewed_tier), engine::kBetTierCount - 1);
    return static_cast<int>(std::lround(scenario.tiers[idx].fold_probability * 100.0));
}

}  // namespace

float draw_opp_fold(ImDrawList* dl, float x, float y, const engine::ScenarioState& scenario,
                    std::uint8_t viewed_tier, bool show_hud) {
    const float line = readout_line_height(kReadoutRelPrimary);
    // Aggressor scenarios only: the opponent's situational fold frequency is given
    // scenario data (the mirror of the Caller's To Call line). HUD-gated exactly like
    // the pot total and the To Call line.
    if (dl == nullptr || !show_hud || !engine::is_aggressor(scenario.type)) {
        return line;
    }
    const std::string text = std::format("Opp Fold: {}%", opp_fold_pct(scenario, viewed_tier));
    text_at(dl, x, y, readout_font_size(kReadoutRelPrimary), theme::ColorToken::TextPrimary,
            text.c_str());
    return line;
}

void draw_table_opp_fold(ImDrawList* dl, float anchor_x, float anchor_y,
                         const engine::ScenarioState& scenario, std::uint8_t viewed_tier,
                         bool show_hud) {
    if (dl == nullptr || !show_hud || !engine::is_aggressor(scenario.type)) {
        return;
    }
    const std::string text = std::format("Opp fold: {}%", opp_fold_pct(scenario, viewed_tier));
    centered_text_at(dl, anchor_x, anchor_y, readout_font_size(kReadoutRelPrimary),
                     theme::ColorToken::TextPrimary, text.c_str());
}

}  // namespace poker_trainer::render
