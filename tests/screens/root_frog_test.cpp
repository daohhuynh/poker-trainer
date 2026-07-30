// Zone 07 — Root frog unit tests.
//
// Covers only the pure surface: the appearance roll and its tutorial-prompt
// suppression, the bubble's line selection, and the bottom-right geometry. The
// render itself (bubble art, wrapped text, the frog over it) draws through ImGui
// and is browser-verified, not faked into a test here.

#include "screens/root_frog.hpp"

#include "animations/button_morph.hpp"
#include "backbone/screen_state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <gtest/gtest.h>

namespace sc = poker_trainer::screens;
namespace bb = poker_trainer::backbone;
namespace an = poker_trainer::animations;

namespace {

constexpr an::Canvas kCanvas{1920.0f, 1080.0f};

// A frog that definitely rolled in, so suppression can be tested independently
// of the roll itself.
[[nodiscard]] sc::RootFrogState rolled_in_frog() {
    sc::RootFrogState state{};
    state.rolled_in = true;
    return state;
}

// Entry timestamps that produce a frog, so tests that need one are not hostage
// to a particular millisecond hashing the right way.
[[nodiscard]] std::uint64_t first_entry_ms_that_rolls_in(std::uint32_t nonce) {
    for (std::uint64_t ms = 0; ms < 1000; ++ms) {
        if (sc::roll_frog_appears(ms, nonce)) {
            return ms;
        }
    }
    ADD_FAILURE() << "no entry timestamp in the first second rolls a frog";
    return 0;
}

}  // namespace

// ----- The appearance roll ----------------------------------------------------

TEST(RootFrogRoll, LandsNearTheDeclaredQuarter) {
    std::size_t hits = 0;
    constexpr std::size_t kSamples = 20000;
    for (std::uint64_t ms = 0; ms < kSamples; ++ms) {
        if (sc::roll_frog_appears(ms, /*nonce=*/1)) {
            ++hits;
        }
    }
    const double rate = static_cast<double>(hits) / static_cast<double>(kSamples);
    EXPECT_NEAR(rate, 0.25, 0.02);
}

TEST(RootFrogRoll, ConsecutiveEntriesInTheSameMillisecondCanDiffer) {
    // The nonce is what keeps two entries that land in the same millisecond from
    // producing identical rolls forever.
    bool saw_true = false;
    bool saw_false = false;
    for (std::uint32_t nonce = 0; nonce < 64; ++nonce) {
        if (sc::roll_frog_appears(/*now_ms=*/5000, nonce)) {
            saw_true = true;
        } else {
            saw_false = true;
        }
    }
    EXPECT_TRUE(saw_true);
    EXPECT_TRUE(saw_false);
}

// ----- Tutorial-prompt suppression --------------------------------------------

TEST(RootFrogRoll, SuppressedWhenTheTutorialPromptIsUpAtEntry) {
    sc::RootFrogState state{};
    sc::on_root_entry(state, first_entry_ms_that_rolls_in(/*nonce=*/1),
                      bb::TutorialPhase::Prompt);
    EXPECT_FALSE(sc::frog_showing(state, bb::TutorialPhase::Prompt));
}

TEST(RootFrogRoll, SuppressionOutlastsThePromptForTheRestOfTheVisit) {
    // The prompt owns the moment it is up: dismissing it must not pop a frog into
    // the corner behind the user.
    sc::RootFrogState state{};
    sc::on_root_entry(state, first_entry_ms_that_rolls_in(/*nonce=*/1),
                      bb::TutorialPhase::Prompt);
    EXPECT_FALSE(sc::frog_showing(state, bb::TutorialPhase::Inactive));
}

TEST(RootFrogRoll, SuppressedWhenThePromptIsRaisedAfterEntry) {
    // Zone 14 raises the prompt during its own render, which can land after the
    // frame tick that rolled the frog.
    sc::RootFrogState state = rolled_in_frog();
    ASSERT_TRUE(sc::frog_showing(state, bb::TutorialPhase::Inactive));
    sc::note_tutorial_phase(state, bb::TutorialPhase::Prompt);
    EXPECT_FALSE(sc::frog_showing(state, bb::TutorialPhase::Inactive));
}

TEST(RootFrogRoll, TutorialPhasesOtherThanPromptDoNotSuppress) {
    sc::RootFrogState state = rolled_in_frog();
    sc::note_tutorial_phase(state, bb::TutorialPhase::Active);
    sc::note_tutorial_phase(state, bb::TutorialPhase::Complete);
    sc::note_tutorial_phase(state, bb::TutorialPhase::Inactive);
    EXPECT_TRUE(sc::frog_showing(state, bb::TutorialPhase::Active));
    EXPECT_TRUE(sc::frog_showing(state, bb::TutorialPhase::Complete));
}

TEST(RootFrogRoll, ANewVisitClearsThePreviousVisitsSuppression) {
    sc::RootFrogState state{};
    sc::on_root_entry(state, first_entry_ms_that_rolls_in(/*nonce=*/1),
                      bb::TutorialPhase::Prompt);
    ASSERT_TRUE(state.prompt_seen_this_visit);
    sc::on_root_entry(state, 12345, bb::TutorialPhase::Inactive);
    EXPECT_FALSE(state.prompt_seen_this_visit);
}

// ----- Nothing persists across a visit ----------------------------------------

