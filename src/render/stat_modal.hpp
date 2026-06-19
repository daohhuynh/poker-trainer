#pragma once

#include "engine/scenario.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

// Zone 13 — the Post-Round stat modal's recap model + render.
//
// The PURE recap model (which rows show per tab, the three-column cell values,
// the Summary aggregation, the Time-Grade overtime/undertime decision, and the
// pass -> dealer-expression mapping) is unit-tested and lives here ImGui-free.
// The render functions take an ImDrawList* the Post-Round render path supplies;
// they are forward-declared the same way Zone 08's render headers are, so the
// unit tests include this header for the pure model and never pull in ImGui.

struct ImDrawList;
struct ImVec2;

namespace poker_trainer::render {

// The tabs of a multi-tier Aggressor recap: one per bet-size tier plus Summary.
// Tier1..Tier4 map 1:1 to BetTier ordinals; Summary is the scenario-level view.
enum class RecapTab : std::uint8_t {
    Tier1 = 0,
    Tier2 = 1,
    Tier3 = 2,
    Tier4 = 3,
    Summary = 4,
};
inline constexpr std::size_t kRecapTabCount = 5;

// Column-1 display name for a math input (Module 5's input set). The Equity input
// covers both the Caller's Equity and the Aggressor Semi-Bluff's Equity-if-Called.
[[nodiscard]] std::string_view input_display_name(engine::InputId input) noexcept;

// The three-column recap header labels (A2): column 0 is blank (it heads the input-
// name column), column 1 names the target value, column 2 the user's. Pure so the
// wording lives in one tested place; the render draws columns 1-2 (no column 0 text,
// no vertical divider lines).
[[nodiscard]] std::array<std::string_view, 3> recap_column_headers() noexcept;

// One three-column recap row: Column 1 is input_display_name(input); Column 2 is
// the correct answer with its grading deviation (correct_value +/- margin); Column
// 3 is the user's submitted answer, color-coded state_pass when `correct`,
// state_fail otherwise. `submitted` is nullopt when the box was left blank.
struct RecapRow {
    engine::InputId input{engine::InputId::PotOdds};
    std::optional<std::uint8_t> tier_index;
    double correct_value{0.0};
    double margin{0.0};
    std::optional<double> submitted;
    bool correct{false};
};

// Whether this scenario's recap shows the tier-tab strip: only a multi-tier
// Aggressor (Bet Sizing Engine on) has per-tier tabs. Caller and single-tier
// Aggressor render the recap body directly with no strip.
[[nodiscard]] bool has_tier_tabs(const engine::ScenarioState& scenario) noexcept;

// Rows for one tier tab (tier 0..3) of a multi-tier Aggressor: that tier's
// per-tier Fold Probability + EV, followed by the bet-size-independent inputs
// echoed on every tab (Equity if Called when present, then the single Bet Size
// pick) for visual symmetry.
[[nodiscard]] std::vector<RecapRow> build_tier_rows(const engine::GradingResult& result,
                                                    std::uint8_t tier);

// Rows for a non-tabbed recap (Caller, single-tier Aggressor): every graded input,
// in grading order.
[[nodiscard]] std::vector<RecapRow> build_flat_rows(const engine::GradingResult& result);

// Accuracy across a set of rows, as a rounded 0..100 percentage. Empty -> 0.
[[nodiscard]] int rows_accuracy_pct(std::span<const RecapRow> rows) noexcept;

// Per-tier correct/total tally (the Summary tab's per-tier breakdown row).
struct TierTally {
    int correct{0};
    int total{0};
};

// Scenario-level aggregation for the Summary tab: total inputs correct out of
// total across all tiers (the "9/12 correct (75%)" line), plus the per-tier
// breakdown (each tier's graded Fold + EV). Bet-size-independent inputs and the
// single Bet Size pick fold into total/total_correct but into no per-tier tally.
struct SummaryData {
    int total_correct{0};
    int total{0};
    std::array<TierTally, engine::kBetTierCount> per_tier{};
};

[[nodiscard]] SummaryData build_summary(const engine::GradingResult& result) noexcept;

// Rounded 0..100 percentage for a summary (total_correct / total). Empty -> 0.
[[nodiscard]] int summary_pct(const SummaryData& summary) noexcept;

// The Time-Grade row's values. Real timing is Zone 10's (Temporal, W4); until then
// the Post-Round path feeds stub values through this struct so the row + coloring
// render against the (stubbed) timer.
struct TimeGrade {
    int target_s{0};
    int actual_s{0};
};

// True when the user went over their target time (Actual rendered state_fail);
// false when at or under (Actual rendered state_pass / undertime).
[[nodiscard]] bool time_grade_overtime(const TimeGrade& grade) noexcept;

// The front-facing dealer's performance expression. Pass -> Neutral
// (butler_neutral / frog blush); anything else -> Raised (butler_raised / frog
// tongue). Set at scenario completion and held constant across tab navigation.
enum class DealerExpression : std::uint8_t {
    Neutral = 0,
    Raised = 1,
};

[[nodiscard]] DealerExpression dealer_expression(bool pass) noexcept;

// ===== Tier-tab strip geometry (pure; shared by render + mouse hit-test) =======

// The bounding box of the tier-tab strip and a single tab cell, in screen pixels.
// Both the renderer (drawing the strip + the bounded focus ring) and the screen's
// mouse handling (which tab was clicked) derive cell rects from this one formula,
// so they never drift.
struct StripGeom {
    float x{0.0f};       // strip left
    float y{0.0f};       // strip top
    float tab_w{0.0f};   // width of one tab cell
    float h{0.0f};       // strip height
};

// Compute the strip geometry from the modal's top-left and overall width.
[[nodiscard]] StripGeom tab_strip_geom(float modal_tl_x, float modal_tl_y, float modal_width,
                                       float text_line_height) noexcept;

// The tab index (0..4) whose cell contains (px, py), or -1 when the point is
// outside the strip. Unit-tested.
[[nodiscard]] int tab_index_at(const StripGeom& geom, float px, float py) noexcept;

// ===== Render (ImGui; forward-declared types keep this header pure) ============

// Parameters for one stat-modal render pass. The Post-Round path computes the
// modal rect, the active tab, the (stubbed) Time-Grade, the fade `alpha` (0..1,
// the Phase-3 modal fade-in), and whether the tier-tab strip currently holds
// keyboard focus (so the bounded focus ring renders).
struct StatModalRender {
    ImVec2* top_left;       // modal top-left, in screen pixels
    ImVec2* bottom_right;   // modal bottom-right
    float alpha;            // modal fade-in multiplier (1.0 once fully arrived)
    bool strip_focused;     // draw the 2px border_focus ring around the strip
};

// Render the full stat modal: translucent panel, tier-tab strip (multi-tier
// only), the active tab's body (three-column rows + Time-Grade row + Overall row,
// or the Summary layout), and the side-pot icon next to "Overall" when applicable.
void render_stat_modal(ImDrawList* dl, const engine::ScenarioState& scenario,
                       const engine::GradingResult& result, RecapTab active_tab,
                       const TimeGrade& time_grade, const StatModalRender& params);

}  // namespace poker_trainer::render
