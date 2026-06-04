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
#include <imgui_internal.h>  // ImGui::ClearActiveID -- release an active InputText

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

FocusReconcile reconcile_imgui_focus(const InterrogatorState& state,
                                     backbone::FocusableId prev,
                                     backbone::FocusableId current) noexcept {
    // Bet-size group (a non-text stop) holds focus -> yield ImGui keyboard capture.
    // Decided EVERY frame the group is focused, NOT only on the change frame, so it
    // sits ABOVE the unchanged-focus early-out below. A pending SetKeyboardFocusHere
    // from the box the user just navigated away from is applied by ImGui *after* a
    // one-shot ClearActiveID on the change frame -- that re-activates the box and
    // leaves it the active InputText permanently, so digits 1-4 then type into the
    // box instead of selecting a tier. Re-yielding each frame releases the
    // re-activated box on the next frame and keeps WantCaptureKeyboard false. The
    // render glue gates the actual ClearActiveID on io.WantTextInput, so a bet
    // button mid-click (ActiveId set but WantTextInput NOT set) is never cleared.
    if (state.bet_group.present && current == state.bet_group.focus_id) {
        return {ImGuiFocusAction::YieldKeyboard, backbone::kNoFocus};
    }
    // Everything else acts only on a change: re-applying text focus every frame
    // would trap the caret (SetKeyboardFocusHere) or fight the user's edits.
    if (current == prev) {
        return {ImGuiFocusAction::None, backbone::kNoFocus};
    }
    // Focus landed on a numeric box -> give ImGui text focus to that box so typing
    // follows the outline (Tab / 1-6 navigation, no click required).
    for (const NumericBox& box : state.boxes) {
        if (box.focus_id == current) {
            return {ImGuiFocusAction::FocusTextBox, current};
        }
    }
    // Focus moved to an element Z09 does not own (a Z08/Z11 cluster stop) -- leave
    // ImGui's keyboard state untouched. SEAM(Z08/Z11).
    return {ImGuiFocusAction::None, backbone::kNoFocus};
}

void focus_box_on_click(const NumericBox& box) noexcept {
    backbone::activate_keyboard_mode();
    backbone::snap_focus_to(box.focus_id);
}

// ===== Render (Module 5 layout) — not unit-tested (CLAUDE.md sec.9) =====

namespace {

// Draw one labeled numeric box. The box fill uses bg_input, its border
// border_default, the text text_input; when keyboard-focused, a 2px border_focus
// outline overlays the standard border (Visual State — Numeric Boxes). When
// `grab_focus` is set (focus_manager moved the outline onto this box via Tab /
// 1-6 this frame), ImGui's keyboard focus is steered here so typing follows the
// outline without a click.
void draw_numeric_box(NumericBox& box, const char* label, float box_width, bool grab_focus) {
    ImGui::PushID(static_cast<int>(box.focus_id.value & 0x7fffffffULL));
    ImGui::TextColored(theme::get_color(theme::ColorToken::TextSecondary), "%s", label);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::get_color(theme::ColorToken::InputBg));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::InputText));
    ImGui::PushStyleColor(ImGuiCol_Border, theme::get_color(theme::ColorToken::BorderDefault));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::SetNextItemWidth(box_width);
    if (grab_focus) {
        // Steer ImGui's keyboard focus to the box the outline just landed on, so
        // the typing target follows keyboard navigation (called only on the focus-
        // change frame -- see reconcile_imgui_focus -- never every frame).
        ImGui::SetKeyboardFocusHere();
    }
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

    // Reconcile ImGui's keyboard focus to focus_manager (the single focused
    // element) on the frame focus changes. Nav (Tab / 1-6) moves focus_manager
    // between frames; this couples ImGui to it -- a box that just gained focus
    // grabs ImGui text focus (SetKeyboardFocusHere, in draw_numeric_box); the bet
    // group yields ImGui keyboard capture so digits 1-4 reach its select handler.
    const backbone::FocusableId focus_now =
        backbone::is_keyboard_mode_active() ? backbone::get_focused_element()
                                            : backbone::kNoFocus;
    const FocusReconcile rec =
        reconcile_imgui_focus(state, state.last_synced_focus, focus_now);
    // YieldKeyboard fires every frame the bet group holds focus (see
    // reconcile_imgui_focus). Only release ImGui's active item when a TEXT field is
    // actually active (io.WantTextInput) -- a numeric box that the pending
    // SetKeyboardFocusHere re-activated after the focus moved to the group. Gating
    // on WantTextInput means a bet button being clicked (it sets ActiveId but never
    // WantTextInput) is left alone, so its click still registers; without the gate,
    // clearing every frame would eat the button press mid-click.
    if (rec.action == ImGuiFocusAction::YieldKeyboard && ImGui::GetIO().WantTextInput) {
        ImGui::ClearActiveID();  // release the lingering active box (focus -> bet group)
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
            const bool grab = rec.action == ImGuiFocusAction::FocusTextBox &&
                              box.focus_id == rec.target;
            draw_numeric_box(box, label_for(box), box_width, grab);
        }
        if (state.bet_group.present) {
            render_bet_size_group(state.bet_group);
        }
    }
    ImGui::End();

    // Record the element ImGui is now reconciled to (after any click this frame
    // moved both the outline and ImGui's text focus together), so the next frame
    // acts only on a fresh change -- never re-grabbing focus that is already in
    // sync, which would trap the caret.
    state.last_synced_focus = backbone::is_keyboard_mode_active()
                                  ? backbone::get_focused_element()
                                  : backbone::kNoFocus;
}

}  // namespace poker_trainer::interrogator
