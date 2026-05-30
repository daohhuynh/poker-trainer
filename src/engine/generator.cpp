#include "engine/generator.hpp"

#include "engine/deck.hpp"
#include "engine/evaluator.hpp"
#include "engine/fold_function.hpp"
#include "engine/hand_eval.hpp"
#include "engine/rng_seed.hpp"
#include "engine/rng_util.hpp"
#include "engine/side_pot.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace poker_trainer::engine {

namespace {

// ----- Draw order (frozen). Every draw flows through rng_util so the result is
// identical on every platform. The order puts the seed-locked structural draws
// (type, position, cards, stacks, pot) before the settings-dependent ones so
// that changing the difficulty range / side-pot frequency does not shift the
// structural part of the stream; only the street weights (the one seed-encoded
// gameplay setting) influence structure, via the street draw. -----

ScenarioType draw_type(RngEngine& eng) noexcept {
    // 50% Caller / 50% Aggressor; the Aggressor splits uniformly into its three
    // sub-types. Mode (STANDARD / Aggressor / Caller / Custom) is applied
    // upstream by selecting a seed of the desired type, so the type here is a
    // pure function of the seed (shared-URL scenarios reconstruct the same type).
    if (uniform_uint(eng, 2) == 0) {
        return ScenarioType::Caller;
    }
    switch (uniform_uint(eng, 3)) {
        case 0:
            return ScenarioType::AggressorPureBluff;
        case 1:
            return ScenarioType::AggressorValueBet;
        default:
            return ScenarioType::AggressorSemiBluff;
    }
}

Position draw_position(RngEngine& eng) noexcept {
    return static_cast<Position>(static_cast<std::uint8_t>(uniform_uint(eng, kPositionCount)));
}

std::uint8_t board_count_for(Street street) noexcept {
    switch (street) {
        case Street::Preflop:
            return 0;
        case Street::Flop:
            return 3;
        case Street::Turn:
            return 4;
        case Street::River:
            return 5;
    }
    return 0;
}

Street draw_street(RngEngine& eng, ScenarioType type, const settings::GameplaySettings& g) noexcept {
    // Drawing scenarios (Caller, Semi-Bluff) spawn only on Flop or Turn, where a
    // draw and the Rule of 2 & 4 are meaningful; other types span all four
    // streets. Exactly one uniform draw is consumed regardless of the outcome,
    // so a change to the street weights cannot shift later draws.
    std::array<Street, kStreetCount> allowed{};
    std::array<double, kStreetCount> weights{};
    std::size_t count = 0;
    const auto add = [&](Street s, std::uint8_t w) {
        allowed[count] = s;
        weights[count] = static_cast<double>(w);
        ++count;
    };
    if (needs_draw(type)) {
        add(Street::Flop, g.street_weight_flop);
        add(Street::Turn, g.street_weight_turn);
    } else {
        add(Street::Preflop, g.street_weight_preflop);
        add(Street::Flop, g.street_weight_flop);
        add(Street::Turn, g.street_weight_turn);
        add(Street::River, g.street_weight_river);
    }

    double total = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        total += weights[i];
    }
    const double u = uniform_unit(eng);
    if (total <= 0.0) {
        const auto idx = static_cast<std::size_t>(u * static_cast<double>(count));
        return allowed[std::min(idx, count - 1)];
    }
    const double target = u * total;
    double acc = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        acc += weights[i];
        if (target < acc) {
            return allowed[i];
        }
    }
    return allowed[count - 1];
}

// Whether the hero currently holds a made straight or flush over the known cards.
bool is_made(const std::array<Card, 2>& hole,
             const std::array<Card, 5>& board,
             std::uint8_t board_count) noexcept {
    std::array<Card, 7> known{};
    std::size_t n = 0;
    known[n++] = hole[0];
    known[n++] = hole[1];
    for (std::size_t i = 0; i < static_cast<std::size_t>(board_count); ++i) {
        known[n++] = board[i];
    }
    const std::span<const Card> ks(known.data(), n);
    return has_straight(ks) || has_flush(ks);
}

