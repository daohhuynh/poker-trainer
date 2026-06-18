#pragma once

#include <cstdint>

// Zone 08 — Frog easter egg (ARCHITECTURE Game Screen: clicking the dealer 22
// consecutive times toggles Butler <-> Frog). Mouse-only; the dealer is not in
// any focus list, so the keyboard can never trigger this. The click counter
// resets between scenarios, but the active Butler/Frog state persists across
// scenarios within the session. Purely cosmetic; gameplay is unaffected.
//
// This is the PURE counter state machine (unit-tested). The side effects a toggle
// drives — calling bridge::load_frog_bundle() on the first toggle and
// play_sfx(FrogToggle) on each toggle — live in game_screen.cpp, which reacts to
// the Toggled outcome (tier4_requested gates the one-time load there).

namespace poker_trainer::easter_egg {

// Consecutive dealer clicks that complete one Butler <-> Frog toggle.
inline constexpr int kFrogToggleClicks = 22;

// Session-lifetime easter-egg state, owned by the Game-screen runtime.
struct FrogToggleState {
    int consecutive_clicks{0};  // resets between scenarios and on a completed toggle
    bool frog_active{false};    // persists across scenarios within the session
    bool tier4_requested{false};  // set by game_screen after the first load_frog_bundle()
};

// Outcome of registering one dealer click.
enum class FrogClickOutcome : std::uint8_t {
    Counting = 0,  // click counted; not yet at the toggle threshold
    Toggled = 1,   // this click completed the 22-click toggle (state flipped)
};

// Register a single dealer click. Increments the consecutive count; on reaching
// the threshold it flips frog_active, resets the count to 0, and returns Toggled.
FrogClickOutcome register_dealer_click(FrogToggleState& state) noexcept;

// Reset the consecutive-click counter (called on a new scenario). Leaves the
// active Butler/Frog state and the tier4_requested flag intact.
void reset_click_count(FrogToggleState& state) noexcept;

// The easter-egg-active query (read later by Z13 for the front-facing Frog).
[[nodiscard]] bool frog_active(const FrogToggleState& state) noexcept;

}  // namespace poker_trainer::easter_egg
