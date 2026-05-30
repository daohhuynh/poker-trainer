#pragma once

#include "engine/scenario_id.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

// Z01 scenario property model. Defines the position / street / scenario-type
// enums (these live here, not in Phase 0, per ZONES.md Z01), the card model,
// the fully-resolved ScenarioState the engine produces from a Scenario ID, and
// the UserAnswers / GradingResult contract consumed by Z09 (Math Interrogator)
// and Z13 (Post-Round).
//
// The math model these types carry resolves four gaps Module 1 of ARCHITECTURE
// leaves open (escalated and confirmed before implementation):
//   - Outs counted by standard draw rules from hole+board; Equity via the Rule
//     of 2 & 4 (flop outs*4, turn outs*2, capped at 100). No opponent hand.
//   - Pot/bet/stacks built as round-ish accumulated dollars (see generator.hpp).
//   - Caller EV = net call EV (a fourth formula; the three locked V8.1 Aggressor
//     formulas in evaluator.hpp are untouched).
//   - Correct bet-size tier = the max-EV tier through the V8.1 fold function;
//     Overbet = 1.5x pot.
// See generator.hpp / evaluator.hpp for the frozen constants and formulas.

namespace poker_trainer::engine {

// ----- Card model -----

// Suit ordering is fixed and used to build the canonical ordered deck. The
// concrete order is an internal convention; reproducibility depends only on it
// staying constant, which the deck golden tests lock.
enum class Suit : std::uint8_t {
    Clubs = 0,
    Diamonds = 1,
    Hearts = 2,
    Spades = 3,
};

// Lowest and highest card rank. Ranks are stored as small integers so straight
// detection is plain arithmetic: Ten = 10, Jack = 11, Queen = 12, King = 13,
// Ace = 14. The Ace also plays low (value 1) in the wheel A-2-3-4-5, handled
// explicitly in the hand evaluator.
inline constexpr std::uint8_t kMinRank = 2;
inline constexpr std::uint8_t kMaxRank = 14;
inline constexpr std::size_t kSuitCount = 4;
inline constexpr std::size_t kRankCount = 13;
inline constexpr std::size_t kDeckSize = 52;

struct Card {
    std::uint8_t rank{kMinRank};  // 2..14
    Suit suit{Suit::Clubs};

    constexpr bool operator==(const Card&) const noexcept = default;
};

// Stable index in [0, 52) for a card, used by the deck builder and tests.
[[nodiscard]] constexpr std::size_t card_index(Card c) noexcept {
    return static_cast<std::size_t>(c.rank - kMinRank) * kSuitCount
         + static_cast<std::size_t>(c.suit);
}

// ----- Scenario property enums -----

// Seat positions, in standard 6-max action order. Display abbreviations
// (UTG/HJ/CO/BTN/SB/BB) are a rendering concern (Z08); the engine uses the enum.
enum class Position : std::uint8_t {
    UnderTheGun = 0,
    Hijack = 1,
    Cutoff = 2,
    Button = 3,
    SmallBlind = 4,
    BigBlind = 5,
};
inline constexpr std::size_t kPositionCount = 6;

// Betting street. Determines how many community cards are revealed
// (Preflop 0, Flop 3, Turn 4, River 5).
enum class Street : std::uint8_t {
    Preflop = 0,
    Flop = 1,
    Turn = 2,
    River = 3,
};
inline constexpr std::size_t kStreetCount = 4;

// The two top-level scenario types and the three Aggressor sub-types, exactly
// as Module 1's State Branching Matrix enumerates them.
enum class ScenarioType : std::uint8_t {
    Caller = 0,
    AggressorPureBluff = 1,
    AggressorValueBet = 2,
    AggressorSemiBluff = 3,
};

// The four bet-size tiers presented by the Bet Sizing Engine, in visual order
// (matches Module 5's "1=1/3 Pot, 2=1/2 Pot, 3=Full Pot, 4=Overbet").
enum class BetTier : std::uint8_t {
    OneThirdPot = 0,
    HalfPot = 1,
    FullPot = 2,
    Overbet = 3,
};
inline constexpr std::size_t kBetTierCount = 4;

// True for any of the three Aggressor sub-types.
[[nodiscard]] constexpr bool is_aggressor(ScenarioType t) noexcept {
    return t != ScenarioType::Caller;
}

// True when the scenario's grading needs a drawing hand (Outs / Equity): the
// Caller (Pot Odds / Outs / Equity / EV) and the Aggressor Semi-Bluff (Equity
// if Called). These spawn only on Flop or Turn, where a draw is meaningful.
[[nodiscard]] constexpr bool needs_draw(ScenarioType t) noexcept {
    return t == ScenarioType::Caller || t == ScenarioType::AggressorSemiBluff;
}

// ----- Resolved per-tier Aggressor truth -----

// One bet-sizing tier's resolved math. Computed at generation; consumed by Z09
// for grading and Z13 for the recap. Probabilities are fractions in [0, 1].
struct AggressorTier {
    BetTier tier{BetTier::OneThirdPot};
    double bet_fraction{0.0};       // 1/3, 1/2, 1, or 3/2 of pot
    double bet_dollars{0.0};        // bet_fraction * pot (exact; not rounded)
    double fold_probability{0.0};   // P(fold) via the V8.1 fold function
    double call_probability{0.0};   // 1 - fold_probability
    double ev{0.0};                 // EV at this tier via the locked formula

