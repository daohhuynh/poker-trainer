#pragma once

// Zone 12 — pure, ImGui-free settings logic shared by the section renderers and the
// unit tests. Everything here is value-semantic and deterministic: the coupled
// street-split redistribution, the two-handle difficulty-range clamp + display
// conversions, the integer parse/clamp/step helpers behind the volume and custom-time
// inputs, and the slider ghost-default fraction. The render TUs (sections/*.cpp) call
// these; the .cpp glue is the only part that touches ImGui (mirrors custom_popup.hpp).

#include "settings/settings.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace poker_trainer::settings {

// ----- generic integer helpers (volume / custom-time inputs) -----

[[nodiscard]] constexpr int clamp_int(int v, int lo, int hi) noexcept {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Arrow-key nudge: v + delta, clamped to [lo, hi]. Up/Right => +1, Down/Left => -1.
[[nodiscard]] constexpr int step_clamped(int v, int delta, int lo, int hi) noexcept {
    return clamp_int(v + delta, lo, hi);
}

[[nodiscard]] constexpr bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }

// Parse a text field that the keystroke filter has limited to digits, clamped to
// [lo, hi]. Non-digits are ignored defensively; an empty / all-non-digit field reads
// as `lo`. A running overflow guard keeps very long input from wrapping.
[[nodiscard]] inline int parse_clamped_int(std::string_view text, int lo, int hi) noexcept {
    long value = 0;
    bool any_digit = false;
    for (char c : text) {
        if (is_digit(c)) {
            any_digit = true;
            value = value * 10 + (c - '0');
            if (value > 1'000'000) {
                value = 1'000'000;  // clamp the accumulator; final clamp is to [lo, hi]
            }
        }
    }
    if (!any_digit) {
        return lo;
    }
    return clamp_int(static_cast<int>(value), lo, hi);
}

// ----- street-split weights (Gameplay) -----
//
// Four integer percentages summing to 100. The "most-recently-touched-locks" rule:
// the slider the user just moved holds its new value exactly; the other three
// redistribute proportionally to their current values so the sum stays 100.

enum class Street : std::uint8_t { Preflop = 0, Flop = 1, Turn = 2, River = 3 };

struct StreetWeights {
    std::uint8_t preflop{15};
    std::uint8_t flop{35};
    std::uint8_t turn{30};
    std::uint8_t river{20};
};

inline constexpr StreetWeights kDefaultStreetWeights{15, 35, 30, 20};

// Redistribute after `touched` is set to `new_value` (clamped 0-100). The touched
// street holds exactly its clamped value; the remaining budget (100 - held) is split
// across the other three in proportion to their CURRENT values via the largest-
// remainder method (ties resolve to the lower street index, deterministic). When the
// other three are all zero the budget is spread as evenly as possible. The result
// always sums to exactly 100 and every field clamps within [0, 100].
[[nodiscard]] constexpr StreetWeights redistribute_street_weights(StreetWeights current,
                                                                  Street touched,
                                                                  int new_value) noexcept {
    const std::array<int, 4> w{current.preflop, current.flop, current.turn, current.river};
    const int t = static_cast<int>(touched);
    const int held = clamp_int(new_value, 0, 100);
    const int budget = 100 - held;

    int others_sum = 0;
    for (int i = 0; i < 4; ++i) {
        if (i != t) {
            others_sum += w[static_cast<std::size_t>(i)];
        }
    }

    std::array<int, 4> out{};
    out[static_cast<std::size_t>(t)] = held;

    if (others_sum <= 0) {
        // Degenerate: nothing to weight against. Spread the budget evenly, handing the
        // remainder to the lowest indices first.
        const int base = budget / 3;
        int extra = budget - base * 3;  // 0..2
        for (int i = 0; i < 4; ++i) {
            if (i == t) {
                continue;
            }
            out[static_cast<std::size_t>(i)] = base + (extra > 0 ? 1 : 0);
            if (extra > 0) {
                --extra;
            }
        }
    } else {
        std::array<int, 4> rem{};
        int assigned = 0;
        for (int i = 0; i < 4; ++i) {
            if (i == t) {
                continue;
            }
            const std::size_t u = static_cast<std::size_t>(i);
            const int numer = w[u] * budget;
            out[u] = numer / others_sum;
            rem[u] = numer % others_sum;
            assigned += out[u];
        }
        int deficit = budget - assigned;  // 0..2 leftover units
        while (deficit > 0) {
            int best = -1;
            for (int i = 0; i < 4; ++i) {
                if (i == t) {
                    continue;
                }
                if (best == -1 || rem[static_cast<std::size_t>(i)] > rem[static_cast<std::size_t>(best)]) {
                    best = i;
                }
            }
            out[static_cast<std::size_t>(best)] += 1;
            rem[static_cast<std::size_t>(best)] = -1;  // consumed; never re-picked
            --deficit;
        }
    }

    return StreetWeights{static_cast<std::uint8_t>(out[0]), static_cast<std::uint8_t>(out[1]),
                         static_cast<std::uint8_t>(out[2]), static_cast<std::uint8_t>(out[3])};
}

