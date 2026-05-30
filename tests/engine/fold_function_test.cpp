// Deterministic Fold Function tests: the locked V8.1 curve, its clamp bounds,
// the P(call) complement, and per-scenario F sampling within the difficulty range.

#include "engine/fold_function.hpp"

#include "engine/rng_seed.hpp"
#include "engine/scenario_id.hpp"

#include <gtest/gtest.h>

namespace pe = poker_trainer::engine;

namespace {
constexpr double kEps = 1e-9;
}

TEST(FoldFunction, HalfPotBetEqualsBaseline) {
    // bet_fraction = 0.5 zeroes the size term, so P(fold) == F exactly.
    EXPECT_NEAR(pe::fold_probability(0.50, 0.5), 0.50, kEps);
    EXPECT_NEAR(pe::fold_probability(0.20, 0.5), 0.20, kEps);
    EXPECT_NEAR(pe::fold_probability(0.80, 0.5), 0.80, kEps);
}

TEST(FoldFunction, BiggerBetsFoldMoreOften) {
    // F + 0.15 * (bet_fraction - 0.5), monotone increasing in bet_fraction.
    EXPECT_NEAR(pe::fold_probability(0.5, 1.0 / 3.0), 0.5 + 0.15 * (1.0 / 3.0 - 0.5), kEps);
    EXPECT_NEAR(pe::fold_probability(0.5, 1.0), 0.575, kEps);  // 0.5 + 0.15*0.5
    EXPECT_NEAR(pe::fold_probability(0.5, 1.5), 0.650, kEps);  // 0.5 + 0.15*1.0
    EXPECT_LT(pe::fold_probability(0.5, 1.0 / 3.0), pe::fold_probability(0.5, 0.5));
    EXPECT_LT(pe::fold_probability(0.5, 0.5), pe::fold_probability(0.5, 1.0));
    EXPECT_LT(pe::fold_probability(0.5, 1.0), pe::fold_probability(0.5, 1.5));
}

TEST(FoldFunction, ClampsToBounds) {
    // A very loose opponent and a tiny bet still folds at least 5% of the time.
    EXPECT_NEAR(pe::fold_probability(0.0, 1.0 / 3.0), pe::kFoldProbabilityMin, kEps);
    // A very tight opponent facing a big overbet never exceeds 95%.
    EXPECT_NEAR(pe::fold_probability(1.0, 2.0), pe::kFoldProbabilityMax, kEps);
}

TEST(FoldFunction, CallIsComplementOfFold) {
    for (const double frac : {1.0 / 3.0, 0.5, 1.0, 1.5}) {
        EXPECT_NEAR(pe::fold_probability(0.45, frac) + pe::call_probability(0.45, frac), 1.0, kEps);
    }
}

TEST(FoldFunction, SampledBaselineStaysInRange) {
    for (std::uint64_t id = 1; id <= 2000; ++id) {
        pe::RngSeed seed{pe::ScenarioId{id}};
        const double f = pe::sample_fold_baseline(seed.engine(), 0.2, 0.8);
        EXPECT_GE(f, 0.2);
        EXPECT_LT(f, 0.8 + kEps);
    }
}

TEST(FoldFunction, SampledBaselineIsDeterministic) {
    pe::RngSeed a{pe::ScenarioId{12345}};
    pe::RngSeed b{pe::ScenarioId{12345}};
    EXPECT_EQ(pe::sample_fold_baseline(a.engine(), 0.2, 0.8),
              pe::sample_fold_baseline(b.engine(), 0.2, 0.8));
}

TEST(FoldFunction, DegenerateRangeCollapsesToLowerBound) {
    pe::RngSeed seed{pe::ScenarioId{99}};
    EXPECT_NEAR(pe::sample_fold_baseline(seed.engine(), 0.4, 0.4), 0.4, kEps);
}
