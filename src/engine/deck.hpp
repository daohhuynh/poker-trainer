#pragma once

#include "engine/rng_seed.hpp"
#include "engine/scenario.hpp"

#include <array>

// Deck Manager (Module 1): a standard 52-card deck, a portable shuffle, and the
// remaining-deck view the True Evaluator uses for Outs verification.

namespace poker_trainer::engine {

using Deck = std::array<Card, kDeckSize>;

// The canonical ordered 52-card deck: rank-major (2..A), suit-minor
// (Clubs, Diamonds, Hearts, Spades). The order is an internal convention; what
// matters is that it never changes, so the shuffle's seed->permutation mapping
// stays stable. The deck golden tests lock it.
[[nodiscard]] Deck make_ordered_deck() noexcept;

// Fisher-Yates shuffle in place, drawing through uniform_uint (rng_util.hpp) so
// the resulting permutation is identical on every platform for a given engine
// state. Standard library std::shuffle is deliberately NOT used (see rng_util.hpp).
void shuffle_deck(Deck& deck, RngEngine& eng) noexcept;

}  // namespace poker_trainer::engine
