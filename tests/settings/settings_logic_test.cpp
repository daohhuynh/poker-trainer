// Zone 12 — pure settings-logic unit tests: the coupled street-split redistribution,
// the two-handle difficulty-range clamp + display<->internal round-trip, the integer
// parse/clamp/step behind the volume and custom-time inputs, and the slider ghost
// fraction. No ImGui (the section render TUs are browser-verified per CLAUDE.md §9).

#include "settings/settings_logic.hpp"

#include "settings/settings.hpp"

#include <gtest/gtest.h>

namespace st = poker_trainer::settings;

namespace {

int weight_sum(const st::StreetWeights& w) {
    return w.preflop + w.flop + w.turn + w.river;
}

}  // namespace

// ----- street split: most-recently-touched-locks -----

TEST(StreetSplit, TouchedHoldsExactlyAndSumStays100) {
    const st::StreetWeights r =
        st::redistribute_street_weights(st::kDefaultStreetWeights, st::Street::Preflop, 50);
    EXPECT_EQ(static_cast<int>(r.preflop), 50);  // touched holds exactly
    EXPECT_EQ(weight_sum(r), 100);
}

TEST(StreetSplit, OthersRedistributeProportionally) {
    // From {15,35,30,20}, set preflop -> 50: budget 50 split across {35,30,20} by the
    // largest-remainder method yields {flop 20, turn 18, river 12}.
    const st::StreetWeights r =
        st::redistribute_street_weights(st::kDefaultStreetWeights, st::Street::Preflop, 50);
    EXPECT_EQ(static_cast<int>(r.preflop), 50);
    EXPECT_EQ(static_cast<int>(r.flop), 20);
    EXPECT_EQ(static_cast<int>(r.turn), 18);
    EXPECT_EQ(static_cast<int>(r.river), 12);
    // Proportions preserve the original ordering flop > turn > river.
    EXPECT_GT(r.flop, r.turn);
    EXPECT_GT(r.turn, r.river);
}

TEST(StreetSplit, ClampsTouchedAtZeroAndKeepsSum) {
    const st::StreetWeights r =
        st::redistribute_street_weights(st::kDefaultStreetWeights, st::Street::Preflop, -10);
    EXPECT_EQ(static_cast<int>(r.preflop), 0);
    EXPECT_EQ(weight_sum(r), 100);
}

TEST(StreetSplit, ClampsTouchedAt100AndZeroesOthers) {
    const st::StreetWeights r =
        st::redistribute_street_weights(st::kDefaultStreetWeights, st::Street::River, 250);
    EXPECT_EQ(static_cast<int>(r.river), 100);
    EXPECT_EQ(static_cast<int>(r.preflop), 0);
    EXPECT_EQ(static_cast<int>(r.flop), 0);
    EXPECT_EQ(static_cast<int>(r.turn), 0);
    EXPECT_EQ(weight_sum(r), 100);
}

TEST(StreetSplit, DegenerateOthersAllZeroSpreadsEvenly) {
    const st::StreetWeights start{100, 0, 0, 0};
    const st::StreetWeights r = st::redistribute_street_weights(start, st::Street::Preflop, 40);
    EXPECT_EQ(static_cast<int>(r.preflop), 40);
    EXPECT_EQ(static_cast<int>(r.flop), 20);
    EXPECT_EQ(static_cast<int>(r.turn), 20);
    EXPECT_EQ(static_cast<int>(r.river), 20);
    EXPECT_EQ(weight_sum(r), 100);
}

TEST(StreetSplit, SumInvariantAcrossEveryTouchedValue) {
    for (int v = -5; v <= 105; ++v) {
        for (int s = 0; s < 4; ++s) {
            const st::StreetWeights r = st::redistribute_street_weights(
                st::kDefaultStreetWeights, static_cast<st::Street>(s), v);
            EXPECT_EQ(weight_sum(r), 100) << "street=" << s << " v=" << v;
        }
    }
}

TEST(StreetSplit, RoundTripsThroughGameplaySettings) {
    st::GameplaySettings g{};
    st::apply_street_weights(g, st::StreetWeights{10, 40, 30, 20});
    const st::StreetWeights back = st::street_weights_of(g);
    EXPECT_EQ(static_cast<int>(back.preflop), 10);
    EXPECT_EQ(static_cast<int>(back.flop), 40);
    EXPECT_EQ(static_cast<int>(back.turn), 30);
    EXPECT_EQ(static_cast<int>(back.river), 20);
}

// ----- difficulty range: two-handle clamp + display<->internal -----

TEST(DifficultyRange, DisplayInternalRoundTrip) {
    EXPECT_EQ(st::difficulty_display(0.2f), 20);
    EXPECT_EQ(st::difficulty_display(0.8f), 80);
    EXPECT_FLOAT_EQ(st::difficulty_internal(20), 0.2f);
    EXPECT_FLOAT_EQ(st::difficulty_internal(80), 0.8f);
    for (int d = 0; d <= 100; ++d) {
        EXPECT_EQ(st::difficulty_display(st::difficulty_internal(d)), d) << "d=" << d;
    }
}

TEST(DifficultyRange, LowHandleCannotCrossHigh) {
    const st::DifficultyDisplay r{20, 80};
    EXPECT_EQ(st::set_difficulty_low(r, 50), (st::DifficultyDisplay{50, 80}));
    EXPECT_EQ(st::set_difficulty_low(r, 80), (st::DifficultyDisplay{80, 80}));  // equal allowed
    EXPECT_EQ(st::set_difficulty_low(r, 81), r);                               // cross => no-op
    EXPECT_EQ(st::set_difficulty_low(r, 200), r);                              // clamp then no-op
}

