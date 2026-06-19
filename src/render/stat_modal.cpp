#include "render/stat_modal.hpp"

#include "render/front_dealer.hpp"

#include "assets/asset_paths.hpp"
#include "theme/theme_tokens.hpp"

#include <cmath>
#include <cstddef>
#include <format>
#include <string>

#include <imgui.h>

namespace poker_trainer::render {

namespace {

// Append the rows that are echoed on every tier tab (and that close out a flat
// recap): the bet-size-independent inputs answered once (Equity if Called, Outs)
// and the single Bet Size pick. In the grading result these are the inputs with no
// tier index, in grading order (Equity then Bet Size for an Aggressor).
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
            return std::format("{:.0f}% ± {:.0f}%", row.correct_value, row.margin);
        case engine::InputId::Outs:
            return std::format("{:.0f} (exact)", row.correct_value);
        case engine::InputId::Ev:
            return std::format("${:.0f} ± ${:.2f}", row.correct_value, row.margin);
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
        case engine::InputId::FoldProbability: return "Fold Probability";
        case engine::InputId::BetSize: return "Bet Size";
    }
    return {};
}

std::array<std::string_view, 3> recap_column_headers() noexcept {
    return {std::string_view{}, std::string_view{"Correct Answer"},
            std::string_view{"Your Answer"}};
}

bool has_tier_tabs(const engine::ScenarioState& scenario) noexcept {
    return engine::is_aggressor(scenario.type) && scenario.multi_tier;
}

