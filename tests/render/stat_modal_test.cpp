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

// A multi-tier Semi-Bluff grading result: Fold + EV per tier (0..3), then the
// bet-size-independent Equity-if-Called, then the single Bet Size pick.
eng::GradingResult multi_tier_semibluff() {
    eng::GradingResult r{};
    for (std::uint8_t t = 0; t < eng::kBetTierCount; ++t) {
        r.inputs.push_back(make_grade(eng::InputId::FoldProbability, t, 40.0 + t, 40.0, 5.0, true));
        r.inputs.push_back(make_grade(eng::InputId::Ev, t, 100.0 + t, 100.0, 5.0, true));
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
    EXPECT_EQ(rnd::input_display_name(eng::InputId::FoldProbability), "Fold Probability");
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

TEST(StatModalModel, TierTabsOnlyForMultiTierAggressor) {
    EXPECT_TRUE(rnd::has_tier_tabs(aggressor_scenario(true)));
    EXPECT_FALSE(rnd::has_tier_tabs(aggressor_scenario(false)));
    EXPECT_FALSE(rnd::has_tier_tabs(caller_scenario()));
}

// A tier tab shows that tier's Fold + EV, then the echoed scenario-level inputs
// (Equity if Called, Bet Size).
TEST(StatModalModel, TierRowsAreThatTierPlusEchoedScenarioInputs) {
    const eng::GradingResult r = multi_tier_semibluff();
    const std::vector<rnd::RecapRow> rows = rnd::build_tier_rows(r, 2);
    ASSERT_EQ(rows.size(), 4u);
    EXPECT_EQ(rows[0].input, eng::InputId::FoldProbability);
    EXPECT_EQ(rows[0].tier_index, std::optional<std::uint8_t>{2});
    EXPECT_EQ(rows[1].input, eng::InputId::Ev);
    EXPECT_EQ(rows[1].tier_index, std::optional<std::uint8_t>{2});
    EXPECT_EQ(rows[2].input, eng::InputId::Equity);
    EXPECT_FALSE(rows[2].tier_index.has_value());
    EXPECT_EQ(rows[3].input, eng::InputId::BetSize);
    EXPECT_FALSE(rows[3].tier_index.has_value());
}

// Each tier tab echoes the SAME bet-size-independent inputs (Equity / Bet Size).
TEST(StatModalModel, EveryTierTabEchoesScenarioInputs) {
    const eng::GradingResult r = multi_tier_semibluff();
    for (std::uint8_t t = 0; t < eng::kBetTierCount; ++t) {
        const std::vector<rnd::RecapRow> rows = rnd::build_tier_rows(r, t);
        ASSERT_EQ(rows.size(), 4u);
        EXPECT_EQ(rows[2].input, eng::InputId::Equity);
        EXPECT_EQ(rows[3].input, eng::InputId::BetSize);
    }
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
    // Tier 0: both correct; Tier 1: one of two; scenario-level Bet Size correct.
    r.inputs.push_back(make_grade(eng::InputId::FoldProbability, 0, 40, 40, 5, true));
    r.inputs.push_back(make_grade(eng::InputId::Ev, 0, 100, 100, 5, true));
    r.inputs.push_back(make_grade(eng::InputId::FoldProbability, 1, 50, 99, 5, false));
    r.inputs.push_back(make_grade(eng::InputId::Ev, 1, 110, 110, 5, true));
    r.inputs.push_back(make_grade(eng::InputId::BetSize, std::nullopt, 1, 1, 5, true));

    const rnd::SummaryData s = rnd::build_summary(r);
    EXPECT_EQ(s.total, 5);
    EXPECT_EQ(s.total_correct, 4);
    EXPECT_EQ(s.per_tier[0].correct, 2);
    EXPECT_EQ(s.per_tier[0].total, 2);
    EXPECT_EQ(s.per_tier[1].correct, 1);
    EXPECT_EQ(s.per_tier[1].total, 2);
    EXPECT_EQ(s.per_tier[2].total, 0);  // bet size is scenario-level, not per-tier
    EXPECT_EQ(rnd::summary_pct(s), 80);
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
