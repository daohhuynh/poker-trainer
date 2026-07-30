#include "render/stat_modal.hpp"

#include "theme/theme_tokens.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <vector>

#include <imgui.h>

namespace poker_trainer::render {

namespace {

// Append the scenario-level rows -- the grades with no tier index (Equity if Called
// answered once, then the single Bet Size pick), in grading order. These make up the
// recap's "Overall" section for a multi-tier Aggressor and close out a flat recap.
void append_scenario_level_rows(const engine::GradingResult& result,
                                std::vector<RecapRow>& out) {
    for (const engine::InputGrade& g : result.inputs) {
        if (g.tier_index.has_value()) {
            continue;
        }
        out.push_back(RecapRow{g.input, g.tier_index, g.correct_value, g.margin,
                               g.submitted, g.correct});
    }
}

[[nodiscard]] std::string bet_tier_label(double ordinal) {
    switch (static_cast<int>(std::lround(ordinal))) {
        case 0: return "1/3 Pot";
        case 1: return "1/2 Pot";
        case 2: return "Full Pot";
        case 3: return "Overbet";
        default: return "-";
    }
}

// Column 2: the correct answer with its grading deviation, formatted per input.
[[nodiscard]] std::string format_correct(const RecapRow& row) {
    switch (row.input) {
        case engine::InputId::PotOdds:
        case engine::InputId::Equity:
        case engine::InputId::FoldProbability:
            return std::format("{:.0f}% +/- {:.0f}%", row.correct_value, row.margin);
        case engine::InputId::Outs:
            return std::format("{:.0f} (exact)", row.correct_value);
        case engine::InputId::Ev:
            return std::format("${:.0f} +/- ${:.2f}", row.correct_value, row.margin);
        case engine::InputId::BetSize:
            return bet_tier_label(row.correct_value);
    }
    return {};
}

// Column 3: the user's submitted answer, formatted per input ("-" when blank).
[[nodiscard]] std::string format_submitted(const RecapRow& row) {
    if (!row.submitted.has_value()) {
        return "-";
    }
    const double v = *row.submitted;
    switch (row.input) {
        case engine::InputId::PotOdds:
        case engine::InputId::Equity:
        case engine::InputId::FoldProbability:
            return std::format("{:.0f}%", v);
        case engine::InputId::Outs:
            return std::format("{:.0f}", v);
        case engine::InputId::Ev:
            return std::format("${:.0f}", v);
        case engine::InputId::BetSize:
            return bet_tier_label(v);
    }
    return {};
}

[[nodiscard]] int pct_of(int correct, int total) noexcept {
    if (total <= 0) {
        return 0;
    }
    return static_cast<int>(
        std::lround(100.0 * static_cast<double>(correct) / static_cast<double>(total)));
}

// ---- Render helpers (ImGui) ----

[[nodiscard]] ImU32 token_alpha_u32(theme::ColorToken token, float alpha) {
    ImVec4 c = theme::get_color(token);
    c.w *= alpha;
    return ImGui::ColorConvertFloat4ToU32(c);
}

void text_at(ImDrawList* dl, float x, float y, theme::ColorToken token, float alpha,
             const std::string& s) {
    dl->AddText(ImVec2{x, y}, token_alpha_u32(token, alpha), s.c_str());
}

}  // namespace

std::string_view input_display_name(engine::InputId input) noexcept {
    switch (input) {
        case engine::InputId::PotOdds: return "Pot Odds";
        case engine::InputId::Outs: return "Outs";
        case engine::InputId::Equity: return "Equity";
        case engine::InputId::Ev: return "EV";
        case engine::InputId::FoldProbability: return "Breakeven Fold %";
        case engine::InputId::BetSize: return "Bet Size";
    }
    return {};
}

std::array<std::string_view, 3> recap_column_headers() noexcept {
    return {std::string_view{}, std::string_view{"Correct Answer"},
            std::string_view{"Your Answer"}};
}

bool has_tier_tabs(const engine::ScenarioState& scenario) noexcept {
    return engine::is_aggressor(scenario.type) && scenario.multi_tier &&
           engine::aggressor_has_per_tier_inputs(scenario.type);
}

// The FULL input set for one tier tab, in on-screen order: Breakeven Fold % (this
// tier), Equity if Called (scenario-level, echoed), EV (this tier), Bet Size
// (scenario-level, echoed). Absent inputs for the sub-type are skipped. Each tier is
// self-contained so its Overall row scores exactly what the tier shows.
std::vector<RecapRow> build_tier_rows(const engine::GradingResult& result, std::uint8_t tier) {
    std::vector<RecapRow> rows;
    const auto push_first = [&](engine::InputId id, bool per_tier) {
        for (const engine::InputGrade& g : result.inputs) {
            const bool tier_ok = per_tier ? (g.tier_index.has_value() && *g.tier_index == tier)
                                          : !g.tier_index.has_value();
            if (g.input == id && tier_ok) {
                rows.push_back(RecapRow{g.input, g.tier_index, g.correct_value, g.margin,
                                        g.submitted, g.correct});
                return;
            }
        }
    };
    push_first(engine::InputId::FoldProbability, /*per_tier=*/true);  // Breakeven Fold %
    push_first(engine::InputId::Equity, /*per_tier=*/false);          // Equity if Called
    push_first(engine::InputId::Ev, /*per_tier=*/true);               // EV
    push_first(engine::InputId::BetSize, /*per_tier=*/false);         // Bet Size
    return rows;
}

std::vector<RecapRow> build_scenario_level_rows(const engine::GradingResult& result) {
    std::vector<RecapRow> rows;
    append_scenario_level_rows(result, rows);
    return rows;
}

std::vector<RecapRow> build_flat_rows(const engine::GradingResult& result) {
    std::vector<RecapRow> rows;
    rows.reserve(result.inputs.size());
    for (const engine::InputGrade& g : result.inputs) {
        rows.push_back(RecapRow{g.input, g.tier_index, g.correct_value, g.margin,
                                g.submitted, g.correct});
    }
    return rows;
}

int rows_accuracy_pct(std::span<const RecapRow> rows) noexcept {
    if (rows.empty()) {
        return 0;
    }
    int correct = 0;
    for (const RecapRow& r : rows) {
        if (r.correct) {
            ++correct;
        }
    }
    return pct_of(correct, static_cast<int>(rows.size()));
}

SummaryData build_summary(const engine::GradingResult& result) noexcept {
    SummaryData s{};
    s.total = static_cast<int>(result.inputs.size());
    for (const engine::InputGrade& g : result.inputs) {
        if (g.correct) {
            ++s.total_correct;
        }
        if (g.tier_index.has_value() && *g.tier_index < engine::kBetTierCount) {
            TierTally& tally = s.per_tier[*g.tier_index];
            ++tally.total;
            if (g.correct) {
                ++tally.correct;
            }
        }
    }
    return s;
}

int summary_pct(const SummaryData& summary) noexcept {
    return pct_of(summary.total_correct, summary.total);
}

int tier_accuracy_pct(const engine::GradingResult& result, std::uint8_t tier) {
    const std::vector<RecapRow> rows = build_tier_rows(result, tier);
    return rows_accuracy_pct(std::span<const RecapRow>{rows});
}

bool time_grade_overtime(const TimeGrade& grade) noexcept {
    return grade.actual_s > grade.target_s;
}

DealerExpression dealer_expression(bool pass) noexcept {
    return pass ? DealerExpression::Neutral : DealerExpression::Raised;
}

StripGeom tab_strip_geom(float modal_tl_x, float modal_tl_y, float modal_width,
                         float text_line_height) noexcept {
    const float pad = modal_width * 0.05f;
    StripGeom g{};
    g.x = modal_tl_x + pad;
    g.y = modal_tl_y + pad * 0.5f;
    g.h = text_line_height * 1.8f;
    g.tab_w = (modal_width - pad * 2.0f) / static_cast<float>(kRecapTabCount);
    return g;
}

int tab_index_at(const StripGeom& geom, float px, float py) noexcept {
    if (py < geom.y || py > geom.y + geom.h) {
        return -1;
    }
    const float rel = px - geom.x;
    if (rel < 0.0f || geom.tab_w <= 0.0f) {
        return -1;
    }
    const int idx = static_cast<int>(rel / geom.tab_w);
    if (idx < 0 || idx >= static_cast<int>(kRecapTabCount)) {
        return -1;
    }
    return idx;
}

// ===== Render ==================================================================

namespace {

// Draw the horizontal tier-tab strip (Tier 1..4, Summary). The active tab fills
// with accent_primary; the others read text_secondary. When `strip_focused`, a
// bounded 2px border_focus ring is drawn around the whole strip (single tab stop).
void draw_tab_strip(ImDrawList* dl, const StripGeom& g, RecapTab active, bool strip_focused,
                    float alpha) {
    constexpr std::array<const char*, kRecapTabCount> labels = {
        "Tier 1", "Tier 2", "Tier 3", "Tier 4", "Summary"};
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const float x = g.x + static_cast<float>(i) * g.tab_w;
        const ImVec2 cell_tl{x, g.y};
        const ImVec2 cell_br{x + g.tab_w, g.y + g.h};
        const bool is_active = (static_cast<std::size_t>(active) == i);
        if (is_active) {
            dl->AddRectFilled(cell_tl, cell_br,
                              token_alpha_u32(theme::ColorToken::AccentPrimary, alpha), 4.0f);
        }
        const ImVec2 ts = ImGui::CalcTextSize(labels[i]);
        const theme::ColorToken txt =
            is_active ? theme::ColorToken::TextButton : theme::ColorToken::TextSecondary;
        dl->AddText(ImVec2{x + (g.tab_w - ts.x) * 0.5f, g.y + (g.h - ts.y) * 0.5f},
                    token_alpha_u32(txt, alpha), labels[i]);
    }
    if (strip_focused) {
        const float strip_w = g.tab_w * static_cast<float>(kRecapTabCount);
        dl->AddRect(ImVec2{g.x, g.y}, ImVec2{g.x + strip_w, g.y + g.h},
                    token_alpha_u32(theme::ColorToken::BorderFocus, alpha), 4.0f, 0, 2.0f);
    }
}

// Font/row scale for the emphasized Overall row (A3): the dominant grade row reads
// larger / weightier than the per-input rows, the clear focal point.
constexpr float kOverallFontScale = 1.4f;
constexpr float kOverallRowScale = 1.35f;

// Draw the labeled header row (A2): [blank | "Correct Answer" | "Your Answer"] in
// text_secondary (subdued), with the same bottom separator as a data row. No
// vertical divider lines — columns are aligned, not boxed. Returns the next row's y.
float draw_header_row(ImDrawList* dl, const ImVec2& tl, float width, float y, float row_h,
                      const std::array<float, 3>& col_x, float alpha) {
    const float text_y = y + (row_h - ImGui::GetTextLineHeight()) * 0.5f;
    const std::array<std::string_view, 3> headers = recap_column_headers();
    for (std::size_t c = 1; c < headers.size(); ++c) {
        text_at(dl, col_x[c], text_y, theme::ColorToken::TextSecondary, alpha,
                std::string{headers[c]});
    }
    const float next_y = y + row_h;
    dl->AddLine(ImVec2{tl.x, next_y}, ImVec2{tl.x + width, next_y},
                token_alpha_u32(theme::ColorToken::SeparatorLine, alpha * 0.6f), 1.0f);
    return next_y;
}

// Draw one three-column row and its bottom separator. `label` heads column 0 (the
// input name, or a "Tier N" tag in the per-tier grid). Returns the next row's y.
float draw_recap_row(ImDrawList* dl, const ImVec2& tl, float width, float y, float row_h,
                     const std::array<float, 3>& col_x, std::string_view label,
                     const RecapRow& row, float alpha) {
    const float text_y = y + (row_h - ImGui::GetTextLineHeight()) * 0.5f;
    text_at(dl, col_x[0], text_y, theme::ColorToken::TextPrimary, alpha, std::string{label});
    text_at(dl, col_x[1], text_y, theme::ColorToken::TextSecondary, alpha, format_correct(row));
    const theme::ColorToken c3 =
        row.correct ? theme::ColorToken::StatePass : theme::ColorToken::StateFail;
    text_at(dl, col_x[2], text_y, c3, alpha, format_submitted(row));
    const float next_y = y + row_h;
    dl->AddLine(ImVec2{tl.x, next_y}, ImVec2{tl.x + width, next_y},
                token_alpha_u32(theme::ColorToken::SeparatorLine, alpha * 0.6f), 1.0f);
    return next_y;
}

// "Target Time: Xs | Actual Time: Ys (+Zs Overtime)" — Actual state_fail when
// overtime, state_pass when undertime. Real values are Zone 10's (SEAM); the
// caller feeds stub values until then.
float draw_time_grade_row(ImDrawList* dl, const ImVec2& tl, float width, float y, float row_h,
                          const std::array<float, 3>& col_x, const TimeGrade& tg, float alpha) {
    const float text_y = y + (row_h - ImGui::GetTextLineHeight()) * 0.5f;
    // Tutorial scenarios disable the timer: show the placeholder in place of the
    // Target / Actual values (text_secondary), no overtime/undertime coloring.
    if (tg.tutorial_disabled) {
        text_at(dl, col_x[0], text_y, theme::ColorToken::TextSecondary, alpha, "Time Grade:");
        text_at(dl, col_x[1], text_y, theme::ColorToken::TextSecondary, alpha,
                "Tutorial - timer disabled");
        const float disabled_next_y = y + row_h;
        dl->AddLine(ImVec2{tl.x, disabled_next_y}, ImVec2{tl.x + width, disabled_next_y},
                    token_alpha_u32(theme::ColorToken::SeparatorLine, alpha * 0.6f), 1.0f);
        return disabled_next_y;
    }
    text_at(dl, col_x[0], text_y, theme::ColorToken::TextSecondary, alpha,
            std::format("Target Time: {}s", tg.target_s));
    const bool over = time_grade_overtime(tg);
    const int delta = tg.actual_s - tg.target_s;
    const std::string actual =
        over ? std::format("Actual: {}s (+{}s Overtime)", tg.actual_s, delta)
             : std::format("Actual: {}s ({}s Undertime)", tg.actual_s, -delta);
    const theme::ColorToken c =
        over ? theme::ColorToken::StateFail : theme::ColorToken::StatePass;
    text_at(dl, col_x[1], text_y, c, alpha, actual);
    const float next_y = y + row_h;
    dl->AddLine(ImVec2{tl.x, next_y}, ImVec2{tl.x + width, next_y},
                token_alpha_u32(theme::ColorToken::SeparatorLine, alpha * 0.6f), 1.0f);
    return next_y;
}

// A "label ............ N%" row (no Correct/Your columns): the Summary tab's per-tier
// Overall lines. Column 0 holds the label, column 2 the tier's accuracy. Returns the
// next row's y.
float draw_pct_row(ImDrawList* dl, const ImVec2& tl, float width, float y, float row_h,
                   const std::array<float, 3>& col_x, std::string_view label, int pct,
                   float alpha) {
    const float text_y = y + (row_h - ImGui::GetTextLineHeight()) * 0.5f;
    text_at(dl, col_x[0], text_y, theme::ColorToken::TextPrimary, alpha, std::string{label});
    text_at(dl, col_x[2], text_y, theme::ColorToken::TextSecondary, alpha,
            std::format("{}%", pct));
    const float next_y = y + row_h;
    dl->AddLine(ImVec2{tl.x, next_y}, ImVec2{tl.x + width, next_y},
                token_alpha_u32(theme::ColorToken::SeparatorLine, alpha * 0.6f), 1.0f);
    return next_y;
}

// The dominant "Overall" grade row (A3): larger font, the accuracy percent in
// accent_primary (the focal point). Column 0 holds the "Overall" label, column 2 the
// percent. `row_h` is the (taller) overall row height the caller passes.
void draw_overall_row(ImDrawList* dl, float y, float row_h, const std::array<float, 3>& col_x,
                      int accuracy_pct, float alpha) {
    ImFont* font = ImGui::GetFont();
    const float font_size = ImGui::GetFontSize() * kOverallFontScale;
    const float text_y = y + (row_h - font_size) * 0.5f;
    dl->AddText(font, font_size, ImVec2{col_x[0], text_y},
                token_alpha_u32(theme::ColorToken::TextPrimary, alpha), "Overall");
    const std::string pct = std::format("{}%", accuracy_pct);
    dl->AddText(font, font_size, ImVec2{col_x[2], text_y},
                token_alpha_u32(theme::ColorToken::AccentPrimary, alpha), pct.c_str());
}

// The gap + heavy rule + dominant "Overall" grade row (A3) that closes a recap body.
// Shared by the tier tab, the Summary tab, and the flat recap so they never drift.
// Returns the content's ending y (for scroll-height measurement).
float draw_bottom_overall_row(ImDrawList* dl, const ImVec2& tl, float width, float pad, float y,
                              float row_h, const std::array<float, 3>& col_x, int accuracy_pct,
                              float alpha) {
    y += row_h * 0.5f;
    dl->AddLine(ImVec2{tl.x + pad, y}, ImVec2{tl.x + width - pad, y},
                token_alpha_u32(theme::ColorToken::SeparatorLine, alpha), 2.0f);
    y += row_h * 0.25f;
    const float overall_h = row_h * kOverallRowScale;
    draw_overall_row(dl, y, overall_h, col_x, accuracy_pct, alpha);
    return y + overall_h;
}

// One tier tab's body: the labeled header, the tier's FULL input set (Breakeven Fold
// %, Equity if Called, EV, Bet Size as applicable), the Time-Grade, then the tier's
// OWN Overall accuracy (scoped to what this tier shows). Returns the ending y.
float draw_tier_body(ImDrawList* dl, const ImVec2& tl, float width, float pad, float y,
                     float row_h, const std::array<float, 3>& col_x,
                     const engine::GradingResult& result, std::uint8_t tier, const TimeGrade& tg,
                     float alpha) {
    const std::vector<RecapRow> rows = build_tier_rows(result, tier);
    y = draw_header_row(dl, tl, width, y, row_h, col_x, alpha);
    for (const RecapRow& row : rows) {
        y = draw_recap_row(dl, tl, width, y, row_h, col_x, input_display_name(row.input), row, alpha);
    }
    y = draw_time_grade_row(dl, tl, width, y, row_h, col_x, tg, alpha);
    return draw_bottom_overall_row(dl, tl, width, pad, y, row_h, col_x,
                                   tier_accuracy_pct(result, tier), alpha);
}

// The Summary tab: one "Tier N — %" row per bet tier (that tier's own Overall), the
// Time-Grade, then the whole-round Overall (unique-input accuracy). Returns ending y.
float draw_summary_body(ImDrawList* dl, const ImVec2& tl, float width, float pad, float y,
                        float row_h, const std::array<float, 3>& col_x,
                        const engine::GradingResult& result, const TimeGrade& tg, float alpha) {
    for (std::uint8_t t = 0; t < engine::kBetTierCount; ++t) {
        y = draw_pct_row(dl, tl, width, y, row_h, col_x, std::format("Tier {}", t + 1),
                         tier_accuracy_pct(result, t), alpha);
    }
    y = draw_time_grade_row(dl, tl, width, y, row_h, col_x, tg, alpha);
    return draw_bottom_overall_row(dl, tl, width, pad, y, row_h, col_x,
                                   summary_pct(build_summary(result)), alpha);
}

// A flat (non-tabbed) recap body: Caller or single-tier Aggressor. Every graded input
// in one list, the Time-Grade, then the Overall accuracy. Returns the ending y.
float draw_flat_body(ImDrawList* dl, const ImVec2& tl, float width, float pad, float y,
                     float row_h, const std::array<float, 3>& col_x,
                     const engine::GradingResult& result, const TimeGrade& tg, float alpha) {
    const std::vector<RecapRow> rows = build_flat_rows(result);
    y = draw_header_row(dl, tl, width, y, row_h, col_x, alpha);
    for (const RecapRow& row : rows) {
        y = draw_recap_row(dl, tl, width, y, row_h, col_x, input_display_name(row.input), row, alpha);
    }
    y = draw_time_grade_row(dl, tl, width, y, row_h, col_x, tg, alpha);
    return draw_bottom_overall_row(dl, tl, width, pad, y, row_h, col_x,
                                   rows_accuracy_pct(std::span<const RecapRow>{rows}), alpha);
}

}  // namespace

