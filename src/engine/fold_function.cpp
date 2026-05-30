#include "engine/fold_function.hpp"

#include "engine/rng_util.hpp"

#include <algorithm>

namespace poker_trainer::engine {

double fold_probability(double f, double bet_fraction) noexcept {
    const double raw = f + 0.15 * (bet_fraction - 0.5);
    return std::clamp(raw, kFoldProbabilityMin, kFoldProbabilityMax);
}

double call_probability(double f, double bet_fraction) noexcept {
    return 1.0 - fold_probability(f, bet_fraction);
}

double sample_fold_baseline(RngEngine& eng, double diff_min, double diff_max) noexcept {
    const double width = diff_max - diff_min;
    if (width <= 0.0) {
        // Degenerate or inverted range: F collapses to the lower bound. The draw
        // is still consumed so the RNG stream position does not depend on the
        // difficulty range.
        (void)uniform_unit(eng);
        return diff_min;
    }
    return diff_min + uniform_unit(eng) * width;
}

}  // namespace poker_trainer::engine
