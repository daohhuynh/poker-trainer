#pragma once

#include "settings/settings.hpp"

// Zone 14 — forced-settings save / apply / restore (pure).
//
// While the tutorial walkthrough runs, three settings are forced to known values
// so the teaching surface is predictable (ARCHITECTURE "Notes — Tutorial System"
// §Forced Settings):
//   • HUD                  → ON  (the user sees floating bet / pot numbers)
//   • Visual Countdown     → OFF (no timer rendering while teaching)
//   • Bet Sizing Engine    → ON  (Step 6 is the multi-tier drill)
//
// The Delta Timer disable is NOT a setting override: backbone is_modal_locked /
// delta_timer derive it from the live tutorial phase, so it needs no save/restore.
// Reduce Motion, Theme, and Units keep the user's values and are untouched here.
//
// These functions only mutate an in-memory Settings; the tutorial never persists
// the forced values, so the user's durable IDBFS settings are intact whether the
// tutorial completes, is skipped, or the tab is closed mid-run. capture → apply at
// start, restore at exit, and the live Settings is byte-identical to its pre-start
// value (the symmetry the unit tests lock).

namespace poker_trainer::tutorial {

// The three user values displaced by the forced override, captured at start.
struct SavedSettings {
    bool show_hud{true};
    bool show_countdown{false};
    bool bet_sizing_enabled{true};
};

// Read the three to-be-forced fields off the live settings.
[[nodiscard]] SavedSettings capture_forced(const settings::Settings& live) noexcept;

// Force the three fields to their tutorial values.
void apply_forced(settings::Settings& live) noexcept;

// Restore the three fields from a prior capture. Symmetric with capture/apply:
// capture(s); apply(s); restore(s, saved) leaves s equal to its pre-apply value.
void restore_forced(settings::Settings& live, const SavedSettings& saved) noexcept;

}  // namespace poker_trainer::tutorial
