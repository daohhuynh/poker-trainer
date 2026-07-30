#pragma once

// Zone 07 — the Root screen's frog.
//
// A small frog sits in the Root screen's bottom-right corner on a quarter of
// visits. Clicking it ribbits and opens a speech bubble to its left; clicking
// again swaps the line. It is decoration on the Root screen, so it lives with
// the rest of Zone 07's Root code — src/easter_egg/ is Zone 08's dealer toggle
// (22 clicks on the dealer swaps the Butler for a frog), a different feature.
//
// Contract:
//   * The appearance roll happens once on ENTRY to Root, never per frame, so the
//     frog cannot strobe. Leaving to Mode Selection and coming back re-rolls.
//   * Nothing here persists. A reload that rolls a frog shows no bubble until it
//     is clicked; the state below is session-local and is never written to
//     settings or persistence.
//   * The frog never shares the screen with the first-run tutorial prompt — that
//     prompt owns the moment it is up.
//   * Mouse only. It is deliberately not a focusable: the 22-click dealer egg
//     sets that precedent (ARCHITECTURE keeps it out of the focus list), and a
//     Tab stop on a joke would be a real navigation cost.
//
// The state is caller-owned (Zone 05's main loop holds it inside ScreensRuntime
// and threads it in by reference), matching how the Root -> Mode MorphController
// is owned. CLAUDE.md section 10 forbids file-scope mutable state here.

#include "animations/button_morph.hpp"

#include "backbone/screen_state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace poker_trainer::screens {

// ----- Dialogue ---------------------------------------------------------------
//
// Short enough to read at a glance inside the bubble. The frog hints at things
// that are actually in the trainer (the 22-click dealer toggle, tomatoes only
// paying out on a clean pass, the four themes, the 10s pre-flop target, the
// shareable Scenario ID) rather than filling space.
inline constexpr std::array<std::string_view, 10> kFrogLines{
    "ribbit... try clicking on the dealer 22 times... ribbit",
    "ribbit... tomatoes only come from a clean pass... ribbit",
    "ribbit... the target time is a par, not a limit... ribbit",
    "ribbit... four themes. you will pick one and keep it... ribbit",
    "ribbit... the shop takes tomatoes. it takes a lot... ribbit",
    "ribbit... every hand has a number. share it... ribbit",
    "ribbit... pre-flop gives you ten seconds. be quick... ribbit",
    "ribbit... nobody reads the leaderboard. except me... ribbit",
    "ribbit... pot odds first. the rest is arithmetic... ribbit",
    "ribbit... the butler has no eyes. I have checked... ribbit",
};

inline constexpr std::size_t kFrogLineCount = kFrogLines.size();

// Percent of Root visits that get a frog.
inline constexpr std::uint32_t kFrogAppearPercent = 25;

// ----- State ------------------------------------------------------------------

struct RootFrogState {
    // Result of this visit's appearance roll.
    bool rolled_in{false};

    // Latched true if the first-run tutorial prompt was up at any point during
    // this visit. The prompt is raised by Zone 14 during its own render, which
    // can happen AFTER the entry roll on the very first frame, so a one-shot
    // check at entry is not enough — and once the prompt has owned the moment,
    // the frog stays away for the rest of the visit rather than popping in
    // behind the user's dismissal.
    bool prompt_seen_this_visit{false};

    // Whether the bubble is currently open, and which line it shows.
    bool bubble_open{false};
    std::size_t line{0};

    // Monotonic salt, bumped on every entry and every click, so two events that
    // land in the same millisecond still draw different values.
    std::uint32_t nonce{0};

    // Per-session entropy, drawn once from a non-deterministic source.
    //
    // The animation clock alone cannot seed this: total_ms_since_app_start()
    // restarts at 0 on every page load, so the entry roll on the boot frame always
    // saw the same (near-zero) input and produced the same answer every time. That
    // is not a 25% chance, it is one fixed outcome per build -- the frog either
    // always appeared on a fresh tab or, as shipped, never did. Mixing in a session
    // seed is what makes the roll actually random. Mirrors audio.cpp's
    // make_session_seed and bridge::game_launch's master_rng.
    std::uint64_t session_seed{0};
};

// ----- Pure logic -------------------------------------------------------------

