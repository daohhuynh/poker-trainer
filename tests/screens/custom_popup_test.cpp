// Zone 07 — Custom popup pure-logic unit tests (coupled solver, keystroke rules,
// Save/Reset/Play action semantics).

#include "screens/custom_popup.hpp"

#include "screens/mode_selection_screen.hpp"

#include "backbone/game_mode.hpp"

#include <optional>

#include <gtest/gtest.h>

namespace sc = poker_trainer::screens;
namespace bb = poker_trainer::backbone;

namespace {

// backbone::CustomConfig has no operator== (it is a Phase 0 contract we do not
// own), so compare field-wise.
void expect_weights(const bb::CustomConfig& got, int aggressor, int caller) {
    EXPECT_EQ(static_cast<int>(got.aggressor_weight), aggressor);
    EXPECT_EQ(static_cast<int>(got.caller_weight), caller);
}

// Faked persistence boundary for Save/Reset.
class FakeStore : public sc::CustomWeightsStore {
public:
    std::optional<bb::CustomConfig> saved;
    int save_calls{0};

    void save(bb::CustomConfig weights) override {
        saved = weights;
        ++save_calls;
    }
    [[nodiscard]] std::optional<bb::CustomConfig> load() const override { return saved; }
};

}  // namespace

// ----- Coupled-weight solver -----

TEST(CoupledSolver, SetAggressorDerivesCaller) {
    expect_weights(sc::solve_from(sc::WeightField::Aggressor, 70), 70, 30);
}

TEST(CoupledSolver, SetCallerDerivesAggressor) {
    expect_weights(sc::solve_from(sc::WeightField::Caller, 25), 75, 25);
}

TEST(CoupledSolver, ClampsAboveAndBelow) {
    expect_weights(sc::solve_from(sc::WeightField::Aggressor, 150), 100, 0);
    expect_weights(sc::solve_from(sc::WeightField::Aggressor, -5), 0, 100);
    expect_weights(sc::solve_from(sc::WeightField::Caller, 999), 0, 100);
}

TEST(CoupledSolver, SumIsAlways100ForArbitraryInput) {
    for (int v = -20; v <= 130; ++v) {
        const auto a = sc::solve_from(sc::WeightField::Aggressor, v);
        EXPECT_EQ(a.aggressor_weight + a.caller_weight, 100) << "aggr field v=" << v;
        const auto c = sc::solve_from(sc::WeightField::Caller, v);
        EXPECT_EQ(c.aggressor_weight + c.caller_weight, 100) << "caller field v=" << v;
    }
}

TEST(CoupledSolver, ArrowStepPropagatesAndClamps) {
    expect_weights(sc::step_weight(bb::CustomConfig{70, 30}, sc::WeightField::Aggressor, +1), 71, 29);
    expect_weights(sc::step_weight(bb::CustomConfig{70, 30}, sc::WeightField::Caller, +1), 69, 31);
    expect_weights(sc::step_weight(bb::CustomConfig{100, 0}, sc::WeightField::Aggressor, +1), 100, 0);
    expect_weights(sc::step_weight(bb::CustomConfig{0, 100}, sc::WeightField::Aggressor, -1), 0, 100);
}

// ----- Keystroke / text rules -----

TEST(Keystroke, AcceptsOnlyDigits) {
    for (char c = '0'; c <= '9'; ++c) {
        EXPECT_TRUE(sc::accepts_text_char(c)) << "digit " << c;
    }
    EXPECT_FALSE(sc::accepts_text_char('a'));
    EXPECT_FALSE(sc::accepts_text_char('-'));
    EXPECT_FALSE(sc::accepts_text_char('.'));
    EXPECT_FALSE(sc::accepts_text_char(' '));
    EXPECT_FALSE(sc::accepts_text_char('%'));
}

TEST(Keystroke, ParseClampsAndHandlesEmpty) {
    EXPECT_EQ(sc::parse_clamped_weight(""), 0);
    EXPECT_EQ(sc::parse_clamped_weight("42"), 42);
    EXPECT_EQ(sc::parse_clamped_weight("007"), 7);
    EXPECT_EQ(sc::parse_clamped_weight("100"), 100);
    EXPECT_EQ(sc::parse_clamped_weight("150"), 100);
    EXPECT_EQ(sc::parse_clamped_weight("99999"), 100);
}

// ----- Action-button semantics -----

TEST(ResetAction, NoPriorSaveReturns5050) {
    expect_weights(sc::reset_weights(std::nullopt), 50, 50);
    FakeStore store;  // load() returns nullopt
    expect_weights(sc::reset_to_saved(store), 50, 50);
}

TEST(ResetAction, RestoresLastSaved) {
    expect_weights(sc::reset_weights(bb::CustomConfig{70, 30}), 70, 30);
    FakeStore store;
    sc::save_weights(store, bb::CustomConfig{70, 30});
    expect_weights(sc::reset_to_saved(store), 70, 30);
}

TEST(SaveAction, PersistsCurrentWeights) {
    FakeStore store;
    sc::save_weights(store, bb::CustomConfig{80, 20});
    ASSERT_TRUE(store.saved.has_value());
    expect_weights(*store.saved, 80, 20);
    EXPECT_EQ(store.save_calls, 1);
}

TEST(PlayAction, EmitsCustomModeWithWeightsAndDoesNotPersist) {
    FakeStore store;
    const sc::LaunchRequest req = sc::launch_request_for_custom(bb::CustomConfig{65, 35});
    EXPECT_EQ(req.mode, bb::GameMode::Custom);
    ASSERT_TRUE(req.config.has_value());
    expect_weights(*req.config, 65, 35);
    // Play never touches the persistence boundary.
    EXPECT_EQ(store.save_calls, 0);
    EXPECT_FALSE(store.saved.has_value());
}

// ----- Other launch paths emit mode only (no engine contact, no config) -----

TEST(LaunchPaths, StandardAggressorCallerEmitModeOnly) {
    EXPECT_EQ(sc::launch_request_for_standard().mode, bb::GameMode::Standard);
    EXPECT_FALSE(sc::launch_request_for_standard().config.has_value());
    EXPECT_EQ(sc::launch_request_for_aggressor().mode, bb::GameMode::Aggressor);
    EXPECT_FALSE(sc::launch_request_for_aggressor().config.has_value());
    EXPECT_EQ(sc::launch_request_for_caller().mode, bb::GameMode::Caller);
    EXPECT_FALSE(sc::launch_request_for_caller().config.has_value());
}
