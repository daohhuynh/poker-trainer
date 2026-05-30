#pragma once

#include "engine/rng_seed.hpp"

// Side Pot Engine (Module 1): a base-percentage chance that a scenario is an
// all-in side-pot split.
//
// Side-pot status is a property of the scenario, not a modifier of the graded
// math: Module 5 lists no side-pot-specific math inputs, and side pots surface
// only as a visual layer (Z08's two-pile + all-in marker) and a Z10 timing
// modifier (+25% target time). The True Evaluator therefore computes pot odds,
// EV, outs, and equity on the main pot exactly as it would for a non-side-pot
// scenario; this flag drives rendering and timing alone.

namespace poker_trainer::engine {

// Roll side-pot status from one uniform draw: true with probability `frequency`
// (the configurable Side Pot Engine frequency, default ~0.10). `frequency` is
// clamped to [0, 1]; the draw is always consumed so the RNG stream position is
// independent of the configured value.
[[nodiscard]] bool roll_side_pot(RngEngine& eng, float frequency) noexcept;

}  // namespace poker_trainer::engine