[[nodiscard]] constexpr StreetWeights street_weights_of(const GameplaySettings& g) noexcept {
    return StreetWeights{g.street_weight_preflop, g.street_weight_flop, g.street_weight_turn,
                         g.street_weight_river};
}

constexpr void apply_street_weights(GameplaySettings& g, StreetWeights w) noexcept {
    g.street_weight_preflop = w.preflop;
    g.street_weight_flop = w.flop;
    g.street_weight_turn = w.turn;
    g.street_weight_river = w.river;
}

// ----- difficulty range (Gameplay) -----
//
// Stored internally as two floats in [0.0, 1.0] (difficulty_min/max); shown to the
// user as integer percentages [0, 100]. Two independent handles (low, high) that
// cannot cross: an attempt to move a handle past the other is a no-op (no swap).

[[nodiscard]] constexpr int difficulty_display(float internal) noexcept {
    const int d = static_cast<int>(internal * 100.0f + 0.5f);
    return clamp_int(d, 0, 100);
}

[[nodiscard]] constexpr float difficulty_internal(int display) noexcept {
    return static_cast<float>(clamp_int(display, 0, 100)) / 100.0f;
}

struct DifficultyDisplay {
    int low{20};
    int high{80};
    constexpr bool operator==(const DifficultyDisplay&) const noexcept = default;
};

[[nodiscard]] constexpr DifficultyDisplay difficulty_display_range(const GameplaySettings& g) noexcept {
    return DifficultyDisplay{difficulty_display(g.difficulty_min), difficulty_display(g.difficulty_max)};
}

// Set the low handle to display value `d` (clamped 0-100). No-op if it would exceed
// the current high handle (handles cannot cross or swap).
[[nodiscard]] constexpr DifficultyDisplay set_difficulty_low(DifficultyDisplay r, int d) noexcept {
    const int v = clamp_int(d, 0, 100);
    if (v > r.high) {
        return r;
    }
    return DifficultyDisplay{v, r.high};
}

// Set the high handle to display value `d` (clamped 0-100). No-op if it would fall
// below the current low handle.
[[nodiscard]] constexpr DifficultyDisplay set_difficulty_high(DifficultyDisplay r, int d) noexcept {
    const int v = clamp_int(d, 0, 100);
    if (v < r.low) {
        return r;
    }
    return DifficultyDisplay{r.low, v};
}

constexpr void apply_difficulty(GameplaySettings& g, DifficultyDisplay r) noexcept {
    g.difficulty_min = difficulty_internal(r.low);
    g.difficulty_max = difficulty_internal(r.high);
}

// ----- slider ghost-default marker -----

// Normalized [0, 1] position of `value` along a [lo, hi] track. Used to place both
// the live handle and the faded ghost marker of the default value.
[[nodiscard]] constexpr float slider_fraction(float value, float lo, float hi) noexcept {
    if (hi <= lo) {
        return 0.0f;
    }
    const float f = (value - lo) / (hi - lo);
    return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
}

}  // namespace poker_trainer::settings
