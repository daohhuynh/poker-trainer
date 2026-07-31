// Zone 12 — pure settings-logic unit tests: the coupled street-split redistribution,
// the integer parse/clamp/step behind the volume and custom-time inputs, and the
// slider ghost fraction. No ImGui (the section render TUs are browser-verified per
// CLAUDE.md §9).

#include "settings/settings_logic.hpp"
#include "settings/settings_modal.hpp"

#include "settings/settings.hpp"

#include "audio/audio_paths.hpp"

#include <cstddef>

#include <gtest/gtest.h>

namespace st = poker_trainer::settings;
namespace audio = poker_trainer::audio;

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

// The difficulty range setting was removed from the UI (F is now situational). Its
// display<->internal helpers and two-handle clamp are gone; the sealed settings field
// remains only for persistence compatibility and is exercised by the validation test
// below (SettingsValidation.RejectsInvertedDifficultyRange).

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

// ----- the genre filter's ordinal contract -----

// Two places convert ActiveMusicGenre to an audio-layer filter by subtracting one (boot's
// genre_filter_of, and the settings-blob codec's version-1 migration, which ADDS one going
// the other way). Both are silent if the +1 offset ever drifts — a wrong-but-valid genre is
// not a crash, just the wrong music — so pin the offset here.
TEST(GenreFilter, NonAllValuesSitOneAboveTheAudioGenre) {
    static_assert(static_cast<int>(st::ActiveMusicGenre::All) == 0,
                  "All must be the zero value so it is the default");
    static_assert(static_cast<int>(st::ActiveMusicGenre::LoungeJazz) ==
                  static_cast<int>(audio::MusicGenre::LoungeJazz) + 1);
    static_assert(static_cast<int>(st::ActiveMusicGenre::Classical) ==
                  static_cast<int>(audio::MusicGenre::Classical) + 1);
    static_assert(static_cast<int>(st::ActiveMusicGenre::BossaNova) ==
                  static_cast<int>(audio::MusicGenre::BossaNova) + 1);
    static_assert(static_cast<int>(st::ActiveMusicGenre::Ambient) ==
                  static_cast<int>(audio::MusicGenre::Ambient) + 1);
    // One genre value per catalog genre, plus All.
    EXPECT_EQ(static_cast<std::size_t>(st::ActiveMusicGenre::Ambient), audio::kMusicGenreCount);
}

// ----- the Settings Tab order must fit its storage in BOTH auth states -----

// Regression: active_focus_order was a hand-written 54 against a 53-stop order whose
// signed-in form is 55 (the guest Sign In / Sign Up pair is replaced by four stops).
// Every signed-in open therefore wrote one FocusableId past the end. On wasm32 those
// eight bytes ran off active_focus_count, through account_view_profile_open, and into
// search_buf[0..2] -- so opening Settings while signed in showed three unrenderable
// characters sitting in the search box, on every load, with no way to clear them.
// Guests were unaffected (53 fits), which is what made it look like a sync bug.
//
// Counted the way build_account_focus_order builds it, so the arithmetic is checked
// rather than the literal.
namespace {

[[nodiscard]] std::size_t focus_stops_for(bool logged_in) {
    std::size_t n = 0;
    for (const poker_trainer::backbone::FocusableId id : st::kSettingsFocusOrder) {
        if (id == st::kAcSignIn) {
            n += logged_in ? st::kLoggedInAccountStops : st::kGuestAccountStops;
        } else if (id == st::kAcSignUp) {
            // emitted alongside kAcSignIn
        } else {
            ++n;
        }
    }
    return n;
}

}  // namespace

TEST(SettingsFocusOrder, SignedInOrderFitsItsStorage) {
    const std::size_t capacity = st::SettingsModalState{}.active_focus_order.size();
    EXPECT_LE(focus_stops_for(/*logged_in=*/true), capacity);
    EXPECT_LE(focus_stops_for(/*logged_in=*/false), capacity);
}

TEST(SettingsFocusOrder, SignedInOrderIsTwoStopsLongerThanGuest) {
    // The exact relationship the capacity is derived from. If the account blocks change
    // size without kGuestAccountStops / kLoggedInAccountStops following, this fails
    // before the capacity silently goes short again.
    EXPECT_EQ(focus_stops_for(true),
              focus_stops_for(false) + st::kLoggedInAccountStops - st::kGuestAccountStops);
    EXPECT_EQ(focus_stops_for(false), st::kSettingsFocusOrder.size());
}
