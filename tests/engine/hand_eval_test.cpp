// Hand-evaluation tests: straight/flush detection (incl. the wheel), outs
// counting for the canonical draws and combos, made-hand handling, and the
// Rule-of-2-and-4 equity mapping.

#include "engine/hand_eval.hpp"

#include "engine/scenario.hpp"

#include <array>
#include <vector>

#include <gtest/gtest.h>

namespace pe = poker_trainer::engine;

namespace {

constexpr double kEps = 1e-9;

pe::Card c(std::uint8_t rank, pe::Suit suit) {
    return pe::Card{rank, suit};
}

// Convenience: 0/3/4 board entries plus the count.
struct Flop {
    std::array<pe::Card, 5> board{};
    std::uint8_t count{0};
};

Flop flop(pe::Card a, pe::Card b, pe::Card d) {
    Flop f{};
    f.board[0] = a;
    f.board[1] = b;
    f.board[2] = d;
    f.count = 3;
    return f;
}

Flop turn(pe::Card a, pe::Card b, pe::Card d, pe::Card e) {
    Flop f{};
    f.board[0] = a;
    f.board[1] = b;
    f.board[2] = d;
    f.board[3] = e;
    f.count = 4;
    return f;
}

std::vector<pe::Card> cards(std::initializer_list<pe::Card> cs) {
    return std::vector<pe::Card>(cs);
}

}  // namespace

using pe::Suit;

TEST(HandEval, DetectsFlush) {
    EXPECT_TRUE(pe::has_flush(cards({c(14, Suit::Hearts), c(13, Suit::Hearts),
                                     c(9, Suit::Hearts), c(4, Suit::Hearts),
                                     c(2, Suit::Hearts)})));
    EXPECT_FALSE(pe::has_flush(cards({c(14, Suit::Hearts), c(13, Suit::Hearts),
                                      c(9, Suit::Hearts), c(4, Suit::Hearts),
                                      c(2, Suit::Clubs)})));
}

TEST(HandEval, DetectsStraightIncludingWheel) {
    // Broadway.
    EXPECT_TRUE(pe::has_straight(cards({c(10, Suit::Clubs), c(11, Suit::Diamonds),
                                        c(12, Suit::Hearts), c(13, Suit::Spades),
                                        c(14, Suit::Clubs)})));
    // The wheel A-2-3-4-5.
    EXPECT_TRUE(pe::has_straight(cards({c(14, Suit::Clubs), c(2, Suit::Diamonds),
                                        c(3, Suit::Hearts), c(4, Suit::Spades),
                                        c(5, Suit::Clubs)})));
    // Four to a straight is not a straight.
    EXPECT_FALSE(pe::has_straight(cards({c(9, Suit::Clubs), c(8, Suit::Diamonds),
                                         c(7, Suit::Hearts), c(6, Suit::Spades),
                                         c(2, Suit::Clubs)})));
}

TEST(HandEval, FlushDrawHasNineOuts) {
    const std::array<pe::Card, 2> hole{c(14, Suit::Hearts), c(13, Suit::Hearts)};
    const Flop f = flop(c(12, Suit::Hearts), c(7, Suit::Hearts), c(2, Suit::Clubs));
    EXPECT_EQ(pe::count_draw_outs(hole, f.board, f.count), 9);
}

TEST(HandEval, OpenEndedStraightDrawHasEightOuts) {
    const std::array<pe::Card, 2> hole{c(9, Suit::Clubs), c(8, Suit::Hearts)};
    const Flop f = flop(c(7, Suit::Spades), c(6, Suit::Diamonds), c(2, Suit::Clubs));
    EXPECT_EQ(pe::count_draw_outs(hole, f.board, f.count), 8);
}

TEST(HandEval, GutshotHasFourOuts) {
    const std::array<pe::Card, 2> hole{c(9, Suit::Clubs), c(7, Suit::Hearts)};
    const Flop f = flop(c(6, Suit::Spades), c(5, Suit::Diamonds), c(2, Suit::Clubs));
    EXPECT_EQ(pe::count_draw_outs(hole, f.board, f.count), 4);
}

TEST(HandEval, ComboFlushPlusGutshotCountsUnionOnce) {
    // Flush draw (4 hearts: A K Q 2) + broadway gutshot needing a Ten.
    // 9 hearts + 4 tens, minus the Ten of hearts counted in both = 12 outs.
    const std::array<pe::Card, 2> hole{c(14, Suit::Hearts), c(13, Suit::Hearts)};
    const Flop f = flop(c(12, Suit::Hearts), c(11, Suit::Clubs), c(2, Suit::Hearts));
    EXPECT_EQ(pe::count_draw_outs(hole, f.board, f.count), 12);
}

TEST(HandEval, MadeStraightFlushHasNoDrawOuts) {
    const std::array<pe::Card, 2> hole{c(14, Suit::Hearts), c(13, Suit::Hearts)};
    const Flop f = flop(c(12, Suit::Hearts), c(11, Suit::Hearts), c(10, Suit::Hearts));
    EXPECT_EQ(pe::count_draw_outs(hole, f.board, f.count), 0);
}

TEST(HandEval, OutsAreSameCountOnFlopAndTurn) {
    // Flush draw still four-to-a-flush after a non-heart turn card: nine outs.
    const std::array<pe::Card, 2> hole{c(14, Suit::Hearts), c(13, Suit::Hearts)};
    const Flop t = turn(c(12, Suit::Hearts), c(7, Suit::Hearts), c(2, Suit::Clubs),
                        c(3, Suit::Spades));
    EXPECT_EQ(pe::count_draw_outs(hole, t.board, t.count), 9);
}

TEST(HandEval, RuleOfTwoAndFour) {
    EXPECT_NEAR(pe::equity_from_outs(9, pe::Street::Flop), 36.0, kEps);
    EXPECT_NEAR(pe::equity_from_outs(9, pe::Street::Turn), 18.0, kEps);
    EXPECT_NEAR(pe::equity_from_outs(8, pe::Street::Flop), 32.0, kEps);
    EXPECT_NEAR(pe::equity_from_outs(4, pe::Street::Turn), 8.0, kEps);
    // Capped at 100 for monster combo draws.
    EXPECT_NEAR(pe::equity_from_outs(30, pe::Street::Flop), 100.0, kEps);
    // No cards to come on the river / no board pre-flop -> zero.
    EXPECT_NEAR(pe::equity_from_outs(9, pe::Street::River), 0.0, kEps);
    EXPECT_NEAR(pe::equity_from_outs(9, pe::Street::Preflop), 0.0, kEps);
}
