#pragma once

#include "tutorial/tutorial.hpp"

#include "backbone/screen_state.hpp"

#include "animations/button_morph.hpp"
#include "tutorial/step_sequencer.hpp"

#include <optional>

// Zone 14 — internal helpers shared between overlay.cpp and the Tutorial Complete
// screen (screens/tutorial_complete_screen.cpp). Not part of the public Z14 API.

namespace poker_trainer::tutorial {

// The installed app-root runtime, or nullptr before install_tutorial (native
// tests). Mirrors modal::modal_runtime(); free functions null-guard it.
[[nodiscard]] TutorialRuntime* tutorial_runtime();

// End the tutorial successfully and navigate to `dest` (Mode Selection for
// "Continue Playing", Root for "Home"): restore the forced settings, set
// has_seen_tutorial_prompt + has_completed_tutorial, set the tutorial phase
// Inactive, and switch the screen. Shared by the Tutorial Complete buttons.
void tutorial_finish_to(backbone::ScreenId dest);

// Restore the forced settings to the user's captured values (idempotent: a no-op if
// nothing was forced). Used by tutorial_skip / tutorial_finish_to.
void tutorial_restore_forced_settings();

// The on-screen rect a spotlight lights for `target` on a canvas of size `c`, or
// nullopt for SpotTarget::None.
//
// Exposed so it can be tested. The Game-screen rects are derived from
// render::compute_layout rather than restated as canvas fractions, and the invariant
// worth pinning is that each one actually contains the element it names -- that is
// precisely what silently broke when the table was reshaped and the hand-copied
// fractions stayed put.
[[nodiscard]] std::optional<animations::Rect> spotlight_rect(SpotTarget target,
                                                             animations::Canvas c);

}  // namespace poker_trainer::tutorial
