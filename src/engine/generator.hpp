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

// Fixed blind base. Blind value is not a persisted setting (removed in S19); the
// engine sets it internally. Stacks and pots scale off the big blind.
inline constexpr int kSmallBlind = 1;
inline constexpr int kBigBlind = 2;

// Bet-size tiers as pot fractions, in Module 5's visual order
// (1/3, 1/2, Full, Overbet). Overbet = 1.5x pot.
inline constexpr std::array<double, kBetTierCount> kBetTierFractions{
    1.0 / 3.0, 0.5, 1.0, 1.5};
inline constexpr double kOverbetFraction = 1.5;

// Pots are drawn as round-ish WHOLE dollars across this range (e.g. $63, $87 are
// expected; they are deliberately not snapped to clean $5/$10 multiples, since
// rounding a messy pot mentally is part of the drilled skill).
inline constexpr int kPotMinDollars = 40;
inline constexpr int kPotMaxDollars = 240;

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