TEST(DifficultyRange, HighHandleCannotCrossLow) {
    const st::DifficultyDisplay r{20, 80};
    EXPECT_EQ(st::set_difficulty_high(r, 50), (st::DifficultyDisplay{20, 50}));
    EXPECT_EQ(st::set_difficulty_high(r, 20), (st::DifficultyDisplay{20, 20}));  // equal allowed
    EXPECT_EQ(st::set_difficulty_high(r, 19), r);                               // cross => no-op
    EXPECT_EQ(st::set_difficulty_high(r, -5), r);                               // clamp then no-op
}

TEST(DifficultyRange, AppliesToGameplaySettings) {
    st::GameplaySettings g{};
    st::apply_difficulty(g, st::DifficultyDisplay{30, 70});
    EXPECT_FLOAT_EQ(g.difficulty_min, 0.3f);
    EXPECT_FLOAT_EQ(g.difficulty_max, 0.7f);
    EXPECT_EQ(st::difficulty_display_range(g), (st::DifficultyDisplay{30, 70}));
}

// ----- volume / custom-time integer inputs -----

TEST(IntInput, VolumeParseClampsToRange) {
    EXPECT_EQ(st::parse_clamped_int("75", 0, 100), 75);
    EXPECT_EQ(st::parse_clamped_int("150", 0, 100), 100);
    EXPECT_EQ(st::parse_clamped_int("", 0, 100), 0);
    EXPECT_EQ(st::parse_clamped_int("007", 0, 100), 7);
    EXPECT_EQ(st::parse_clamped_int("9999999", 0, 100), 100);
}

TEST(IntInput, CustomTimeParseClampsToOneTo300) {
    EXPECT_EQ(st::parse_clamped_int("30", 1, 300), 30);
    EXPECT_EQ(st::parse_clamped_int("0", 1, 300), 1);
    EXPECT_EQ(st::parse_clamped_int("500", 1, 300), 300);
    EXPECT_EQ(st::parse_clamped_int("", 1, 300), 1);
}

TEST(IntInput, ArrowStepClampsAtBounds) {
    EXPECT_EQ(st::step_clamped(50, 1, 0, 100), 51);
    EXPECT_EQ(st::step_clamped(100, 1, 0, 100), 100);
    EXPECT_EQ(st::step_clamped(0, -1, 0, 100), 0);
    EXPECT_EQ(st::step_clamped(1, -1, 1, 300), 1);
    EXPECT_EQ(st::step_clamped(300, 1, 1, 300), 300);
}

TEST(IntInput, DigitFilterRejectsNonDigits) {
    EXPECT_TRUE(st::is_digit('0'));
    EXPECT_TRUE(st::is_digit('9'));
    EXPECT_FALSE(st::is_digit('a'));
    EXPECT_FALSE(st::is_digit('-'));
    EXPECT_FALSE(st::is_digit('.'));
    EXPECT_FALSE(st::is_digit(' '));
}

// ----- slider ghost fraction -----

TEST(SliderFraction, NormalizesAndClamps) {
    EXPECT_FLOAT_EQ(st::slider_fraction(50.0f, 0.0f, 100.0f), 0.5f);
    EXPECT_FLOAT_EQ(st::slider_fraction(0.2f, 0.0f, 1.0f), 0.2f);
    EXPECT_FLOAT_EQ(st::slider_fraction(150.0f, 0.0f, 100.0f), 1.0f);
    EXPECT_FLOAT_EQ(st::slider_fraction(-5.0f, 0.0f, 100.0f), 0.0f);
    EXPECT_FLOAT_EQ(st::slider_fraction(5.0f, 0.0f, 0.0f), 0.0f);  // degenerate track
}

// ----- settings::validate() (defined in the Z12 lib) -----

TEST(Validate, DefaultSettingsAreValid) {
    EXPECT_EQ(st::validate(st::Settings{}), st::SettingsValidationResult::Ok);
}

TEST(Validate, RejectsBadStreetWeights) {
    st::Settings s{};
    s.gameplay.street_weight_preflop = 99;  // sum != 100
    EXPECT_EQ(st::validate(s), st::SettingsValidationResult::InvalidStreetWeights);
}

TEST(Validate, RejectsBadCustomWeights) {
    st::Settings s{};
    s.gameplay.custom_aggressor_weight = 60;  // 60 + 50 != 100
    EXPECT_EQ(st::validate(s), st::SettingsValidationResult::InvalidCustomModeWeights);
}

TEST(Validate, RejectsInvertedDifficultyRange) {
    st::Settings s{};
    s.gameplay.difficulty_min = 0.9f;
    s.gameplay.difficulty_max = 0.1f;  // min > max
    EXPECT_EQ(st::validate(s), st::SettingsValidationResult::InvalidDifficultyRange);
}

TEST(Validate, RejectsOutOfRangeCustomTimeWhenEnabled) {
    st::Settings s{};
    s.gameplay.time_pressure_custom_enabled = true;
    s.gameplay.time_pressure_custom_seconds = 0;  // < 1
    EXPECT_EQ(st::validate(s), st::SettingsValidationResult::InvalidTimePressureCustom);
}

TEST(Validate, RejectsOutOfRangeVolume) {
    st::Settings s{};
    s.audio.volume = 200;  // > 100
    EXPECT_EQ(st::validate(s), st::SettingsValidationResult::InvalidVolumeValue);
}

TEST(Validate, RejectsOverlongDisplayName) {
    st::Settings s{};
    s.account.display_name_override = std::string(40, 'x');  // > 32
    EXPECT_EQ(st::validate(s), st::SettingsValidationResult::InvalidDisplayNameOverride);
}
