#include "engine/hand_eval.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace poker_trainer::engine {

bool has_straight(std::span<const Card> cards) noexcept {
    // present[r] is true when rank r appears. Index 1 is reserved for the Ace
    // playing low (the wheel A-2-3-4-5); index 14 is the Ace playing high.
    std::array<bool, 15> present{};
    for (const Card& c : cards) {
        present[c.rank] = true;
    }
    if (present[14]) {
        present[1] = true;
    }
    for (std::size_t low = 1; low <= 10; ++low) {
        bool run = true;
        for (std::size_t k = 0; k < 5; ++k) {
            if (!present[low + k]) {
                run = false;
                break;
            }
        }
        if (run) {
            return true;
        }
    }
    return false;
}

bool has_flush(std::span<const Card> cards) noexcept {
    std::array<int, kSuitCount> counts{};
    for (const Card& c : cards) {
        ++counts[static_cast<std::size_t>(c.suit)];
    }
    for (const int n : counts) {
        if (n >= 5) {
            return true;
        }
    }
    return false;
}

int count_draw_outs(const std::array<Card, 2>& hole,
                    const std::array<Card, 5>& board,
                    std::uint8_t board_count) noexcept {
    // Collect the known cards (hole + revealed board).
    std::array<Card, 7> known{};
    std::size_t n = 0;
    known[n++] = hole[0];
    known[n++] = hole[1];
    for (std::size_t i = 0; i < static_cast<std::size_t>(board_count); ++i) {
        known[n++] = board[i];
    }
    const std::span<const Card> known_span(known.data(), n);

    const bool flush_made = has_flush(known_span);
    const bool straight_made = has_straight(known_span);

    std::array<bool, kDeckSize> seen{};
    for (std::size_t i = 0; i < n; ++i) {
        seen[card_index(known[i])] = true;
    }

    int outs = 0;
    for (std::uint8_t rank = kMinRank; rank <= kMaxRank; ++rank) {
        for (std::size_t s = 0; s < kSuitCount; ++s) {
            const Card candidate{rank, static_cast<Suit>(static_cast<std::uint8_t>(s))};
            if (seen[card_index(candidate)]) {
                continue;
            }
            std::array<Card, 8> test{};
            for (std::size_t i = 0; i < n; ++i) {
                test[i] = known[i];
            }
            test[n] = candidate;
            const std::span<const Card> test_span(test.data(), n + 1);

            const bool completes_flush = !flush_made && has_flush(test_span);
            const bool completes_straight = !straight_made && has_straight(test_span);
            if (completes_flush || completes_straight) {
                ++outs;
            }
        }
    }
    return outs;
}

double equity_from_outs(int outs, Street street) noexcept {
    int multiplier = 0;
    if (street == Street::Flop) {
        multiplier = 4;  // two cards to come
    } else if (street == Street::Turn) {
        multiplier = 2;  // one card to come
    }
    const double equity = static_cast<double>(outs * multiplier);
    return std::min(equity, 100.0);
}

}  // namespace poker_trainer::engine