float render_stat_modal(ImDrawList* dl, const engine::ScenarioState& scenario,
                        const engine::GradingResult& result, RecapTab active_tab,
                        const TimeGrade& time_grade, const StatModalRender& params) {
    const ImVec2 tl = *params.top_left;
    const ImVec2 br = *params.bottom_right;
    const float alpha = params.alpha;
    const float width = br.x - tl.x;

    // Opaque panel, matching every other modal in the app. ARCHITECTURE specifies
    // bg_modal_translucent (65%) here, but over the photographic room background the
    // recap rows were unreadable through it, so the user overrode the spec in favour of
    // the solid bg_modal fill -- including where it covers part of the dealer, which is
    // intended. Only the arrival-fade alpha modulates it now.
    dl->AddRectFilled(tl, br, token_alpha_u32(theme::ColorToken::BgModalSurface, alpha), 8.0f);

    const float pad = width * 0.05f;
    const bool tabbed = has_tier_tabs(scenario);
    float body_top = tl.y + pad;
    if (tabbed) {
        // The tier-tab strip is PINNED: drawn before (and outside) the body clip so it
        // stays fixed while the body scrolls beneath it.
        const StripGeom g = tab_strip_geom(tl.x, tl.y, width, ImGui::GetTextLineHeight());
        draw_tab_strip(dl, g, active_tab, params.strip_focused, alpha);
        body_top = g.y + g.h + pad * 0.5f;
    }

    const std::array<float, 3> col_x = {tl.x + pad, tl.x + width * 0.40f, tl.x + width * 0.74f};
    const float row_h = ImGui::GetTextLineHeight() * 1.8f;
    const float body_bottom = br.y - pad * 0.5f;
    const float avail = body_bottom - body_top;

    // Clip the scrollable body to the modal so nothing draws past the panel; offset it
    // by scroll_offset. The tier strip above stays pinned (drawn outside this clip).
    dl->PushClipRect(ImVec2{tl.x, body_top}, ImVec2{br.x, body_bottom}, true);
    const float y_start = body_top - params.scroll_offset;
    float y_end = y_start;
    if (tabbed && active_tab == RecapTab::Summary) {
        y_end = draw_summary_body(dl, tl, width, pad, y_start, row_h, col_x, result, time_grade,
                                  alpha);
    } else if (tabbed) {
        y_end = draw_tier_body(dl, tl, width, pad, y_start, row_h, col_x, result,
                               static_cast<std::uint8_t>(active_tab), time_grade, alpha);
    } else {
        y_end = draw_flat_body(dl, tl, width, pad, y_start, row_h, col_x, result, time_grade, alpha);
    }
    dl->PopClipRect();

    // Overflow = content height beyond the visible body region (clamped >= 0). The
    // caller clamps scrolling to [0, overflow] and enables the mouse wheel over it.
    const float content_px = y_end - y_start;
    return std::max(0.0f, content_px - avail);
}

}  // namespace poker_trainer::render
