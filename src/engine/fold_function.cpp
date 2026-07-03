#include "engine/fold_function.hpp"

#include "engine/rng_util.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace poker_trainer::engine {

namespace {

// Street term: later streets fold more (draws have missed, more hands whiffed,
// the decision is more binary); the flop is the low point, the river the high.
[[nodiscard]] double street_fold_adj(Street street) noexcept {
    switch (street) {
        case Street::Preflop:
            return -0.06;  // widest continue ranges; opponents call the most
        case Street::Flop:
            return -0.03;
        case Street::Turn:
            return 0.03;
        case Street::River:
            return 0.08;
    }
    return 0.0;
}

// Gap between adjacent distinct ranks -> a connectedness score. Touching (gap 1)
// is maximally connected; a one-gapper (2) still leaves many straights; wider
// gaps taper off. A gap over 4 shares no straight and scores 0.
[[nodiscard]] double gap_connect_score(int gap) noexcept {
    switch (gap) {
        case 1:
            return 1.0;
        case 2:
            return 0.6;
        case 3:
            return 0.3;
        case 4:
            return 0.15;
        default:
            return 0.0;
    }
}

}  // namespace

double fold_probability(double f, double bet_fraction) noexcept {
    const double raw = f + kBetSizeFoldCoeff * (bet_fraction - kBetSizeFoldCenter);
    return std::clamp(raw, kFoldProbabilityMin, kFoldProbabilityMax);
}

double call_probability(double f, double bet_fraction) noexcept {
    return 1.0 - fold_probability(f, bet_fraction);
}

double fold_texture_wetness(const std::array<Card, 5>& board, std::uint8_t board_count) noexcept {
    const auto n = static_cast<std::size_t>(board_count);
    if (n < 3) {
        return 0.5;  // preflop / no meaningful texture: neutral
    }

    // Suitedness: the largest count of one suit on the board.
    std::array<int, kSuitCount> suit_counts{};
    for (std::size_t i = 0; i < n; ++i) {
        ++suit_counts[static_cast<std::size_t>(board[i].suit)];
    }
    const int max_suit = *std::max_element(suit_counts.begin(), suit_counts.end());
    const double flush_wet = max_suit >= 3 ? 1.0 : (max_suit == 2 ? 0.5 : 0.0);

    // Connectedness: distinct sorted ranks, averaged adjacent-gap score.
    std::array<std::uint8_t, kRankCount> present{};
    for (std::size_t i = 0; i < n; ++i) {
        present[static_cast<std::size_t>(board[i].rank - kMinRank)] = 1;
    }
    std::array<int, kRankCount> ranks{};
    std::size_t distinct = 0;
    for (std::size_t r = 0; r < kRankCount; ++r) {
        if (present[r] != 0) {
            ranks[distinct++] = static_cast<int>(r) + kMinRank;
        }
    }
    double straight_wet = 0.0;
    if (distinct >= 2) {
        double sum = 0.0;
        for (std::size_t i = 0; i + 1 < distinct; ++i) {
            sum += gap_connect_score(ranks[i + 1] - ranks[i]);
        }
        straight_wet = std::clamp(sum / static_cast<double>(distinct - 1), 0.0, 1.0);
    }

    double wet = 0.55 * flush_wet + 0.55 * straight_wet;
    // A paired board offers fewer straight/flush combinations -> drier.
    if (distinct < n) {
        wet -= 0.15;
    }
    return std::clamp(wet, 0.0, 1.0);
}

double fold_board_danger(const std::array<Card, 5>& board, std::uint8_t board_count) noexcept {
    const auto n = static_cast<std::size_t>(board_count);
    std::uint8_t top = 0;
    for (std::size_t i = 0; i < n; ++i) {
        top = std::max(top, board[i].rank);
    }
    switch (top) {
        case 14:
            return 1.0;   // Ace: the classic scare card
        case 13:
            return 0.55;  // King
        case 12:
            return 0.25;  // Queen
        default:
            return 0.0;
    }
}

double fold_center(Street street, const std::array<Card, 5>& board,
                   std::uint8_t board_count) noexcept {
    const double wetness = fold_texture_wetness(board, board_count);
    const double danger = fold_board_danger(board, board_count);
    const double center = kBaseFoldRate + street_fold_adj(street) +
                          kTextureFoldCoeff * (0.5 - wetness) + kDangerFoldCoeff * danger;
    return std::clamp(center, kFoldProbabilityMin, kFoldProbabilityMax);
}

double sample_situational_f(RngEngine& eng, Street street, const std::array<Card, 5>& board,
                            std::uint8_t board_count) noexcept {
    const double center = fold_center(street, board, board_count);
    // Exactly one draw, mirroring the prior sampler, so the RNG stream position is
    // unchanged for any draw made after this one.
    const double u = uniform_unit(eng);
    const double jitter = (u * 2.0 - 1.0) * kFoldJitterPp;  // [-jitter, +jitter]
    return std::clamp(center + jitter, kFoldProbabilityMin, kFoldProbabilityMax);
}

}  // namespace poker_trainer::engine
