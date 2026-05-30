#pragma once

#include "engine/scenario.hpp"

#include <span>

// Draw recognition, outs counting, and Rule-of-2-and-4 equity for the True
// Evaluator. The model (escalated and confirmed): no opponent hand is dealt;
// "outs" are the unseen cards that complete a straight or flush the hero is
// drawing to, and "equity" is those outs run through the Rule of 2 and 4.
//
// There is deliberately no full showdown hand-ranker: the math model never
// compares the hero against a specific opponent hand, so only the boolean
// "is a straight/flush present" predicates and the outs count are needed.

namespace poker_trainer::engine {

// True if some five of `cards` form a straight (Ace plays high or low, so
// A-2-3-4-5 and T-J-Q-K-A both count). Accepts 5..7 cards.
[[nodiscard]] bool has_straight(std::span<const Card> cards) noexcept;

// True if at least five of `cards` share a suit.
[[nodiscard]] bool has_flush(std::span<const Card> cards) noexcept;

// Count the hero's draw outs: unseen cards (the 52 minus hole minus revealed
// board) that COMPLETE a straight or flush the hero does not already hold. A
// card that completes both is counted once. When the hero already has a made
// straight or flush, that category contributes no outs (the generator keeps
// drawing scenarios off made hands; see generator.cpp). `board_count` is the
// number of valid entries in `board` (3 on the flop, 4 on the turn).
[[nodiscard]] int count_draw_outs(const std::array<Card, 2>& hole,
                                  const std::array<Card, 5>& board,
                                  std::uint8_t board_count) noexcept;

// Rule of 2 and 4: equity percent = outs * 4 on the flop (two cards to come),
// outs * 2 on the turn (one card to come), capped at 100. Streets with no cards
// to come (Pre-flop has no board, River has no future card) yield 0 — the
// generator never spawns drawing scenarios on those streets.
[[nodiscard]] double equity_from_outs(int outs, Street street) noexcept;

}  // namespace poker_trainer::engine
