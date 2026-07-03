#pragma once

#include "engine/rng_seed.hpp"
#include "engine/scenario.hpp"

#include <array>
#include <cstdint>

// Deterministic Fold Function (Module 1) and the SITUATIONAL sampling of the
// opponent's baseline fold tendency F.
//
// F is no longer drawn from a flat difficulty range. It is computed from the
// situation -- a base fold rate adjusted by street, board texture, and board
// danger -- then given a small residual jitter. The bet-size dependence stays in
// fold_probability(), applied per bet tier, and is the PRIMARY driver of how
// often the opponent folds. The value shown to the player on the HUD is the
// per-tier P(fold) returned by fold_probability(F, bet_fraction).

namespace poker_trainer::engine {

// Clamp bounds on any fold probability (the baseline F and every per-tier P(fold)).
inline constexpr double kFoldProbabilityMin = 0.05;
inline constexpr double kFoldProbabilityMax = 0.95;

// ----- Bet-size term (PRIMARY factor) -----
//
// P(fold) = clamp(F + kBetSizeFoldCoeff * (bet_fraction - kBetSizeFoldCenter), ..).
// Half-pot is the neutral size (no adjustment). The coefficient is chosen so the
// swing across the tier range (1/3 pot .. 3/2 pot overbet) is ~29 percentage
// points, dominating the situational factors below (bet size is the largest
// effect, per the design).
inline constexpr double kBetSizeFoldCoeff = 0.25;
inline constexpr double kBetSizeFoldCenter = 0.5;

// ----- Situational baseline (bet-size-INDEPENDENT) center -----
//
// center = clamp(kBaseFoldRate + street + texture + danger, min, max), where:
//   street : rises toward the river (flop lowest, ~14 pp span).
//   texture: dry boards raise F, wet/coordinated boards lower it (~±13 pp).
//   danger : overcards (A/K/Q) raise F slightly (up to ~+6 pp).
inline constexpr double kBaseFoldRate = 0.45;
inline constexpr double kTextureFoldCoeff = 0.26;  // x (0.5 - wetness) -> ±0.13
inline constexpr double kDangerFoldCoeff = 0.06;   // x danger[0,1]     -> up to +0.06

// Narrow residual jitter around the situational center (±5 pp): the situation
// determines the value, a little randomness keeps it from being a static lookup.
inline constexpr double kFoldJitterPp = 0.05;

// Per-tier P(fold): the situational baseline F shifted by the bet-size term.
// Bigger bets fold the opponent more often; clamped so the opponent is never a
// pure station or a pure folder.
[[nodiscard]] double fold_probability(double f, double bet_fraction) noexcept;

// P(call) = 1 - P(fold).
[[nodiscard]] double call_probability(double f, double bet_fraction) noexcept;

// ----- Situational factor helpers (pure; exposed for tests) -----

// Board wetness in [0, 1]: 0 = bone-dry / disconnected, 1 = very wet (flush and
// straight possibilities). Combines suitedness (shared suits), connectedness
// (small rank gaps), and pairing (a pair dampens draw density). A board with
// fewer than three cards (preflop) is neutral (0.5).
[[nodiscard]] double fold_texture_wetness(const std::array<Card, 5>& board,
                                          std::uint8_t board_count) noexcept;

// Board danger in [0, 1]: overcard scare from the highest card present (Ace 1.0,
// King ~0.55, Queen ~0.25, else 0). An empty board (preflop) is 0.
[[nodiscard]] double fold_board_danger(const std::array<Card, 5>& board,
                                       std::uint8_t board_count) noexcept;

// The clamped situational center (no jitter): the bet-size-independent baseline
// fold tendency for this spot. Exposed so tests can assert the factor directions
// and rough magnitudes without depending on the RNG jitter.
[[nodiscard]] double fold_center(Street street, const std::array<Card, 5>& board,
                                 std::uint8_t board_count) noexcept;

// Sample the situational baseline tendency F for one scenario: the clamped center
// plus a narrow random jitter (±kFoldJitterPp). Consumes exactly one RNG draw so
// the generator's stream position is unchanged from the prior flat-range sampler.
// F is a VARIABLE, computed property -- the same seed reproduces the same board
// and therefore the same F, but F is not itself seed-locked across a board change.
[[nodiscard]] double sample_situational_f(RngEngine& eng, Street street,
                                          const std::array<Card, 5>& board,
                                          std::uint8_t board_count) noexcept;

}  // namespace poker_trainer::engine