void draw_cards(RngEngine& eng, ScenarioState& s) noexcept {
    const std::uint8_t bc = board_count_for(s.street);
    s.board_count = bc;

    if (!needs_draw(s.type)) {
        Deck deck = make_ordered_deck();
        shuffle_deck(deck, eng);
        s.hole[0] = deck[0];
        s.hole[1] = deck[1];
        for (std::size_t i = 0; i < static_cast<std::size_t>(bc); ++i) {
            s.board[i] = deck[2 + i];
        }
        return;
    }

    // Drawing scenario: reshuffle until the hero is on a clean draw (no made
    // straight/flush, at least one out) at the chosen street. Best-effort
    // fallback (most outs) guarantees termination; clean draws are common, so
    // the cap is effectively never reached (golden-tested on default settings).
    int best_outs = -1;
    std::array<Card, 2> best_hole{};
    std::array<Card, 5> best_board{};
    for (int attempt = 0; attempt < kMaxRejectionAttempts; ++attempt) {
        Deck deck = make_ordered_deck();
        shuffle_deck(deck, eng);
        std::array<Card, 2> hole{deck[0], deck[1]};
        std::array<Card, 5> board{};
        for (std::size_t i = 0; i < static_cast<std::size_t>(bc); ++i) {
            board[i] = deck[2 + i];
        }
        const int outs = count_draw_outs(hole, board, bc);
        if (!is_made(hole, board, bc) && outs > 0) {
            s.hole = hole;
            s.board = board;
            return;
        }
        if (outs > best_outs) {
            best_outs = outs;
            best_hole = hole;
            best_board = board;
        }
    }
    s.hole = best_hole;
    s.board = best_board;
}

int draw_stack(RngEngine& eng) noexcept {
    const int steps = (kStackMaxBb - kStackMinBb) / kStackStepBb + 1;
    const int n_bb =
        kStackMinBb + kStackStepBb * static_cast<int>(uniform_uint(eng, static_cast<std::uint64_t>(steps)));
    return n_bb * kBigBlind;
}

int draw_pot(RngEngine& eng) noexcept {
    return kPotMinDollars
         + static_cast<int>(uniform_uint(eng, static_cast<std::uint64_t>(kPotMaxDollars - kPotMinDollars + 1)));
}

// Resolve the Caller's faced bet (pot already set) plus the full Caller truth.
void resolve_caller_bet(RngEngine& eng, ScenarioState& s) noexcept {
    s.caller_outs = count_draw_outs(s.hole, s.board, s.board_count);
    s.caller_equity_pct = equity_from_outs(s.caller_outs, s.street);
    const double equity_frac = s.caller_equity_pct / 100.0;
    const double pot_d = static_cast<double>(s.pot);

    int best_bet = 1;
    double best_score = -1e18;
    double best_pot_odds = 0.0;

    for (int attempt = 0; attempt < kMaxRejectionAttempts; ++attempt) {
        const double frac =
            kCallerBetFractionMin + uniform_unit(eng) * (kCallerBetFractionMax - kCallerBetFractionMin);
        int bet = static_cast<int>(std::lround(frac * pot_d));
        if (bet < 1) {
            bet = 1;
        }
        const double pot_odds_pct = 100.0 * pot_odds_fraction(pot_d, static_cast<double>(bet));
        const double gap = std::abs(s.caller_equity_pct - pot_odds_pct);
        const bool in_band = pot_odds_pct >= kPotOddsMinPct && pot_odds_pct <= kPotOddsMaxPct;

        if (in_band && gap >= kCallerDecisionMarginPct) {
            s.faced_bet = bet;
            s.caller_pot_odds_pct = pot_odds_pct;
            s.caller_ev = net_call_ev(equity_frac, pot_d, static_cast<double>(bet));
            return;
        }
        // Best-effort: prefer in-band candidates, then the largest decision gap.
        const double score = in_band ? gap : (gap - 1000.0);
        if (score > best_score) {
            best_score = score;
            best_bet = bet;
            best_pot_odds = pot_odds_pct;
        }
    }

    s.faced_bet = best_bet;
    s.caller_pot_odds_pct = best_pot_odds;
    s.caller_ev = net_call_ev(equity_frac, pot_d, static_cast<double>(best_bet));
}

double tier_ev(ScenarioType type, double p_fold, double p_call, double equity_frac,
               double pot, double bet) noexcept {
    switch (type) {
        case ScenarioType::AggressorPureBluff:
            return pure_bluff_ev(p_fold, pot, bet);
        case ScenarioType::AggressorValueBet:
            return value_bet_ev(p_call, bet);
        case ScenarioType::AggressorSemiBluff:
            return semi_bluff_ev(p_fold, equity_frac, pot, bet);
        default:
            return 0.0;
    }
}

