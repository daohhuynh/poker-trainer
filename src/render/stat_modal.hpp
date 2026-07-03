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

// Whether this scenario's recap shows the tier-tab strip: any multi-tier Aggressor
// sub-type (each now has a per-tier input -- EV always, plus Breakeven Fold % for
// Pure/Semi-Bluff) gets per-tier tabs. Caller and single-tier Aggressor render the
// recap body directly with no strip.
[[nodiscard]] bool has_tier_tabs(const engine::ScenarioState& scenario) noexcept;

// The FULL input set for one tier tab (tier 0..3): that tier's per-tier grades
// (Breakeven Fold % and/or EV) PLUS the scenario-level inputs (Equity if Called, Bet
// Size). Each tier is self-contained -- the persistent Equity / Bet Size answers are
// echoed into every tier and counted toward that tier's own accuracy, since a user may
// change them while on any tier. Order: Breakeven Fold %, Equity if Called, EV, Bet
// Size (absent inputs skipped for the sub-type).
[[nodiscard]] std::vector<RecapRow> build_tier_rows(const engine::GradingResult& result,
                                                    std::uint8_t tier);

// The SCENARIO-LEVEL rows (the grades with no tier index): Equity if Called (Value
// Bet / Semi-Bluff) then the single Bet Size pick, in grading order.
[[nodiscard]] std::vector<RecapRow> build_scenario_level_rows(const engine::GradingResult& result);

// Rows for a non-tabbed recap (Caller, single-tier Aggressor): every graded input, in
// grading order.
[[nodiscard]] std::vector<RecapRow> build_flat_rows(const engine::GradingResult& result);

// Accuracy across a set of rows, as a rounded 0..100 percentage. Empty -> 0.
[[nodiscard]] int rows_accuracy_pct(std::span<const RecapRow> rows) noexcept;

// One tier's self-contained accuracy (rounded 0..100): the accuracy across
// build_tier_rows(result, tier) -- that tier's per-tier inputs plus the echoed
// scenario-level inputs. Shown as the tier tab's bottom "Overall" row and as the
// tier's row in the Summary tab.
[[nodiscard]] int tier_accuracy_pct(const engine::GradingResult& result, std::uint8_t tier);

// Per-tier correct/total tally (the Summary tab's per-tier breakdown row).
struct TierTally {
    int correct{0};
    int total{0};
};

// Scenario-level aggregation for the Summary tab: total inputs correct out of
// total across all tiers (the "9/12 correct (75%)" line), plus the per-tier
// breakdown (each tier's graded per-tier inputs: Breakeven Fold % and/or EV).
// Bet-size-independent inputs and the single Bet Size pick fold into
// total/total_correct but into no per-tier tally.
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
    // Tutorial scenarios disable the Delta Timer entirely: the row then renders the
    // "Tutorial — timer disabled" placeholder instead of Target / Actual values
    // (ARCHITECTURE §Forced Settings). The Post-Round path sets this from the live
    // tutorial phase.
    bool tutorial_disabled{false};
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
    float scroll_offset;    // body scroll in px (0 = top); the tab strip stays pinned
};

// Render the full stat modal: translucent panel, PINNED tier-tab strip (multi-tier
// only), and the active tab's scrollable body (the tier's three-column input rows +
// Time-Grade row + the tier's Overall row, or the Summary's per-tier Overall rows +
// the whole-round Overall row). The body is clipped to the modal rect and offset by
// `params.scroll_offset` so it stays contained. Returns the body's overflow in px
// (content height beyond the visible body region, clamped >= 0) so the caller can
// clamp scrolling and enable the mouse wheel.
[[nodiscard]] float render_stat_modal(ImDrawList* dl, const engine::ScenarioState& scenario,
                                      const engine::GradingResult& result, RecapTab active_tab,
                                      const TimeGrade& time_grade, const StatModalRender& params);

}  // namespace poker_trainer::render
