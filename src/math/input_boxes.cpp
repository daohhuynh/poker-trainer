#include "math/input_boxes.hpp"

#include "math/bet_size_buttons.hpp"
#include "math/interrogator.hpp"

#include "backbone/focus_manager.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <system_error>

#include <imgui.h>

#include "theme/theme_tokens.hpp"

namespace poker_trainer::interrogator {

namespace {

// Per-input keystroke-filter policy. EV is the only signed input; Outs is the
// only integer (no decimal point). Everything else is a non-negative decimal.
struct NumericPolicy {
    bool allow_decimal;
    bool allow_minus;
};

[[nodiscard]] NumericPolicy policy_for(engine::InputId id) noexcept {
    switch (id) {
        case engine::InputId::Outs:
            return {false, false};
        case engine::InputId::Ev:
            return {true, true};
        case engine::InputId::PotOdds:
        case engine::InputId::Equity:
        case engine::InputId::FoldProbability:
        case engine::InputId::BetSize:
            break;
    }
    return {true, false};
}

[[nodiscard]] backbone::FocusableId focus_id_for(engine::InputId id,
                                                 std::optional<std::uint8_t> tier) noexcept {
    switch (id) {
        case engine::InputId::PotOdds:
            return kFocusPotOdds;
        case engine::InputId::Outs:
            return kFocusOuts;
        case engine::InputId::Equity:
            return kFocusEquity;
        case engine::InputId::Ev:
            return tier.has_value() ? kFocusEvTier[*tier] : kFocusEvCaller;
        case engine::InputId::FoldProbability:
            return tier.has_value() ? kFocusFoldTier[*tier] : kFocusFoldTier[0];
        case engine::InputId::BetSize:
            return kFocusBetSizeGroup;
    }
    return kFocusPotOdds;
}

[[nodiscard]] NumericBox make_box(engine::InputId id, std::optional<std::uint8_t> tier) noexcept {
    const NumericPolicy policy = policy_for(id);
    NumericBox box{};
    box.input = id;
    box.tier = tier;
    box.focus_id = focus_id_for(id, tier);
    box.allow_decimal = policy.allow_decimal;
    box.allow_minus = policy.allow_minus;
    return box;
}

// True when this box's typed text holds no parseable value yet.
[[nodiscard]] bool buffer_empty(const NumericBox& box) noexcept {
    return box.text[0] == '\0';
}

[[nodiscard]] bool focus_on(backbone::FocusableId id) {
    return backbone::is_keyboard_mode_active() && backbone::get_focused_element() == id;
}

[[nodiscard]] ImU32 token_u32(theme::ColorToken token) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(token));
}

// ImGui keystroke filter: keep only characters the numeric policy permits.
int numeric_char_filter(ImGuiInputTextCallbackData* data) {
    const auto* box = static_cast<const NumericBox*>(data->UserData);
    const std::string_view current{data->Buf, static_cast<std::size_t>(data->BufTextLen)};
    if (data->EventChar < 256 &&
        accepts_numeric_char(current, static_cast<char>(data->EventChar), box->allow_decimal,
                             box->allow_minus)) {
        return 0;  // keep
    }
    return 1;  // discard
}

}  // namespace

bool accepts_numeric_char(std::string_view current, char ch, bool allow_decimal,
                          bool allow_minus) noexcept {
    if (ch >= '0' && ch <= '9') {
        return true;
    }
    if (ch == '.') {
        return allow_decimal && current.find('.') == std::string_view::npos;
    }
    if (ch == '-') {
        return allow_minus && current.empty();
    }
    return false;
}

std::vector<NumericBox> build_boxes(const engine::ScenarioState& s) {
    std::vector<NumericBox> boxes;
    if (s.type == engine::ScenarioType::Caller) {
        boxes.push_back(make_box(engine::InputId::PotOdds, std::nullopt));
        boxes.push_back(make_box(engine::InputId::Outs, std::nullopt));
        boxes.push_back(make_box(engine::InputId::Equity, std::nullopt));
        boxes.push_back(make_box(engine::InputId::Ev, std::nullopt));
        return boxes;
    }

    // Aggressor. Bet-size-dependent Fold% / EV spawn per presented tier; the
    // bet-size-independent Equity-if-Called (Semi-Bluff) spawns exactly once.
    const bool semi = (s.type == engine::ScenarioType::AggressorSemiBluff);
    if (!s.multi_tier) {
        const auto t = static_cast<std::uint8_t>(s.presented_tier);
        boxes.push_back(make_box(engine::InputId::FoldProbability, t));
        if (semi) {
            // Single-tier order matches the spec branch list: Fold, Equity, EV.
            boxes.push_back(make_box(engine::InputId::Equity, std::nullopt));
        }
        boxes.push_back(make_box(engine::InputId::Ev, t));
        return boxes;
    }

    for (std::uint8_t t = 0; t < engine::kBetTierCount; ++t) {
        boxes.push_back(make_box(engine::InputId::FoldProbability, t));
        boxes.push_back(make_box(engine::InputId::Ev, t));
    }
    if (semi) {
        // Multi-tier: the single Equity-if-Called box follows the per-tier inputs.
        boxes.push_back(make_box(engine::InputId::Equity, std::nullopt));
    }
    return boxes;
}