std::vector<RecapRow> build_tier_rows(const engine::GradingResult& result, std::uint8_t tier) {
    std::vector<RecapRow> rows;
    for (const engine::InputGrade& g : result.inputs) {
        if (g.tier_index.has_value() && *g.tier_index == tier) {
            rows.push_back(RecapRow{g.input, g.tier_index, g.correct_value, g.margin,
                                    g.submitted, g.correct});
        }
    }
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

// Draw one three-column row and its bottom separator. Returns the next row's y.
float draw_recap_row(ImDrawList* dl, const ImVec2& tl, float width, float y, float row_h,
                     const std::array<float, 3>& col_x, const RecapRow& row, float alpha) {
    const float text_y = y + (row_h - ImGui::GetTextLineHeight()) * 0.5f;
    text_at(dl, col_x[0], text_y, theme::ColorToken::TextPrimary, alpha,
            std::string{input_display_name(row.input)});
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

// The bottom "Overall" row, rendered as the dominant grade row (A3): larger font,
// the accuracy percent in accent_primary (the focal point). Column 0 holds the
// "Overall" label (+ side-pot icon when applicable), column 2 the percent across
// this tab's rows. `row_h` is the (taller) overall row height the caller passes.
void draw_overall_row(ImDrawList* dl, const ImVec2& tl, float y, float row_h,
                      const std::array<float, 3>& col_x, bool side_pot, int accuracy_pct,
                      float alpha) {
    ImFont* font = ImGui::GetFont();
    const float font_size = ImGui::GetFontSize() * kOverallFontScale;
    const float text_y = y + (row_h - font_size) * 0.5f;
    float label_x = col_x[0];
    if (side_pot) {
        const float icon = font_size;
        draw_image_alpha(dl, ImVec2{label_x, text_y}, ImVec2{label_x + icon, text_y + icon},
                         assets::AssetId::IconSidePotChip, alpha);
        label_x += icon + 6.0f;
    }
    dl->AddText(font, font_size, ImVec2{label_x, text_y},
                token_alpha_u32(theme::ColorToken::TextPrimary, alpha), "Overall");
    const std::string pct = std::format("{}%", accuracy_pct);
    dl->AddText(font, font_size, ImVec2{col_x[2], text_y},
                token_alpha_u32(theme::ColorToken::AccentPrimary, alpha), pct.c_str());
    (void)tl;
}

// The Summary tab: scenario-level overview (no three-column structure). Overall
// fraction + percent, a per-tier breakdown row, the scenario Time-Grade, and the
// side-pot icon when applicable.
void draw_summary_body(ImDrawList* dl, const ImVec2& body_tl, float width,
                       const engine::GradingResult& result, const engine::ScenarioState& scenario,
                       const TimeGrade& tg, float alpha) {
    const SummaryData s = build_summary(result);
    const float line = ImGui::GetTextLineHeight() * 1.8f;
    float y = body_tl.y;
    const float x = body_tl.x;

    float label_x = x;
    if (scenario.side_pot) {
        const float icon = ImGui::GetTextLineHeight();
        draw_image_alpha(dl, ImVec2{label_x, y}, ImVec2{label_x + icon, y + icon},
                         assets::AssetId::IconSidePotChip, alpha);
        label_x += icon + 6.0f;
    }
    text_at(dl, label_x, y, theme::ColorToken::TextPrimary, alpha,
            std::format("{}/{} correct ({}%)", s.total_correct, s.total, summary_pct(s)));
    y += line;

    std::string breakdown = "Per tier:";
    for (std::size_t t = 0; t < engine::kBetTierCount; ++t) {
        breakdown += std::format("  T{}: {}/{}", t + 1, s.per_tier[t].correct, s.per_tier[t].total);
    }
    text_at(dl, x, y, theme::ColorToken::TextSecondary, alpha, breakdown);
    y += line;

    const bool over = time_grade_overtime(tg);
    text_at(dl, x, y, theme::ColorToken::TextSecondary, alpha,
            std::format("Target Time: {}s", tg.target_s));
    text_at(dl, x + width * 0.45f, y,
            over ? theme::ColorToken::StateFail : theme::ColorToken::StatePass, alpha,
            over ? std::format("Actual: {}s (+{}s Overtime)", tg.actual_s, tg.actual_s - tg.target_s)
                 : std::format("Actual: {}s ({}s Undertime)", tg.actual_s, tg.target_s - tg.actual_s));
}

}  // namespace

void render_stat_modal(ImDrawList* dl, const engine::ScenarioState& scenario,
                       const engine::GradingResult& result, RecapTab active_tab,
                       const TimeGrade& time_grade, const StatModalRender& params) {
    const ImVec2 tl = *params.top_left;
    const ImVec2 br = *params.bottom_right;
    const float alpha = params.alpha;
    const float width = br.x - tl.x;

    // Translucent panel (bg_modal_translucent already encodes the 65% base opacity;
    // the fade alpha multiplies it).
    dl->AddRectFilled(tl, br, token_alpha_u32(theme::ColorToken::BgModalTranslucent, alpha), 8.0f);

    const float pad = width * 0.05f;
    const bool tabbed = has_tier_tabs(scenario);
    float body_top = tl.y + pad;
    if (tabbed) {
        const StripGeom g = tab_strip_geom(tl.x, tl.y, width, ImGui::GetTextLineHeight());
        draw_tab_strip(dl, g, active_tab, params.strip_focused, alpha);
        body_top = g.y + g.h + pad * 0.5f;
    }

    const std::array<float, 3> col_x = {tl.x + pad, tl.x + width * 0.40f, tl.x + width * 0.74f};
    const ImVec2 body_tl{tl.x + pad, body_top};

    if (tabbed && active_tab == RecapTab::Summary) {
        draw_summary_body(dl, body_tl, width - pad * 2.0f, result, scenario, time_grade, alpha);
        return;
    }

    const std::vector<RecapRow> rows =
        tabbed ? build_tier_rows(result, static_cast<std::uint8_t>(active_tab))
               : build_flat_rows(result);

    const float row_h = ImGui::GetTextLineHeight() * 1.8f;
    float y = body_top;
    y = draw_header_row(dl, tl, width, y, row_h, col_x, alpha);  // A2: labeled header
    for (const RecapRow& row : rows) {
        y = draw_recap_row(dl, tl, width, y, row_h, col_x, row, alpha);
    }
    y = draw_time_grade_row(dl, tl, width, y, row_h, col_x, time_grade, alpha);

    // A3: separate the Overall row from the per-input rows with a gap + a heavier
    // rule, then render it as the dominant grade row (taller, larger font). `pad`
    // is the modal body inset computed above.
    y += row_h * 0.5f;
    dl->AddLine(ImVec2{tl.x + pad, y}, ImVec2{tl.x + width - pad, y},
                token_alpha_u32(theme::ColorToken::SeparatorLine, alpha), 2.0f);
    y += row_h * 0.25f;
    draw_overall_row(dl, tl, y, row_h * kOverallRowScale, col_x, scenario.side_pot,
                     rows_accuracy_pct(std::span<const RecapRow>{rows}), alpha);
}

}  // namespace poker_trainer::render