TEST(RootFrogRoll, EntryClosesAnyBubbleLeftOpen) {
    sc::RootFrogState state = rolled_in_frog();
    sc::advance_frog_bubble(state, /*now_ms=*/777);
    ASSERT_TRUE(state.bubble_open);

    sc::on_root_entry(state, /*now_ms=*/888, bb::TutorialPhase::Inactive);
    EXPECT_FALSE(state.bubble_open);
}

// ----- The bubble's dialogue ---------------------------------------------------

TEST(RootFrogBubble, FirstClickOpensTheBubbleOnAValidLine) {
    sc::RootFrogState state = rolled_in_frog();
    sc::advance_frog_bubble(state, /*now_ms=*/4242);
    EXPECT_TRUE(state.bubble_open);
    EXPECT_LT(state.line, sc::kFrogLineCount);
}

TEST(RootFrogBubble, AnyLineIsReachableOnTheFirstClick) {
    // A first click that could never show line 0 would quietly shrink the pool.
    std::array<bool, sc::kFrogLineCount> reached{};
    for (std::uint64_t ms = 0; ms < 2000; ++ms) {
        sc::RootFrogState state = rolled_in_frog();
        sc::advance_frog_bubble(state, ms);
        reached[state.line] = true;
    }
    for (std::size_t i = 0; i < sc::kFrogLineCount; ++i) {
        EXPECT_TRUE(reached[i]) << "line " << i << " is unreachable on a first click";
    }
}

TEST(RootFrogBubble, ARepeatClickNeverRepeatsTheCurrentLine) {
    for (std::size_t current = 0; current < sc::kFrogLineCount; ++current) {
        for (std::uint64_t entropy = 0; entropy < 2000; ++entropy) {
            const std::size_t next = sc::next_frog_line(current, entropy);
            EXPECT_NE(next, current) << "current=" << current << " entropy=" << entropy;
            EXPECT_LT(next, sc::kFrogLineCount);
        }
    }
}

TEST(RootFrogBubble, EveryRepeatClickChangesWhatIsOnScreen) {
    // Clicking twice must never look like it did nothing.
    sc::RootFrogState state = rolled_in_frog();
    sc::advance_frog_bubble(state, /*now_ms=*/100);
    for (std::uint64_t ms = 101; ms < 400; ++ms) {
        const std::size_t before = state.line;
        sc::advance_frog_bubble(state, ms);
        EXPECT_TRUE(state.bubble_open);
        EXPECT_NE(state.line, before);
    }
}

TEST(RootFrogBubble, EveryLineIsShortEnoughToReadAtAGlance) {
    for (const std::string_view line : sc::kFrogLines) {
        EXPECT_FALSE(line.empty());
        // The bubble interior fits roughly four wrapped lines at the derived text
        // size; anything past this starts spilling out of the art.
        EXPECT_LE(line.size(), 64u) << line;
    }
}

// ----- Geometry ----------------------------------------------------------------

TEST(RootFrogGeometry, FrogSitsInTheBottomRightAndInsideTheCanvas) {
    const an::Rect frog = sc::frog_rect(kCanvas);
    EXPECT_GT(frog.x, kCanvas.width * 0.5f);
    EXPECT_GT(frog.y, kCanvas.height * 0.5f);
    EXPECT_LE(frog.x + frog.w, kCanvas.width);
    EXPECT_LE(frog.y + frog.h, kCanvas.height);
    EXPECT_FLOAT_EQ(frog.w, frog.h);  // the art is square; the rect must be too
}

TEST(RootFrogGeometry, BubbleSitsEntirelyLeftOfTheFrog) {
    const an::Rect frog = sc::frog_rect(kCanvas);
    const an::Rect bubble = sc::frog_bubble_rect(kCanvas);
    EXPECT_LT(bubble.x + bubble.w, frog.x);
    EXPECT_GT(bubble.x, 0.0f);
    // Holds speech_bubble.png's 3:2 aspect, so the panel is never stretched.
    EXPECT_NEAR(bubble.w / bubble.h, 768.0f / 512.0f, 0.001f);
}

TEST(RootFrogGeometry, FrogAndBubbleClearTheRootButtonGrid) {
    // The grid is the centre 2x2. The bubble is opaque, so overlapping Help (the
    // grid's bottom-right cell) would both look wrong and put unclickable pixels
    // over a live button. Checked across the aspects the canvas actually takes,
    // since the frog is height-relative and the gap to Help is not.
    const std::array<an::Canvas, 5> canvases{
        an::Canvas{1920.0f, 1080.0f}, an::Canvas{1440.0f, 900.0f},
        an::Canvas{1280.0f, 720.0f},  an::Canvas{1000.0f, 1000.0f},
        an::Canvas{2560.0f, 1080.0f},
    };
    for (const an::Canvas canvas : canvases) {
        const an::Rect help = an::root_grid_button_rect(an::MorphButton::Help, canvas);
        const an::Rect frog = sc::frog_rect(canvas);
        const an::Rect bubble = sc::frog_bubble_rect(canvas);
        EXPECT_GT(frog.x, help.x + help.w) << canvas.width << "x" << canvas.height;
        // The bubble may sit within Help's columns; it must never sit within its rows.
        EXPECT_GT(bubble.y, help.y + help.h) << canvas.width << "x" << canvas.height;
        EXPECT_LE(bubble.y + bubble.h, canvas.height) << canvas.width << "x" << canvas.height;
        EXPECT_GT(bubble.x, 0.0f) << canvas.width << "x" << canvas.height;
    }
}