std::vector<backbone::FocusableId> build_focus_segment(const InterrogatorState& state) {
    std::vector<backbone::FocusableId> segment;
    segment.reserve(state.boxes.size() + 1);
    for (const NumericBox& box : state.boxes) {
        segment.push_back(box.focus_id);
    }
    if (state.bet_group.present) {
        segment.push_back(state.bet_group.focus_id);
    }
    return segment;
}

void configure_for_scenario(InterrogatorState& state, const engine::ScenarioState& s) {
    state.scenario = s;
    state.boxes = build_boxes(s);
    state.bet_group = BetSizeGroup{};
    state.bet_group.present = bet_group_present(s);
    state.last_result.reset();
    state.last_math_pass = false;
    state.focus_segment = build_focus_segment(state);
}

std::optional<double> parse_box_double(const NumericBox& box) noexcept {
    if (buffer_empty(box)) {
        return std::nullopt;
    }
    const char* begin = box.text.data();
    char* end = nullptr;
    const double value = std::strtod(begin, &end);
    if (end == begin || *end != '\0') {
        return std::nullopt;  // nothing parsed ("-", ".", "-.") or trailing junk
    }
    return value;
}

std::optional<int> parse_box_int(const NumericBox& box) noexcept {
    if (buffer_empty(box)) {
        return std::nullopt;
    }
    const std::string_view sv{box.text.data()};
    int value = 0;
    const auto* const stop = sv.data() + sv.size();
    const auto [ptr, ec] = std::from_chars(sv.data(), stop, value);
    if (ec != std::errc{} || ptr != stop) {
        return std::nullopt;
    }
    return value;
}

// ===== Render (Module 5 layout) — not unit-tested (CLAUDE.md sec.9) =====

namespace {

// Draw one labeled numeric box. The box fill uses bg_input, its border
// border_default, the text text_input; when keyboard-focused, a 2px border_focus
// outline overlays the standard border (Visual State — Numeric Boxes).
void draw_numeric_box(NumericBox& box, const char* label, float box_width) {
    ImGui::PushID(static_cast<int>(box.focus_id.value & 0x7fffffffULL));
    ImGui::TextColored(theme::get_color(theme::ColorToken::TextSecondary), "%s", label);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::get_color(theme::ColorToken::InputBg));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::InputText));
    ImGui::PushStyleColor(ImGuiCol_Border, theme::get_color(theme::ColorToken::BorderDefault));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::SetNextItemWidth(box_width);
    // SEAM(Z05): keyboard char events are not yet fed into ImGui IO (platform.cpp
    // dispatches keys only to the event router), so live typing lands once that
    // feed + a router keyboard gate (mirroring router_should_see_mouse) are wired.
    // The buffer is Z09-owned, so grading reads the same array regardless.
    ImGui::InputText("##box", box.text.data(), box.text.size(),
                     ImGuiInputTextFlags_CallbackCharFilter, &numeric_char_filter, &box);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    if (focus_on(box.focus_id)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                    token_u32(theme::ColorToken::BorderFocus), 0.0f, 0, 2.0f);
    }
    ImGui::PopID();
}

[[nodiscard]] const char* label_for(const NumericBox& box) noexcept {
    switch (box.input) {
        case engine::InputId::PotOdds:
            return "Pot Odds";
        case engine::InputId::Outs:
            return "Outs";
        case engine::InputId::Equity:
            return "Equity";
        case engine::InputId::Ev:
            return "EV";
        case engine::InputId::FoldProbability:
            return "Fold Probability";
        case engine::InputId::BetSize:
            return "Bet Size";
    }
    return "";
}

}  // namespace

void render_math_inputs(InterrogatorRuntime& runtime, const engine::ScenarioState& scenario) {
    InterrogatorState& state = runtime.state;
    if (!state.scenario.has_value() || state.scenario->id != scenario.id) {
        configure_for_scenario(state, scenario);
    }

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float box_width = vp->Size.x * 0.10f;
    // Left-middle, vertically centered (ImGui centers the window's content from
    // this anchor as boxes stack up/down).
    ImGui::SetNextWindowPos(ImVec2{vp->Pos.x + vp->Size.x * 0.04f, vp->Pos.y + vp->Size.y * 0.5f},
                            ImGuiCond_Always, ImVec2{0.0f, 0.5f});
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground |
                                   ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin("##math_inputs", nullptr, flags)) {
        for (NumericBox& box : state.boxes) {
            draw_numeric_box(box, label_for(box), box_width);
        }
        if (state.bet_group.present) {
            render_bet_size_group(state.bet_group);
        }
    }
    ImGui::End();
}

}  // namespace poker_trainer::interrogator