    bool operator==(const AggressorTier&) const noexcept = default;
};

// ----- ScenarioState -----

// The fully-resolved scenario: the seed-locked identity (cards, position, type,
// side-pot, stacks) plus the settings-dependent and computed values (street, F,
// the math truth). generate_scenario() produces this; evaluate() grades against
// the stored truth; Z08 renders from the economics and cards.
struct ScenarioState {
    ScenarioId id{};
    ScenarioType type{ScenarioType::Caller};
    Position position{Position::UnderTheGun};
    Street street{Street::Flop};
    bool side_pot{false};

    // Cards. `hole` is the user's two cards; `board` holds the revealed
    // community cards, `board_count` of which are valid (0 / 3 / 4 / 5).
    std::array<Card, 2> hole{};
    std::array<Card, 5> board{};
    std::uint8_t board_count{0};

    // Economics, in whole dollars. `pot` is chips in the middle BEFORE the
    // current action (Module 1's pot convention). `faced_bet` is the opponent's
    // bet a Caller must call (0 for Aggressor scenarios, where the user bets).
    int small_blind{0};
    int big_blind{0};
    int pot{0};
    int effective_stack{0};
    int faced_bet{0};

    // Opponent baseline fold tendency F (Aggressor only; 0 for Caller). Variable:
    // re-rolled within the current difficulty range at generation, NOT seed-locked.
    double fold_baseline_f{0.0};

    // --- Computed truth: Caller (valid when type == Caller) ---
    double caller_pot_odds_pct{0.0};  // bet / (pot + bet), as 0..100
    int caller_outs{0};
    double caller_equity_pct{0.0};    // Rule of 2 & 4, as 0..100
    double caller_ev{0.0};            // net call EV, dollars

    // --- Computed truth: Aggressor (valid when is_aggressor(type)) ---
    // Equity if called (Semi-Bluff only, bet-size-independent), as 0..100.
    double aggressor_equity_pct{0.0};
    std::array<AggressorTier, kBetTierCount> tiers{};
    // The full 4-tier truth is always computed. `multi_tier` mirrors the Bet
    // Sizing Engine toggle: true -> all four tiers are presented and graded (the
    // multi-tier drill); false -> a single bet size, the seed-drawn
    // `presented_tier`, is shown and graded (the user still picks the optimal
    // size via Bet Size, graded against correct_bet_tier).
    bool multi_tier{true};
    BetTier presented_tier{BetTier::HalfPot};
    BetTier correct_bet_tier{BetTier::HalfPot};  // the max-EV tier

    // Bit-exact equality: a given (id, settings) regenerates an identical state,
    // so default (==) over all fields, including doubles, is the determinism
    // contract the generator tests assert.
    bool operator==(const ScenarioState&) const noexcept = default;
};

// ----- UserAnswers -----

// A submitted answer set. Each field is std::optional so an unfilled box
// (graded incorrect) is distinguishable from a typed value. Percent inputs hold
// the raw number the user typed (30 means 30%, not 0.30). Only the fields
// relevant to the scenario's type are graded; the rest are ignored.
struct UserAnswers {
    // Caller inputs.
    std::optional<double> pot_odds_pct;
    std::optional<int> outs;
    std::optional<double> caller_equity_pct;
    std::optional<double> caller_ev;

    // Aggressor inputs. `equity_if_called_pct` is Semi-Bluff only and answered
    // once (bet-size-independent). `selected_bet_tier` is the user's single pick
    // of the optimal size. `tier_fold_pct` / `tier_ev` hold the per-tier
    // Fold Probability and EV answers, indexed by BetTier; only the first
    // `active_tier_count` entries are graded.
    std::optional<double> equity_if_called_pct;
    std::optional<BetTier> selected_bet_tier;
    std::array<std::optional<double>, kBetTierCount> tier_fold_pct{};
    std::array<std::optional<double>, kBetTierCount> tier_ev{};
};

// ----- GradingResult -----

// Identifies which math input a grade belongs to (matches Module 5's input set).
enum class InputId : std::uint8_t {
    PotOdds = 0,
    Outs = 1,
    Equity = 2,        // Caller Equity or Aggressor Equity-if-Called
    Ev = 3,
    FoldProbability = 4,
    BetSize = 5,
};

// One graded input. `tier_index` is set (0..3) for per-tier Aggressor inputs and
// nullopt otherwise. `correct_value` and `submitted` carry the comparable
// numbers (percent, dollars, integer outs, or the tier ordinal for BetSize) so
// Z13 can render "correct +/- margin" and the user's color-coded answer.
// `submitted` is nullopt when the box was left unfilled.
struct InputGrade {
    InputId input{InputId::PotOdds};
    std::optional<std::uint8_t> tier_index;
    double correct_value{0.0};
    std::optional<double> submitted;
    double margin{0.0};
    bool correct{false};
};

struct GradingResult {
    std::vector<InputGrade> inputs;
    bool all_correct{false};
};

}  // namespace poker_trainer::engine
