// Deck Manager tests: the canonical ordered deck, and a portable Fisher-Yates
// shuffle that is a true permutation and reproduces identically from a seed.

#include "engine/deck.hpp"

#include "engine/rng_seed.hpp"
#include "engine/scenario.hpp"
#include "engine/scenario_id.hpp"

#include <array>
#include <cstddef>

#include <gtest/gtest.h>

namespace pe = poker_trainer::engine;

namespace {

bool all_distinct(const pe::Deck& deck) {
    std::array<bool, pe::kDeckSize> seen{};
    for (const pe::Card& c : deck) {
        const std::size_t idx = pe::card_index(c);
        if (idx >= pe::kDeckSize || seen[idx]) {
            return false;
        }
        seen[idx] = true;
    }
    return true;
}

}  // namespace

TEST(Deck, OrderedDeckHasFiftyTwoDistinctCards) {
    const pe::Deck deck = pe::make_ordered_deck();
    EXPECT_EQ(deck.size(), pe::kDeckSize);
    EXPECT_TRUE(all_distinct(deck));
}

TEST(Deck, OrderedDeckCoversEveryRankAndSuit) {
    const pe::Deck deck = pe::make_ordered_deck();
    std::array<int, pe::kRankCount> rank_counts{};
    std::array<int, pe::kSuitCount> suit_counts{};
    for (const pe::Card& c : deck) {
        ++rank_counts[static_cast<std::size_t>(c.rank - pe::kMinRank)];
        ++suit_counts[static_cast<std::size_t>(c.suit)];
    }
    for (const int n : rank_counts) {
        EXPECT_EQ(n, 4);  // four suits per rank
    }
    for (const int n : suit_counts) {
        EXPECT_EQ(n, 13);  // thirteen ranks per suit
    }
}

TEST(Deck, ShuffleIsAPermutation) {
    pe::RngSeed seed{pe::ScenarioId{0xABCDEF12u}};
    pe::Deck deck = pe::make_ordered_deck();
    pe::shuffle_deck(deck, seed.engine());
    EXPECT_TRUE(all_distinct(deck));
}

TEST(Deck, ShuffleIsDeterministicFromSeed) {
    pe::RngSeed seed_a{pe::ScenarioId{777}};
    pe::RngSeed seed_b{pe::ScenarioId{777}};
    pe::Deck deck_a = pe::make_ordered_deck();
    pe::Deck deck_b = pe::make_ordered_deck();
    pe::shuffle_deck(deck_a, seed_a.engine());
    pe::shuffle_deck(deck_b, seed_b.engine());
    EXPECT_EQ(deck_a, deck_b);
}

TEST(Deck, DifferentSeedsGiveDifferentOrder) {
    pe::RngSeed seed_a{pe::ScenarioId{1}};
    pe::RngSeed seed_b{pe::ScenarioId{2}};
    pe::Deck deck_a = pe::make_ordered_deck();
    pe::Deck deck_b = pe::make_ordered_deck();
    pe::shuffle_deck(deck_a, seed_a.engine());
    pe::shuffle_deck(deck_b, seed_b.engine());
    EXPECT_NE(deck_a, deck_b);
}
