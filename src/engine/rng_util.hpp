#pragma once

#include "engine/rng_seed.hpp"

#include <cstdint>

// Portable random-draw primitives for deterministic scenario generation.
//
// std::mt19937_64's output SEQUENCE is fully specified by the C++ standard, so
// raw engine() calls are identical on every platform. std::uniform_int_distribution
// and std::shuffle are NOT — their internal consumption of the engine is
// implementation-defined and differs across libstdc++ / libc++ / MSVC. Module 1
// requires a scenario to reconstruct identically on every platform, so the
// engine never uses those facilities; it draws through the helpers below, which
// depend only on the standardized raw output and plain integer/float arithmetic.

namespace poker_trainer::engine {

// An unbiased integer in [0, bound) drawn from `eng`. Rejection sampling removes
// the modulo bias that a bare `eng() % bound` would introduce. `bound` must be
// nonzero (a zero bound is a caller error). The rejection loop retries only the
// small tail of outputs that would skew the distribution; its expected number of
// iterations is below 2 for any bound.
[[nodiscard]] inline std::uint64_t uniform_uint(RngEngine& eng, std::uint64_t bound) noexcept {
    // Floor below which an output must be rejected so the surviving range is an
    // exact multiple of `bound`. Computed as (2^64 mod bound) via unsigned
    // negation: -bound == 2^64 - bound, and (2^64 - bound) % bound == 2^64 % bound.
    const std::uint64_t reject_floor = (~bound + 1u) % bound;
    std::uint64_t r = eng();
    while (r < reject_floor) {
        r = eng();
    }
    return r % bound;
}

// A double in [0, 1) drawn from `eng`. Uses the top 53 bits of one 64-bit output
// (the full mantissa precision of a double), the canonical portable construction.
[[nodiscard]] inline double uniform_unit(RngEngine& eng) noexcept {
    // 2^-53 as an exact binary literal; (output >> 11) yields a 53-bit integer.
    return static_cast<double>(eng() >> 11) * 0x1.0p-53;
}

}  // namespace poker_trainer::engine
