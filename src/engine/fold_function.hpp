#pragma once

#include "engine/rng_seed.hpp"

// Deterministic Fold Function (Module 1, locked V8.1) and the per-scenario
// sampling of its baseline tendency F.

namespace poker_trainer::engine {

// Clamp bounds on P(fold), straight from Module 1's formula.
inline constexpr double kFoldProbabilityMin = 0.05;
inline constexpr double kFoldProbabilityMax = 0.95;

// P(fold) = clamp(F + 0.15 * (bet_fraction - 0.5), 0.05, 0.95).
//
//   F             opponent baseline fold tendency for this scenario
//   bet_fraction  the bet size as a fraction of the pot (1/3, 1/2, 1, 3/2, ...)
//
// Bigger bets fold the opponent more often (the +0.15 * (bet_fraction - 0.5)
// term), bounded so the opponent is never a pure station or a pure folder.
[[nodiscard]] double fold_probability(double f, double bet_fraction) noexcept;

// P(call) = 1 - P(fold).
[[nodiscard]] double call_probability(double f, double bet_fraction) noexcept;

// Sample the baseline tendency F for one scenario: a uniform draw from `eng`
// mapped into the difficulty range [diff_min, diff_max] (Module 1's "randomized
// baseline tendency sampled from the seeded RNG within the user's current
// settings range," default [0.2, 0.8]). F is a VARIABLE property — re-rolled
// against the current difficulty range, not seed-locked across settings — so it
// is drawn after all structural draws in the generator. A degenerate range
// (min >= max) yields F = diff_min.
[[nodiscard]] double sample_fold_baseline(RngEngine& eng,
                                          double diff_min,
                                          double diff_max) noexcept;

}  // namespace poker_trainer::engine
