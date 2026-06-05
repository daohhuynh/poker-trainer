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

#include "bridge/focus_registry.hpp"
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
    // New focus segment: forget the prior frame's reconciliation target so the
    // render path re-syncs ImGui to whatever focus_manager now reports.
    state.last_synced_focus = backbone::kNoFocus;
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

void focus_box_on_click(const NumericBox& box) noexcept {
    backbone::activate_keyboard_mode();
    backbone::snap_focus_to(box.focus_id);
}

// ===== Render (Module 5 layout) — not unit-tested (CLAUDE.md sec.9) =====

namespace {

// Draw one labeled numeric box. The box fill uses bg_input, its border
// border_default, the text text_input; when keyboard-focused, the shared substrate
// overlays a 2px border_focus ring (Visual State — Numeric Boxes). The substrate
// also steers ImGui's keyboard focus here on the frame `reconcile` targets this box
// (the outline moved onto it via Tab / 1-6), so typing follows the outline without
// a click; `ring_color` is the border_focus token resolved by the caller.
void draw_numeric_box(NumericBox& box, const char* label, float box_width,
                      const bridge::FocusReconcile& reconcile, std::uint32_t ring_color) {
    ImGui::PushID(static_cast<int>(box.focus_id.value & 0x7fffffffULL));
    ImGui::TextColored(theme::get_color(theme::ColorToken::TextSecondary), "%s", label);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::get_color(theme::ColorToken::InputBg));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::InputText));
    ImGui::PushStyleColor(ImGuiCol_Border, theme::get_color(theme::ColorToken::BorderDefault));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::SetNextItemWidth(box_width);
    bridge::grab_keyboard_if_target(reconcile, box.focus_id);
    // Live typing flows in via the platform keyboard path: DOM key events are fed
    // to ImGui IO (platform.cpp::feed_imgui_keyboard) and the event router is gated
    // on WantCaptureKeyboard (bridge/input_routing.hpp), so an active box receives
    // digits / '.' / '-' here while the global number-key focus binding is
    // suppressed. The buffer is Z09-owned, so grading reads the same array.
    ImGui::InputText("##box", box.text.data(), box.text.size(),
                     ImGuiInputTextFlags_CallbackCharFilter, &numeric_char_filter, &box);
    // A mouse click takes ImGui text focus for free; mirror it into focus_manager
    // so the outline jumps to the clicked box (the single focused element).
    if (ImGui::IsItemClicked()) {
        focus_box_on_click(box);
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    bridge::draw_focus_ring(box.focus_id, ring_color);
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

    // Reconcile ImGui's keyboard focus to focus_manager (the single focused
    // element) on the frame focus changes, via the shared substrate. Nav (Tab /
    // 1-6) moves focus_manager between frames; the substrate couples ImGui to it
    // from the registry's is_text_field -- a box that just gained focus grabs ImGui
    // text focus (grab_keyboard_if_target, in draw_numeric_box); the bet group (a
    // non-text stop) yields ImGui keyboard capture so digits 1-4 reach its select
    // handler. begin_focus_reconcile applies the once-per-frame ClearActiveID.
    // The registry is null only in tests that never render; an unwired registry
    // degrades to no reconcile (the rings below still draw from focus_manager).
    const bridge::FocusReconcile rec =
        runtime.focus_registry != nullptr
            ? bridge::begin_focus_reconcile(*runtime.focus_registry, state.last_synced_focus)
            : bridge::FocusReconcile{};
    const std::uint32_t ring_color =
        ImGui::ColorConvertFloat4ToU32(theme::get_color(theme::ColorToken::BorderFocus));

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
            draw_numeric_box(box, label_for(box), box_width, rec, ring_color);
        }
        if (state.bet_group.present) {
            render_bet_size_group(state.bet_group, ring_color);
        }
    }
    ImGui::End();

    // Record the element ImGui is now reconciled to (after any click this frame
    // moved both the outline and ImGui's text focus together), so the next frame
    // acts only on a fresh change -- never re-grabbing focus that is already in
    // sync, which would trap the caret.
    state.last_synced_focus = bridge::active_focus_or_none();
}

}  // namespace poker_trainer::interrogator
