#include "render/hud.hpp"

#include "engine/scenario.hpp"
#include "theme/theme_tokens.hpp"

#include <cmath>
#include <format>
#include <string>

#include <imgui.h>

namespace poker_trainer::render {

namespace {

[[nodiscard]] ImU32 token_u32(theme::ColorToken token) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(token));
}

}  // namespace

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
    const float line = ImGui::GetTextLineHeightWithSpacing();
    if (dl == nullptr || !show_hud) {
        return line;
    }
    const std::string text =
        std::format("Pot {}", format_amount(scenario.pot, cash_mode, scenario.big_blind));
    dl->AddText(ImVec2{x, y}, token_u32(theme::ColorToken::TextPrimary), text.c_str());
    return line;
}

float draw_blinds(ImDrawList* dl, float x, float y, const engine::ScenarioState& scenario,
                  bool cash_mode, bool show_hud) {
    const float line = ImGui::GetTextLineHeightWithSpacing();
    if (dl == nullptr || !show_hud) {
        return line;
    }
    const std::string sb = format_amount(scenario.small_blind, cash_mode, scenario.big_blind);
    const std::string bb = format_amount(scenario.big_blind, cash_mode, scenario.big_blind);
    const std::string text = std::format("Blinds {} / {}", sb, bb);
    dl->AddText(ImVec2{x, y}, token_u32(theme::ColorToken::TextSecondary), text.c_str());
    return line;
}

float draw_to_call(ImDrawList* dl, float x, float y, const engine::ScenarioState& scenario,
                   bool cash_mode, bool show_hud) {
    const float line = ImGui::GetTextLineHeightWithSpacing();
    // Caller scenarios only: faced_bet is the amount the user is calling (0 for the
    // Aggressor, who faces no bet). HUD-gated exactly like the pot total and the
    // floating call number, so toggling the HUD off hides this too and never leaves
    // the call amount sitting in the corner during the chip-counting drill.
    if (dl == nullptr || !show_hud || scenario.type != engine::ScenarioType::Caller) {
        return line;
    }
    const std::string text =
        std::format("To Call: {}", format_amount(scenario.faced_bet, cash_mode, scenario.big_blind));
    dl->AddText(ImVec2{x, y}, token_u32(theme::ColorToken::TextPrimary), text.c_str());
    return line;
}

void draw_floating_bet(ImDrawList* dl, float anchor_x, float anchor_y, int bet_dollars,
                       bool cash_mode, int big_blind, bool show_hud) {
    if (dl == nullptr || !show_hud) {
        return;
    }
    const std::string text = format_amount(bet_dollars, cash_mode, big_blind);
    const ImVec2 sz = ImGui::CalcTextSize(text.c_str());
    dl->AddText(ImVec2{anchor_x - sz.x * 0.5f, anchor_y}, token_u32(theme::ColorToken::TextPrimary),
                text.c_str());
}

}  // namespace poker_trainer::render
