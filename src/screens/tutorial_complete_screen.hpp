#pragma once

// Zone 14 — the Tutorial Complete terminal screen (ScreenId::TutorialComplete).
//
// A dedicated end-of-tutorial screen that replaces the app chrome entirely (no
// cluster, no logo). It renders the congratulations message and an account-state-
// dependent button row (guest: Sign Up / Continue Playing / Home; signed-in:
// Continue Playing / Home). Owned by Z14 but kept in src/screens/ per ZONES.md.
// install is invoked from tutorial::install_tutorial.

namespace poker_trainer::screens {

// Register the TutorialComplete render callback + its button event handlers. Reads
// the tutorial seams (account state, Auth0 health check, Sign Up modal) through
// tutorial::tutorial_runtime(), so it must be called after install_tutorial stores
// the runtime pointer.
void install_tutorial_complete_screen();

}  // namespace poker_trainer::screens
