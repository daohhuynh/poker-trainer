// Deterministic Fold Function tests: the per-tier bet-size curve (primary factor),
// its clamp bounds, the P(call) complement, and the SITUATIONAL baseline F model
// (street / board texture / overcard danger + a narrow jitter).

#include "engine/fold_function.hpp"

#include "engine/rng_seed.hpp"
#include "engine/scenario.hpp"
#include "engine/scenario_id.hpp"

#include <array>
#include <cmath>

#include <gtest/gtest.h>

namespace pe = poker_trainer::engine;

namespace {
constexpr double kEps = 1e-9;

pe::Card card(std::uint8_t rank, pe::Suit suit) noexcept { return pe::Card{rank, suit}; }

// A dry, disconnected, rainbow low board (no flush/straight density, no overcard).
std::array<pe::Card, 5> dry_board() noexcept {
    return {card(2, pe::Suit::Clubs), card(6, pe::Suit::Diamonds), card(10, pe::Suit::Hearts),
            pe::Card{}, pe::Card{}};
}

// A wet, connected, monotone board (three to a straight and a flush).
std::array<pe::Card, 5> wet_board() noexcept {
    return {card(8, pe::Suit::Hearts), card(9, pe::Suit::Hearts), card(10, pe::Suit::Hearts),
            pe::Card{}, pe::Card{}};
}
}  // namespace

// ----- Bet-size term: the PRIMARY factor -----

TEST(FoldFunction, HalfPotBetEqualsBaseline) {
    // bet_fraction = 0.5 zeroes the size term, so P(fold) == F exactly.
    EXPECT_NEAR(pe::fold_probability(0.50, 0.5), 0.50, kEps);
    EXPECT_NEAR(pe::fold_probability(0.20, 0.5), 0.20, kEps);
    EXPECT_NEAR(pe::fold_probability(0.80, 0.5), 0.80, kEps);
}

TEST(FoldFunction, BiggerBetsFoldMoreOften) {
    // F + kBetSizeFoldCoeff * (bet_fraction - 0.5), monotone increasing in bet_fraction.
    EXPECT_NEAR(pe::fold_probability(0.5, 1.0 / 3.0),
                0.5 + pe::kBetSizeFoldCoeff * (1.0 / 3.0 - 0.5), kEps);
    EXPECT_NEAR(pe::fold_probability(0.5, 1.0), 0.5 + pe::kBetSizeFoldCoeff * 0.5, kEps);
    EXPECT_NEAR(pe::fold_probability(0.5, 1.5), 0.5 + pe::kBetSizeFoldCoeff * 1.0, kEps);
    EXPECT_LT(pe::fold_probability(0.5, 1.0 / 3.0), pe::fold_probability(0.5, 0.5));
    EXPECT_LT(pe::fold_probability(0.5, 0.5), pe::fold_probability(0.5, 1.0));
    EXPECT_LT(pe::fold_probability(0.5, 1.0), pe::fold_probability(0.5, 1.5));
}

TEST(FoldFunction, BetSizeSwingIsTheDominantEffect) {
    // The swing across the tier range (1/3 pot -> overbet) is large (>= 20 pp), the
    // biggest single driver of the opponent's fold frequency.
    const double swing = pe::fold_probability(0.5, 1.5) - pe::fold_probability(0.5, 1.0 / 3.0);
    EXPECT_GE(swing, 0.20);
}

TEST(FoldFunction, ClampsToBounds) {
    // A very loose baseline and a tiny bet still folds at least the floor.
    EXPECT_NEAR(pe::fold_probability(0.0, 1.0 / 3.0), pe::kFoldProbabilityMin, kEps);
    // A very tight baseline facing a big overbet never exceeds the ceiling.
    EXPECT_NEAR(pe::fold_probability(1.0, 2.0), pe::kFoldProbabilityMax, kEps);
}

TEST(FoldFunction, CallIsComplementOfFold) {
    for (const double frac : {1.0 / 3.0, 0.5, 1.0, 1.5}) {
        EXPECT_NEAR(pe::fold_probability(0.45, frac) + pe::call_probability(0.45, frac), 1.0, kEps);
    }
}

// ----- Situational factors: direction and rough magnitude -----