// Session-local mixing. This is a splitmix64 finalizer — an avalanche function,
// not a random engine. Deliberately NOT std::mt19937_64: that generator is
// reserved for scenario generation, whose mapping from a Scenario ID is locked
// for reproducibility (CLAUDE.md section 5). A cosmetic frog must never perturb
// or borrow it. The entropy fed in is the animation clock's milliseconds since
// app start plus the state's nonce, both session-local by construction.
[[nodiscard]] constexpr std::uint64_t frog_mix(std::uint64_t x) noexcept {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

// The appearance roll: true for kFrogAppearPercent of (now_ms, nonce) pairs.
[[nodiscard]] constexpr bool roll_frog_appears(std::uint64_t now_ms,
                                               std::uint32_t nonce) noexcept {
    return frog_mix(now_ms + nonce) % 100ULL < kFrogAppearPercent;
}

// Line to show on a repeat click: uniform over the OTHER lines, so a second click
// can never look like it did nothing.
[[nodiscard]] constexpr std::size_t next_frog_line(std::size_t current,
                                                   std::uint64_t entropy) noexcept {
    const std::size_t offset =
        1u + static_cast<std::size_t>(frog_mix(entropy) % (kFrogLineCount - 1u));
    return (current + offset) % kFrogLineCount;
}

// Draws this session's entropy. Call once at install, before any entry roll.
void seed_root_frog(RootFrogState& state);

// Called once each time Root becomes the current screen. Re-rolls the frog and
// closes any bubble left over from the previous visit.
void on_root_entry(RootFrogState& state, std::uint64_t now_ms,
                   backbone::TutorialPhase phase) noexcept;

// Called every frame with the live tutorial phase; latches the prompt-suppression
// flag. Cheap and idempotent.
void note_tutorial_phase(RootFrogState& state, backbone::TutorialPhase phase) noexcept;

// Whether the frog should be on screen, against an explicitly-given tutorial
// phase. Pure, so the suppression rule is unit-testable without the backbone
// singleton. Two independent gates, both required:
//   * prompt_seen_this_visit — the prompt owned this visit, so the frog stays
//     away even after the user dismisses it, rather than popping in behind them.
//   * phase == Prompt — belt and braces for the boot frame, where Zone 14 raises
//     the prompt during its render, i.e. after this frame's latch has already run.
[[nodiscard]] constexpr bool frog_showing(const RootFrogState& state,
                                          backbone::TutorialPhase phase) noexcept {
    return state.rolled_in && !state.prompt_seen_this_visit &&
           phase != backbone::TutorialPhase::Prompt;
}

// The same predicate against the live tutorial phase from the screen-state
// singleton. Used by both the render and the click hit-test, so what is drawn and
// what is clickable can never disagree.
[[nodiscard]] bool frog_showing(const RootFrogState& state) noexcept;

// Apply a click on the frog: open the bubble on a fresh line, or swap to a
// different line if it is already open. Does not play the ribbit — the caller
// fires that through the bridge SFX seam so this stays pure and unit-testable.
void advance_frog_bubble(RootFrogState& state, std::uint64_t now_ms) noexcept;

// ----- Geometry ---------------------------------------------------------------

// The frog's rect: a square in the bottom-right corner, height-relative like the
// rest of the Root layout, clear of the centre 2x2 grid. Drawn AND hit-tested at
// exactly this rect (no breath, no tilt), so visible == clickable with no
// tolerance to reason about.
//
// The rect OVERHANGS the canvas's right and bottom edges by the art's transparent
// padding, so that the frog's ink sits flush in the corner. Callers must not
// assume it lies inside the viewport; the ink does, which is what matters for
// both drawing and hit-testing.
[[nodiscard]] animations::Rect frog_rect(animations::Canvas canvas) noexcept;

// The speech bubble's rect: up and to the LEFT of the frog, holding
// speech_bubble.png's 3:2 aspect. Positioned so the bubble's ink clears the frog's
// ink by a frog-relative gap and the tail tip aims at the frog's face. Like
// frog_rect this is a padded rect, so it may overlap frog_rect without the drawn
// art overlapping at all.
[[nodiscard]] animations::Rect frog_bubble_rect(animations::Canvas canvas) noexcept;

// The VISIBLE bounds of each: the rects above minus the art's transparent
// padding. The layout contract is written against these, not against the padded
// rects — the frog's ink is what sits flush in the corner, and it is the two inks
// that must never overlap. Exported so that can be asserted rather than assumed.
[[nodiscard]] animations::Rect frog_ink_rect(animations::Canvas canvas) noexcept;
[[nodiscard]] animations::Rect frog_bubble_ink_rect(animations::Canvas canvas) noexcept;

// ----- Render -----------------------------------------------------------------

// Draws the bubble (when open) and the frog over it. A no-op when the frog is not
// showing. Called from the Root dispatch after the Root body, so the frog sits
// above the screen's own art; it is deliberately NOT drawn during the Root ->
// Mode Selection morph, so it leaves the moment the user commits to Play.
void render_root_frog(const RootFrogState& state);

}  // namespace poker_trainer::screens
