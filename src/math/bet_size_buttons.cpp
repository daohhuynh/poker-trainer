#include "math/bet_size_buttons.hpp"

#include "math/interrogator.hpp"

#include "backbone/focus_manager.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

#include <imgui.h>

#include "theme/theme_tokens.hpp"

namespace poker_trainer::interrogator {

bool bet_group_present(const engine::ScenarioState& s) noexcept {
    return engine::is_aggressor(s.type);
}

std::optional<engine::BetTier> bet_tier_for_digit(int digit) noexcept {
    if (digit < 1 || digit > static_cast<int>(engine::kBetTierCount)) {
        return std::nullopt;
    }
    return static_cast<engine::BetTier>(static_cast<std::uint8_t>(digit - 1));
}

bool select_bet_tier_by_digit(BetSizeGroup& group, int digit) noexcept {
    const std::optional<engine::BetTier> tier = bet_tier_for_digit(digit);
    if (!tier.has_value()) {
        return false;
    }
    group.selected = tier;
    return true;
}

void select_bet_tier_on_click(BetSizeGroup& group, engine::BetTier tier) noexcept {
    group.selected = tier;
    backbone::activate_keyboard_mode();
    backbone::snap_focus_to(group.focus_id);
}

// ===== Render (Module 5) — not unit-tested (CLAUDE.md sec.9) =====

namespace {

[[nodiscard]] bool focus_on(backbone::FocusableId id) {
    return backbone::is_keyboard_mode_active() && backbone::get_focused_element() == id;
}

[[nodiscard]] ImU32 token_u32(theme::ColorToken token) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(token));
}

constexpr std::array<const char*, engine::kBetTierCount> kTierLabels = {
    "1/3 Pot", "1/2 Pot", "Full Pot", "Overbet"};

}  // namespace

void render_bet_size_group(BetSizeGroup& group) {
    if (!group.present) {
        return;
    }
    ImGui::TextColored(theme::get_color(theme::ColorToken::TextSecondary), "Bet Size");

    const ImVec2 row_min = ImGui::GetCursorScreenPos();
    for (std::size_t i = 0; i < engine::kBetTierCount; ++i) {
        if (i != 0) {
            ImGui::SameLine();
        }
        const auto tier = static_cast<engine::BetTier>(static_cast<std::uint8_t>(i));
        const bool selected = group.selected.has_value() && *group.selected == tier;
        // Selected button fills with accent_primary across all three button states
        // (so the highlight persists on hover, not just at rest) and overrides the
        // label to bg_primary -- a dark, high-contrast token in every theme. The
        // default text_button (a near-white cream) is illegible on the bright
        // amber-gold / sage / ocean accents; ARCHITECTURE specifies the accent fill
        // for the selected button but is silent on its label color, so the
        // contrasting text token is chosen here (no literal colors). A click
        // selects the tier with the same feedback as keys 1-4 and moves the focus
        // outline onto the group.
        int pushed = 1;
        if (selected) {
            const ImVec4 accent = theme::get_color(theme::ColorToken::AccentPrimary);
            ImGui::PushStyleColor(ImGuiCol_Button, accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, accent);
            ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::BgPrimary));
            pushed = 4;
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, theme::get_color(theme::ColorToken::ButtonBg));
        }
        if (ImGui::Button(kTierLabels[i])) {
            select_bet_tier_on_click(group, tier);
        }
        ImGui::PopStyleColor(pushed);
    }
    const ImVec2 row_max = ImGui::GetItemRectMax();

    // Focus indicator: a single 2px border_focus outline around the whole row's
    // bounding box, not around any individual button (Notes — Keyboard Focus).
    if (focus_on(group.focus_id)) {
        ImGui::GetWindowDrawList()->AddRect(row_min, ImVec2{row_max.x, row_max.y},
                                            token_u32(theme::ColorToken::BorderFocus), 0.0f, 0,
                                            2.0f);
    }
}

}  // namespace poker_trainer::interrogator
