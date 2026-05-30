#include "engine/side_pot.hpp"

#include "engine/rng_util.hpp"

#include <algorithm>

namespace poker_trainer::engine {

bool roll_side_pot(RngEngine& eng, float frequency) noexcept {
    const double p = std::clamp(static_cast<double>(frequency), 0.0, 1.0);
    return uniform_unit(eng) < p;
}

}  // namespace poker_trainer::engine
