#include "engine/deck.hpp"

#include "engine/rng_util.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace poker_trainer::engine {

Deck make_ordered_deck() noexcept {
    Deck deck{};
    std::size_t idx = 0;
    for (std::uint8_t rank = kMinRank; rank <= kMaxRank; ++rank) {
        for (std::size_t s = 0; s < kSuitCount; ++s) {
            deck[idx] = Card{rank, static_cast<Suit>(static_cast<std::uint8_t>(s))};
            ++idx;
        }
    }
    return deck;
}

void shuffle_deck(Deck& deck, RngEngine& eng) noexcept {
    // Fisher-Yates from the back: swap each deck[i] with a uniformly chosen
    // deck[j], j in [0, i]. Uses uniform_uint (rng_util.hpp) rather than
    // std::shuffle so the permutation is identical on every platform.
    for (std::size_t i = kDeckSize - 1; i > 0; --i) {
        const std::uint64_t j = uniform_uint(eng, static_cast<std::uint64_t>(i) + 1u);
        std::swap(deck[i], deck[static_cast<std::size_t>(j)]);
    }
}

}  // namespace poker_trainer::engine
