#pragma once

#include "audio/audio_paths.hpp"  // audio::SfxId

#include <cstdint>
#include <functional>

// Screen-transition orchestrator (Zone 05 / Main Loop; the SEAM(Z14) transitions).
//
// The application's three transition registers (ARCHITECTURE — Notes — Transitions):
//   - Ceremonial fade-to-black-and-back (~1.5s) for significant state changes
//     (Mode Selection -> Game, Post-Round -> Mode Selection, Game -> Mode Selection).
//   - Slide (350ms ease-out) for the Game <-> Post-Round loop.
//   - Crossfade / button morph for Root <-> Mode Selection (owned by Zone 07, governed
//     by Reduce Motion — NOT this module).
//
// This module owns only the first two. Both are driven off the Global Animation Clock
// (backbone::total_ms_since_app_start), so their wall-clock durations are frame-rate
// independent. A single transition runs at a time; a second request while one is in
// flight is ignored (input is inert during a transition, so this only guards bugs).
//
// The "Screen transitions" Recap setting gates both: when OFF, every transition is an
// instant cut — the swap callback runs synchronously and no fade / slide / slide-SFX
// renders. The gate is injected by boot (set_transitions_enabled_gate); unwired (native
// tests, pre-boot) it reports OFF so callers behave synchronously with no render loop
// to advance the animation.
//
// The ImGui rendering (the black overlay, the two-screen slide) is compiled only under
// __EMSCRIPTEN__; the native test build gets no-op render bodies and exercises the
// (instant-cut) state logic. render_active_slide / render_ceremonial_overlay are called
// by the main loop, which is itself wasm-only.

namespace poker_trainer::bridge {

// True while any transition (ceremonial fade or slide) is animating. The DOM input
// layer drops events while this holds so the transition cannot be navigated through or
// clicked during, and no second transition can begin.
[[nodiscard]] bool is_screen_transition_active() noexcept;

// Begin the ~1.5s ceremonial fade-to-black-and-back. The current screen fades to fully
// black (~700ms), holds briefly, then the destination fades in (~700ms). `on_swap` runs
// exactly once, at the fully-black midpoint, and must perform the screen-state change
// plus any state cleanup so the user never sees a hard cut. When "Screen transitions" is
// OFF, this is an instant cut: `on_swap` runs immediately and no fade renders. Ignored
// (a no-op) if a transition is already in progress.
void begin_ceremonial_transition(std::function<void()> on_swap);

// Direction of a Game <-> Post-Round slide.
enum class SlideDirection : std::uint8_t {
    // Game -> Post-Round: 350ms right-to-left. Game exits left, Post-Round enters from
    // the right. Paired with SlideIn.
    GameToPostRound = 0,
    // Post-Round -> Game (Again commit): the symmetric 350ms left-to-right. Post-Round
    // exits right, Game enters from the left. Paired with SlideOut.
    PostRoundToGame = 1,
};

// Begin a 350ms ease-out slide between the Game and Post-Round screens. `sfx` (SlideIn /
// SlideOut) plays at slide start through the normal mute/volume path. `on_complete` runs
// exactly once, at the end of the slide, and performs the final state teardown (the
// screen-state swap for Game -> Post-Round; nothing for the reverse, whose launch already
// ran). When "Screen transitions" is OFF, this is an instant cut: `on_complete` runs
// immediately, no slide renders, and the SFX does NOT play (a slide sound for a slide the
// user never saw). Ignored if a transition is already in progress.
void begin_slide_transition(SlideDirection direction, audio::SfxId sfx,
                            std::function<void()> on_complete);

// Advance + render the active slide, drawing the outgoing and incoming screens at an
// animated horizontal offset. Returns true if it rendered the frame (the caller then
// skips its normal render_screen), false if no slide is active OR the slide finalized
// this frame (the caller then renders the now-current destination normally). Called by
// the main loop in place of render_screen.
[[nodiscard]] bool render_active_slide();

// Advance + render the ceremonial black overlay above every layer (screen, tutorial,
// modal), fading its alpha 0 -> 1 -> 0 and firing `on_swap` at the black midpoint.
// A no-op when no ceremonial transition is active. Called by the main loop last.
void render_ceremonial_overlay();

// Inject the "Screen transitions" live-setting reader (boot: the live Recap setting).
// Unwired => transitions are treated as OFF (instant cuts), so the native test build and
// any pre-boot call behave synchronously.
void set_transitions_enabled_gate(std::function<bool()> enabled);

// Clear all transition state + the injected gate. Used by tests.
void reset_screen_transition_for_testing() noexcept;

}  // namespace poker_trainer::bridge
