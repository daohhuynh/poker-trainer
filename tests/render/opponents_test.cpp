// Zone 08 — position-label assignment + scenario-type -> visual-state unit tests.
// Pure logic only (no live ImGui frame).

#include "render/opponents.hpp"

#include "engine/scenario.hpp"

#include <cstdint>
#include <string_view>

#include <gtest/gtest.h>

namespace rnd = poker_trainer::render;
namespace eng = poker_trainer::engine;

TEST(PositionAbbrev, AllSixSeats) {
    EXPECT_EQ(std::string_view{rnd::position_abbrev(eng::Position::UnderTheGun)}, "UTG");
    EXPECT_EQ(std::string_view{rnd::position_abbrev(eng::Position::Hijack)}, "HJ");
    EXPECT_EQ(std::string_view{rnd::position_abbrev(eng::Position::Cutoff)}, "CO");
    EXPECT_EQ(std::string_view{rnd::position_abbrev(eng::Position::Button)}, "BTN");
    EXPECT_EQ(std::string_view{rnd::position_abbrev(eng::Position::SmallBlind)}, "SB");
    EXPECT_EQ(std::string_view{rnd::position_abbrev(eng::Position::BigBlind)}, "BB");
}

TEST(OpponentDisplayStack, DeterministicReproducibleAndInRange) {
    eng::ScenarioState s{};
    s.id = eng::ScenarioId{4729183746281ull};
    s.big_blind = 100;
    for (std::uint8_t i = 0; i < eng::kPositionCount; ++i) {
        const auto seat = static_cast<eng::Position>(i);
        const int a = rnd::opponent_display_stack(s, seat);
        const int b = rnd::opponent_display_stack(s, seat);
        EXPECT_EQ(a, b);                      // same (id, seat) -> identical
        EXPECT_GE(a, 15 * s.big_blind);       // >= 15 BB
        EXPECT_LE(a, 300 * s.big_blind);      // <= 300 BB
        EXPECT_EQ(a % (5 * s.big_blind), 0);  // round 5-BB multiple
    }
}

TEST(OpponentDisplayStack, SeatsAreNotAllIdentical) {
    // The whole point of the fix: opponents no longer share one stack.
    eng::ScenarioState s{};
    s.id = eng::ScenarioId{42};
    s.big_blind = 50;
    int distinct = 0;
    const int first = rnd::opponent_display_stack(s, eng::Position::UnderTheGun);
    for (std::uint8_t i = 0; i < eng::kPositionCount; ++i) {
        if (rnd::opponent_display_stack(s, static_cast<eng::Position>(i)) != first) {
            ++distinct;
        }
    }
    EXPECT_GT(distinct, 0);  // at least one seat differs from UTG
}

TEST(OpponentChipState, CallerPushesForward) {
    EXPECT_EQ(rnd::opponent_chip_state(eng::ScenarioType::Caller),
              rnd::OpponentChipState::PushedForward);
}

TEST(OpponentChipState, EveryAggressorSubTypeIsEmpty) {
    EXPECT_EQ(rnd::opponent_chip_state(eng::ScenarioType::AggressorPureBluff),
              rnd::OpponentChipState::Empty);
    EXPECT_EQ(rnd::opponent_chip_state(eng::ScenarioType::AggressorValueBet),
              rnd::OpponentChipState::Empty);
    EXPECT_EQ(rnd::opponent_chip_state(eng::ScenarioType::AggressorSemiBluff),
              rnd::OpponentChipState::Empty);
}