// Build the four-tier truth for a given F, returning the table plus the max-EV
// tier index (the reference "correct" tier; ties -> smaller index).
struct TierTable {
    std::array<AggressorTier, kBetTierCount> tiers{};
    std::uint8_t best_index{0};
};

TierTable build_tiers(ScenarioType type, double f, double equity_frac, double pot) noexcept {
    TierTable table{};
    for (std::uint8_t t = 0; t < kBetTierCount; ++t) {
        const double frac = kBetTierFractions[t];
        const double p_fold = fold_probability(f, frac);
        const double p_call = 1.0 - p_fold;
        const double bet = frac * pot;
        const double ev = tier_ev(type, p_fold, p_call, equity_frac, pot, bet);
        table.tiers[t] = AggressorTier{static_cast<BetTier>(t), frac, bet, p_fold, p_call, ev};
    }
    std::uint8_t best = 0;
    for (std::uint8_t t = 1; t < kBetTierCount; ++t) {
        if (table.tiers[t].ev > table.tiers[best].ev) {
            best = t;
        }
    }
    table.best_index = best;
    return table;
}

// Resolve F (within the difficulty range) and the Aggressor tier truth (pot
// already set). Rejects F until the max-EV tier beats the runner-up by a clear
// margin; best-effort fallback by largest margin.
void resolve_aggressor(RngEngine& eng, ScenarioState& s, const settings::GameplaySettings& g) noexcept {
    double equity_frac = 0.0;
    if (s.type == ScenarioType::AggressorSemiBluff) {
        const int outs = count_draw_outs(s.hole, s.board, s.board_count);
        s.aggressor_equity_pct = equity_from_outs(outs, s.street);
        equity_frac = s.aggressor_equity_pct / 100.0;
    }
    const double pot_d = static_cast<double>(s.pot);

    // One F draw from the difficulty range — varied per scenario so the correct
    // tier and P(fold) are not memorizable. No separation-based rejection: tolerant
    // bet-size grading accepts any tier statistically tied with the max-EV tier,
    // so a near-tie simply yields more than one accepted tier (correct behavior),
    // not something to exclude. The correct (reference) tier is the max-EV tier.
    const double f = sample_fold_baseline(eng, static_cast<double>(g.difficulty_min),
                                          static_cast<double>(g.difficulty_max));
    const TierTable table = build_tiers(s.type, f, equity_frac, pot_d);

    s.fold_baseline_f = f;
    s.tiers = table.tiers;
    s.correct_bet_tier = static_cast<BetTier>(table.best_index);

    // The single presented tier (used only when the Bet Sizing Engine is off) is
    // drawn regardless of the toggle, so the toggle changes presentation only,
    // never the RNG stream or any computed truth.
    const auto drawn_tier = static_cast<BetTier>(static_cast<std::uint8_t>(uniform_uint(eng, kBetTierCount)));
    s.multi_tier = g.bet_sizing_engine_enabled;
    s.presented_tier = s.multi_tier ? s.correct_bet_tier : drawn_tier;
}

}  // namespace

ScenarioState generate_scenario(ScenarioId id, const settings::Settings& settings) {
    RngSeed seed(id);
    RngEngine& eng = seed.engine();
    const settings::GameplaySettings& g = settings.gameplay;

    ScenarioState s{};
    s.id = id;
    s.small_blind = kSmallBlind;
    s.big_blind = kBigBlind;

    s.type = draw_type(eng);              // 1. type (seed-locked)
    s.position = draw_position(eng);      // 2. position (seed-locked)
    s.street = draw_street(eng, s.type, g);  // 3. street (street weights)
    draw_cards(eng, s);                   // 4. cards (seed-locked; draw-rejected)
    s.effective_stack = draw_stack(eng);  // 5. stacks (seed-locked)
    s.pot = draw_pot(eng);                // 6a. pot (seed-locked)

    if (s.type == ScenarioType::Caller) {
        resolve_caller_bet(eng, s);       // 6b. Caller faced bet + truth
    }
    s.side_pot = roll_side_pot(eng, g.side_pot_frequency);  // 7. side-pot status
    if (is_aggressor(s.type)) {
        resolve_aggressor(eng, s, g);     // 8. F + Aggressor tier truth
    }
    return s;
}

}  // namespace poker_trainer::engine
