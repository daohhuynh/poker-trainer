#pragma once

#include "engine/scenario_id.hpp"

#include <cstdint>
#include <random>

namespace poker_trainer::engine {

// The underlying engine type. Mersenne Twister 64-bit variant, chosen
// because the Scenario ID is 64 bits and a 64-bit engine consumes it
// without truncation. Mersenne Twister is locked as the algorithm for
// scenario generation across all versions of the trainer; changing it
// would break reproducibility for every saved or shared scenario.
using RngEngine = std::mt19937_64;

// A seeded RNG wrapper. Holds a Mersenne Twister 64-bit engine seeded
// from a ScenarioId. Provides direct access to the engine for use with
// std::uniform_int_distribution, std::shuffle, etc.
class RngSeed {
public:
    // Construct from a ScenarioId. The id must be valid (non-zero);
    // passing kInvalidScenarioId is a contract violation. In debug
    // builds this is asserted; in release builds the behavior is to
    // seed with the zero value, which is still a valid Mersenne
    // Twister seed but produces a sequence the engine considers
    // unreachable from any legitimate scenario.
    explicit RngSeed(ScenarioId id) noexcept;

    // Non-copyable. Copying an RNG produces two RNGs that yield the
    // same sequence, which is rarely the intent and almost always a
    // bug. Force callers to make this explicit.
    RngSeed(const RngSeed&) = delete;
    RngSeed& operator=(const RngSeed&) = delete;

    // Movable. Useful for transferring ownership of the RNG into a
    // scenario state object.
    RngSeed(RngSeed&&) noexcept = default;
    RngSeed& operator=(RngSeed&&) noexcept = default;

    // Access the underlying engine. Returned by reference so callers
    // can pass it to standard distributions and algorithms.
    [[nodiscard]] RngEngine& engine() noexcept { return engine_; }
    [[nodiscard]] const RngEngine& engine() const noexcept { return engine_; }

    // The ScenarioId this RNG was seeded from. Useful for logging
    // and debugging.
    [[nodiscard]] ScenarioId seed_id() const noexcept { return seed_id_; }

private:
    RngEngine engine_;
    ScenarioId seed_id_;
};

}  // namespace poker_trainer::engine
