#include "render/stat_modal.hpp"

#include "engine/scenario.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

namespace rnd = poker_trainer::render;
namespace eng = poker_trainer::engine;

namespace {

eng::InputGrade make_grade(eng::InputId input, std::optional<std::uint8_t> tier,
                           double correct_value, std::optional<double> submitted, double margin,
                           bool correct) {
    eng::InputGrade g{};
    g.input = input;
    g.tier_index = tier;
    g.correct_value = correct_value;
    g.submitted = submitted;
    g.margin = margin;
    g.correct = correct;
    return g;
}

// A multi-tier Semi-Bluff grading result: per tier (0..3) a Breakeven Fold % then a
// dollar EV (restored), then the bet-size-independent Equity-if-Called, then the
// single Bet Size pick. Mirrors the evaluator's grade order.
eng::GradingResult multi_tier_semibluff() {
    eng::GradingResult r{};
    for (std::uint8_t t = 0; t < eng::kBetTierCount; ++t) {
        r.inputs.push_back(make_grade(eng::InputId::FoldProbability, t, 40.0 + t, 40.0, 5.0, true));
        r.inputs.push_back(make_grade(eng::InputId::Ev, t, 10.0 + t, 10.0 + t, 0.5, true));
    }
    r.inputs.push_back(make_grade(eng::InputId::Equity, std::nullopt, 35.0, 35.0, 5.0, true));
    r.inputs.push_back(make_grade(eng::InputId::BetSize, std::nullopt, 1.0, 1.0, 5.0, true));
    return r;
}

eng::ScenarioState caller_scenario() {
    eng::ScenarioState s{};
    s.type = eng::ScenarioType::Caller;
    return s;
}

eng::ScenarioState aggressor_scenario(bool multi) {
    eng::ScenarioState s{};
    s.type = eng::ScenarioType::AggressorSemiBluff;
    s.multi_tier = multi;
    return s;
}

}  // namespace

TEST(StatModalModel, InputDisplayNames) {
    EXPECT_EQ(rnd::input_display_name(eng::InputId::PotOdds), "Pot Odds");
    EXPECT_EQ(rnd::input_display_name(eng::InputId::FoldProbability), "Breakeven Fold %");
    EXPECT_EQ(rnd::input_display_name(eng::InputId::BetSize), "Bet Size");
}

// A2: the labeled header row — column 0 blank (heads the input-name column),
// column 1 names the target answer, column 2 the user's.
TEST(StatModalModel, RecapColumnHeaders) {
    const std::array<std::string_view, 3> headers = rnd::recap_column_headers();
    EXPECT_TRUE(headers[0].empty());
    EXPECT_EQ(headers[1], "Correct Answer");
    EXPECT_EQ(headers[2], "Your Answer");
}

TEST(StatModalModel, TierTabsForEveryMultiTierAggressor) {
    EXPECT_TRUE(rnd::has_tier_tabs(aggressor_scenario(true)));    // Semi-Bluff multi-tier
    EXPECT_FALSE(rnd::has_tier_tabs(aggressor_scenario(false)));  // Semi-Bluff single-tier
    EXPECT_FALSE(rnd::has_tier_tabs(caller_scenario()));
    // Value Bet now has a per-tier input (EV) -> tier tabs when multi-tier.
    eng::ScenarioState vb{};
    vb.type = eng::ScenarioType::AggressorValueBet;
    vb.multi_tier = true;
    EXPECT_TRUE(rnd::has_tier_tabs(vb));
}

