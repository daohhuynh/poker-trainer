#pragma once

// The Tier 1/2 fatal-failure error screen (ARCHITECTURE Module 3 Tier 1/2
// failure handling). A bg_primary fill, a centered "Couldn't load. Check your
// connection and try again." message, and a single themed Retry button that
// reloads the page (window.location.reload). No dashed-ring progress indicator.
//
// Z05 shows this when Zone 02 reports a Tier-1 asset unavailable (the boot
// loading phase), or when the screen state is set to ScreenId::Error for a
// Tier-2-on-use failure. Z05 does not own the retry/backoff (Zone 02 does); it
// only decides when to surface the screen. Emscripten-only.

namespace poker_trainer::bridge {

#ifdef __EMSCRIPTEN__
// Render the error screen for this frame (definition in error_screen.cpp).
void render_error_screen();
#endif

}  // namespace poker_trainer::bridge
