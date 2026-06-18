#pragma once

#include "engine/scenario.hpp"

#include "settings/settings.hpp"

#include <array>

// Scenario Generator (Module 1): the deterministic mapping from a Scenario ID
// (plus the seed-encoded street weights and the play-time difficulty range) to a
// fully-resolved ScenarioState. Same (id, settings) reconstructs the identical
// scenario on every platform.
//
// The constants below are FROZEN and locked by the generator golden tests.
// Changing any of them changes generated scenarios and would, in production,
// require a scenario-format version bump (CLAUDE.md sec.5). They are not part of
// ARCHITECTURE's locked formulas; they are the generation parameters the owner
// authorized (round-ish pots, the pot-fraction spread, stack range, blinds, and
// the two golden-tested decision-margin invariants).

namespace poker_trainer::engine {

// Blind levels. Module 1's Randomizer draws the blinds (with street + position)
// from the seeded RNG; the engine no longer fixes them at 1/2. One level is drawn
// per scenario from this FROZEN ladder, and stacks / pots scale off the chosen
// big blind. The ten levels span the four stake bands the stake-scaled chip tier
// table keys on (render/chips.cpp::active_stake_tier: bb <= 5 Micro, <= 50 Mid,
// <= 500 High, else Nosebleed). The fractional 0.5/1 micro level is omitted: the
// blinds are whole-dollar ints (scenario.hpp), and a half-dollar small blind would
// force a float field. Changing this ladder changes generated economics and is a
// scenario-format-version-class change (CLAUDE.md sec.5), locked by the generator
// golden tests.
struct BlindLevel {
    int small_blind{0};
    int big_blind{0};
};

inline constexpr std::array<BlindLevel, 10> kBlindLevels{{
    BlindLevel{1, 2},     BlindLevel{2, 5},                          // Micro     (bb <= 5)
    BlindLevel{5, 10},    BlindLevel{10, 25},   BlindLevel{25, 50},  // Mid       (bb <= 50)
    BlindLevel{50, 100},  BlindLevel{100, 200}, BlindLevel{200, 500},// High      (bb <= 500)
    BlindLevel{500, 1000},BlindLevel{1000, 2000},                    // Nosebleed (bb > 500)
}};

// Bet-size tiers as pot fractions, in Module 5's visual order
// (1/3, 1/2, Full, Overbet). Overbet = 1.5x pot.
inline constexpr std::array<double, kBetTierCount> kBetTierFractions{
    1.0 / 3.0, 0.5, 1.0, 1.5};
inline constexpr double kOverbetFraction = 1.5;

// Pots are drawn as a round-ish WHOLE number of BIG BLINDS across this range, then
// scaled by the drawn big blind (pot = pot_bb * big_blind). At the {1,2} micro
// level this reproduces the prior $40-$240 dollar range (20-120 BB); at higher
// levels the pot scales with the stake. The BB multiples are deliberately not
// snapped to clean fives, since rounding a messy pot mentally is part of the
// drilled skill.
inline constexpr int kPotMinBb = 20;
inline constexpr int kPotMaxBb = 120;

// The Caller's faced bet is a round dollar amount approximating a pot fraction
// drawn from this spread; the resulting pot odds are accepted only inside the
// band below, so the break-even equity to call lands in roughly 20-40%.
inline constexpr double kCallerBetFractionMin = 0.28;
inline constexpr double kCallerBetFractionMax = 0.90;
inline constexpr double kPotOddsMinPct = 20.0;
inline constexpr double kPotOddsMaxPct = 44.0;

// Decision margin (Caller): |equity - pot odds| must clear this. Comfortably
// outside the +/-5pp grading band so mental rounding can't flip the call/fold.
inline constexpr double kCallerDecisionMarginPct = 8.0;

// Effective stacks: round big-blind multiples, short to deep (implied-odds
// reasoning is a real factor; stacks do not enter the graded math).
inline constexpr int kStackMinBb = 20;
inline constexpr int kStackMaxBb = 200;
inline constexpr int kStackStepBb = 5;

// Aggressor bet tiers carry no separation invariant: bet-size grading is
// tolerant (evaluator.cpp accepts any tier whose EV is within the EV grading
// tolerance of the max-EV tier), so near-ties are correct behavior — they yield
// more than one accepted tier rather than something to exclude. F is sampled
// once per scenario from the difficulty range; the correct (reference) tier is
// simply the max-EV tier. The call/fold decision margin (Caller, above) is the
// only decision-margin invariant retained.

// Deterministic rejection-sampling cap. If no candidate clears an invariant in
// this many tries, the best candidate seen is used so generation always
// terminates. Far above what realistic settings need; default settings clear
// every invariant within a handful of tries (asserted by the golden tests).
inline constexpr int kMaxRejectionAttempts = 1024;

// Reconstruct the scenario identified by `id` under the given settings. The
// seed fixes the locked identity (cards, position, type, side-pot, stacks); the
// settings supply the seed-encoded street weights, the difficulty range for F,
// the side-pot frequency, and the Bet Sizing Engine toggle.
[[nodiscard]] ScenarioState generate_scenario(ScenarioId id, const settings::Settings& settings);

// Cheaply resolve only the seed-locked scenario type for an id, without running
// full generation. Returns exactly the type generate_scenario(id, ...) would
// produce (type is a pure function of the seed). Z05's main loop uses this to
// pick a fresh id matching the launched mode: STANDARD accepts any id, AGGRESSOR
// keeps drawing until is_aggressor(peek_type(id)), CALLER until type == Caller,
// CUSTOM until the type honors the Aggressor/Caller split. The reject loop lives
// in Z05; the engine only exposes this peek.
[[nodiscard]] ScenarioType peek_type(ScenarioId id) noexcept;

}  // namespace poker_trainer::engine
