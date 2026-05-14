#include "engine/rng_seed.hpp"

#include <cassert>

namespace poker_trainer::engine {

RngSeed::RngSeed(ScenarioId id) noexcept
    : engine_(id.value), seed_id_(id) {
    assert(is_valid(id) && "RngSeed constructed with invalid ScenarioId");
}

}  // namespace poker_trainer::engine