// A tier tab shows that tier's FULL input set, in on-screen order: Breakeven Fold %
// (this tier), Equity if Called (echoed), EV (this tier), Bet Size (echoed).
TEST(StatModalModel, TierRowsAreThatTiersFullInputSet) {
    const eng::GradingResult r = multi_tier_semibluff();
    const std::vector<rnd::RecapRow> rows = rnd::build_tier_rows(r, 2);
    ASSERT_EQ(rows.size(), 4u);
    EXPECT_EQ(rows[0].input, eng::InputId::FoldProbability);  // Breakeven Fold % (tier 2)
    EXPECT_EQ(rows[0].tier_index, std::optional<std::uint8_t>{2});
    EXPECT_EQ(rows[1].input, eng::InputId::Equity);           // echoed scenario-level
    EXPECT_FALSE(rows[1].tier_index.has_value());
    EXPECT_EQ(rows[2].input, eng::InputId::Ev);               // EV (tier 2)
    EXPECT_EQ(rows[2].tier_index, std::optional<std::uint8_t>{2});
    EXPECT_EQ(rows[3].input, eng::InputId::BetSize);          // echoed scenario-level
    EXPECT_FALSE(rows[3].tier_index.has_value());
}

// The Overall section is the scenario-level inputs: Equity if Called then Bet Size.
TEST(StatModalModel, ScenarioLevelRowsAreEquityThenBetSize) {
    const eng::GradingResult r = multi_tier_semibluff();
    const std::vector<rnd::RecapRow> rows = rnd::build_scenario_level_rows(r);
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[0].input, eng::InputId::Equity);
    EXPECT_FALSE(rows[0].tier_index.has_value());
    EXPECT_EQ(rows[1].input, eng::InputId::BetSize);
    EXPECT_FALSE(rows[1].tier_index.has_value());
}

// A tier's Overall % scores its own self-contained input set (per-tier + echoed
// scenario-level), so a wrong per-tier input drops only that tier -- not the others.
TEST(StatModalModel, TierAccuracyIsScopedToThatTier) {
    eng::GradingResult r{};
    // Tier 0: breakeven right, EV wrong. Tier 1: both right. Shared Equity + BetSize right.
    r.inputs.push_back(make_grade(eng::InputId::FoldProbability, 0, 25, 25, 5, true));
    r.inputs.push_back(make_grade(eng::InputId::Ev, 0, 10, 99, 0.5, false));
    r.inputs.push_back(make_grade(eng::InputId::FoldProbability, 1, 33, 33, 5, true));
    r.inputs.push_back(make_grade(eng::InputId::Ev, 1, 12, 12, 0.5, true));
    r.inputs.push_back(make_grade(eng::InputId::Equity, std::nullopt, 40, 40, 5, true));
    r.inputs.push_back(make_grade(eng::InputId::BetSize, std::nullopt, 2, 2, 5, true));
    // Tier 0: breakeven + Equity + BetSize correct, EV wrong -> 3/4 = 75%.
    EXPECT_EQ(rnd::tier_accuracy_pct(r, 0), 75);
    // Tier 1: all four correct -> 100%.
    EXPECT_EQ(rnd::tier_accuracy_pct(r, 1), 100);
}

TEST(StatModalModel, FlatRowsAreEveryGradedInput) {
    eng::GradingResult r{};
    r.inputs.push_back(make_grade(eng::InputId::PotOdds, std::nullopt, 25.0, 25.0, 5.0, true));
    r.inputs.push_back(make_grade(eng::InputId::Outs, std::nullopt, 9.0, 9.0, 0.0, true));
    r.inputs.push_back(make_grade(eng::InputId::Equity, std::nullopt, 36.0, 30.0, 5.0, false));
    r.inputs.push_back(make_grade(eng::InputId::Ev, std::nullopt, 12.0, 12.0, 0.5, true));
    const std::vector<rnd::RecapRow> rows = rnd::build_flat_rows(r);
    EXPECT_EQ(rows.size(), 4u);
    EXPECT_EQ(rows[2].input, eng::InputId::Equity);
    EXPECT_FALSE(rows[2].correct);
}