TEST(FoldFunction, DryBoardsRaiseFoldMoreThanWetBoards) {
    // Dry/disconnected boards fold more (hands connect or fold); wet/coordinated
    // boards fold less (draws and continues). Same street isolates the texture factor.
    const double dry = pe::fold_center(pe::Street::Flop, dry_board(), 3);
    const double wet = pe::fold_center(pe::Street::Flop, wet_board(), 3);
    EXPECT_GT(dry, wet);
    // Rough magnitude: the texture swing is on the order of 10-30 pp.
    EXPECT_GE(dry - wet, 0.10);
}

TEST(FoldFunction, WetnessHeuristicOrdersBoards) {
    EXPECT_GT(pe::fold_texture_wetness(wet_board(), 3), pe::fold_texture_wetness(dry_board(), 3));
    // Preflop (no board) is neutral.
    EXPECT_NEAR(pe::fold_texture_wetness(std::array<pe::Card, 5>{}, 0), 0.5, kEps);
}

TEST(FoldFunction, LaterStreetsRaiseFold) {
    // Rising toward the river; the flop is the low point. Same board isolates street.
    const auto b = dry_board();
    const double flop = pe::fold_center(pe::Street::Flop, b, 3);
    const double turn = pe::fold_center(pe::Street::Turn, b, 3);
    const double river = pe::fold_center(pe::Street::River, b, 3);
    EXPECT_LT(flop, turn);
    EXPECT_LT(turn, river);
    // Small-to-moderate: the flop->river street swing is a handful of pp, not dominant.
    EXPECT_GE(river - flop, 0.05);
    EXPECT_LE(river - flop, 0.20);
}

TEST(FoldFunction, OvercardsRaiseDanger) {
    const std::array<pe::Card, 5> ace_board{card(14, pe::Suit::Clubs), card(7, pe::Suit::Diamonds),
                                            card(2, pe::Suit::Hearts), pe::Card{}, pe::Card{}};
    const std::array<pe::Card, 5> king_board{card(13, pe::Suit::Clubs), card(7, pe::Suit::Diamonds),
                                             card(2, pe::Suit::Hearts), pe::Card{}, pe::Card{}};
    const std::array<pe::Card, 5> low_board{card(9, pe::Suit::Clubs), card(7, pe::Suit::Diamonds),
                                            card(2, pe::Suit::Hearts), pe::Card{}, pe::Card{}};
    EXPECT_GT(pe::fold_board_danger(ace_board, 3), pe::fold_board_danger(king_board, 3));
    EXPECT_GT(pe::fold_board_danger(king_board, 3), pe::fold_board_danger(low_board, 3));
    EXPECT_NEAR(pe::fold_board_danger(low_board, 3), 0.0, kEps);
    // Same texture, an ace present raises the situational center (small effect).
    const double with_ace = pe::fold_center(pe::Street::Flop, ace_board, 3);
    const double without = pe::fold_center(pe::Street::Flop, low_board, 3);
    EXPECT_GT(with_ace, without);
    EXPECT_LE(with_ace - without, 0.12);
}

// ----- Sampling: bounds, determinism, situational center -----

TEST(FoldFunction, SampledFStaysInBounds) {
    for (std::uint64_t id = 1; id <= 2000; ++id) {
        pe::RngSeed seed{pe::ScenarioId{id}};
        const double f = pe::sample_situational_f(seed.engine(), pe::Street::Turn, dry_board(), 3);
        EXPECT_GE(f, pe::kFoldProbabilityMin);
        EXPECT_LE(f, pe::kFoldProbabilityMax);
    }
}

TEST(FoldFunction, SampledFIsDeterministic) {
    pe::RngSeed a{pe::ScenarioId{12345}};
    pe::RngSeed b{pe::ScenarioId{12345}};
    EXPECT_EQ(pe::sample_situational_f(a.engine(), pe::Street::Flop, wet_board(), 3),
              pe::sample_situational_f(b.engine(), pe::Street::Flop, wet_board(), 3));
}

TEST(FoldFunction, SampledFStaysNearTheSituationalCenter) {
    // The situation dominates: the sampled F is within the narrow jitter band of the
    // computed center for every seed.
    const auto b = dry_board();
    const double center = pe::fold_center(pe::Street::River, b, 3);
    for (std::uint64_t id = 1; id <= 500; ++id) {
        pe::RngSeed seed{pe::ScenarioId{id}};
        const double f = pe::sample_situational_f(seed.engine(), pe::Street::River, b, 3);
        EXPECT_LE(std::abs(f - center), pe::kFoldJitterPp + kEps);
    }
}
