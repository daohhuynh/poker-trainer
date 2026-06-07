#pragma once

#include <functional>

// Per-frame callback registry (bridge / main-loop utility).
//
// The Emscripten main loop advances the animation clock once per frame and then
// must drive every zone's per-frame work (Z03 audio transport today; Z08
// animations and Z10 the timer later). Rather than the main loop hard-coding a call
// to each zone — which would edit frame() every time a zone wants a tick — zones
// register a per-frame callback at install time, exactly as they register event
// subscriptions through backbone::subscribe_*. The main loop ticks the clock, then
// runs every registered callback in registration order.
//
// This is deliberately NOT a backbone primitive: the six Phase-0 primitives are
// sealed, and a frame-tick fan-out is not one of them. It lives in the bridge layer
// (which owns the main loop) as a plain free-function registry, mirroring the
// backbone buses' shape — free functions over a small internal list — so a zone in
// any wave can self-register a tick without reaching the bridge's app-root runtime.
//
// All callbacks read the already-advanced animation clock, so there is no ordering
// dependency among them; a single ordered list invoked in registration order is the
// whole contract (no priorities). The clock-before-callbacks invariant is owned by
// the main loop, which calls backbone::tick(...) before run_frame_ticks().

namespace poker_trainer::bridge {

// A zone's per-frame work. Invoked once per frame after the animation clock has
// advanced; reads the clock for its own timing.
using FrameTickFn = std::function<void()>;

// Register `tick` to run once per frame. Ticks run in registration order. Called
// from a zone's install_*() at boot (single-threaded; not safe to call concurrently
// with run_frame_ticks(), which the single-threaded main loop guarantees).
void register_frame_tick(FrameTickFn tick);

// Invoke every registered tick in registration order. Called once per frame by the
// main loop, AFTER backbone::tick(...) has advanced the animation clock.
void run_frame_ticks();

// Drop all registered ticks. Used by tests to isolate cases; the app never needs to
// unregister (ticks live for the whole session).
void clear_frame_ticks() noexcept;

}  // namespace poker_trainer::bridge