TEST(StatModalModel, RowsAccuracyPercent) {
    std::vector<rnd::RecapRow> rows;
    rows.push_back(rnd::RecapRow{eng::InputId::PotOdds, std::nullopt, 0, 0, std::nullopt, true});
    rows.push_back(rnd::RecapRow{eng::InputId::Outs, std::nullopt, 0, 0, std::nullopt, true});
    rows.push_back(rnd::RecapRow{eng::InputId::Equity, std::nullopt, 0, 0, std::nullopt, true});
    rows.push_back(rnd::RecapRow{eng::InputId::Ev, std::nullopt, 0, 0, std::nullopt, false});
    EXPECT_EQ(rnd::rows_accuracy_pct(std::span<const rnd::RecapRow>{rows}), 75);
    EXPECT_EQ(rnd::rows_accuracy_pct(std::span<const rnd::RecapRow>{}), 0);
}

TEST(StatModalModel, SummaryAggregatesTotalsAndPerTier) {
    eng::GradingResult r{};
    // Tier 0 breakeven correct; Tier 1 breakeven wrong; Equity + Bet Size correct.
    r.inputs.push_back(make_grade(eng::InputId::FoldProbability, 0, 25, 25, 5, true));
    r.inputs.push_back(make_grade(eng::InputId::FoldProbability, 1, 33, 99, 5, false));
    r.inputs.push_back(make_grade(eng::InputId::Equity, std::nullopt, 40, 40, 5, true));
    r.inputs.push_back(make_grade(eng::InputId::BetSize, std::nullopt, 2, 2, 5, true));

    const rnd::SummaryData s = rnd::build_summary(r);
    EXPECT_EQ(s.total, 4);
    EXPECT_EQ(s.total_correct, 3);
    EXPECT_EQ(s.per_tier[0].correct, 1);
    EXPECT_EQ(s.per_tier[0].total, 1);
    EXPECT_EQ(s.per_tier[1].correct, 0);
    EXPECT_EQ(s.per_tier[1].total, 1);
    EXPECT_EQ(s.per_tier[2].total, 0);  // Equity / Bet Size are scenario-level, not per-tier
    EXPECT_EQ(rnd::summary_pct(s), 75);
}

TEST(StatModalModel, TimeGradeOvertimeDecision) {
    EXPECT_TRUE(rnd::time_grade_overtime(rnd::TimeGrade{15, 22}));
    EXPECT_FALSE(rnd::time_grade_overtime(rnd::TimeGrade{15, 10}));
    EXPECT_FALSE(rnd::time_grade_overtime(rnd::TimeGrade{15, 15}));  // at target = not overtime
}

TEST(StatModalModel, DealerExpressionFromPass) {
    EXPECT_EQ(rnd::dealer_expression(true), rnd::DealerExpression::Neutral);
    EXPECT_EQ(rnd::dealer_expression(false), rnd::DealerExpression::Raised);
}

TEST(StatModalModel, TabStripGeometryAndHitTest) {
    const rnd::StripGeom g = rnd::tab_strip_geom(100.0f, 50.0f, 500.0f, 20.0f);
    // pad = 500*0.05 = 25; strip x = 125; tab_w = (500-50)/5 = 90.
    EXPECT_FLOAT_EQ(g.x, 125.0f);
    EXPECT_FLOAT_EQ(g.tab_w, 90.0f);

    EXPECT_EQ(rnd::tab_index_at(g, g.x + 5.0f, g.y + 1.0f), 0);
    EXPECT_EQ(rnd::tab_index_at(g, g.x + g.tab_w * 4.0f + 5.0f, g.y + 1.0f), 4);  // Summary
    EXPECT_EQ(rnd::tab_index_at(g, g.x - 1.0f, g.y + 1.0f), -1);
    EXPECT_EQ(rnd::tab_index_at(g, g.x + 5.0f, g.y - 1.0f), -1);
    EXPECT_EQ(rnd::tab_index_at(g, g.x + g.tab_w * 5.0f + 1.0f, g.y + 1.0f), -1);  // past Summary
}
