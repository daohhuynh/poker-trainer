#pragma once

#include "persistence/sync_state.hpp"

#include "animations/button_morph.hpp"

struct ImDrawList;

// Zone 11 — Offline sync indicator (ARCHITECTURE L563). An informational cloud glyph — NOT
// a button, no fill, no text — sitting as its own row directly beneath the "Training tool,
// no real money." disclaimer, in text_secondary at ~70% alpha to match it. Shown when a
// server-side sync has most recently FAILED / is in backoff. The message itself is carried
// by the hover tooltip. Reads the Phase-0 sync_state primitive (no direct Zone 04
// dependency).
//
// Position: it hangs off ModalRuntime::disclaimer_rect, published by
// render_training_disclaimer on each frame it draws. So it appears wherever the disclaimer
// does (Root, Mode Selection, Game, Post-Round) and nowhere else — Tutorial Complete and
// the Error screen draw no disclaimer and therefore no indicator.
//
// Still rendered as a top-level overlay (via render_modal_overlay) on the FOREGROUND draw
// list, deliberately: the screens all draw into the background list, which
// render_modal_overlay then dims with the modal scrim and covers with the modal window.
// Sync failures are most often triggered from inside Settings and Shop, so an indicator on
// the background list would hide exactly when the sync it reports is failing.
//
// This is glyph-and-tooltip per the spec, but positioned under the disclaimer rather than
// the spec's "to the left of the Shop icon in the persistent cluster" — the corner it used
// to occupy is now the Root frog's, and the cluster row's left is where the Game screen's
// countdown timer already crowds.
//
// The visibility decision is pure (unit-tested); the render is ImGui (browser).

namespace poker_trainer::modal {

// Visible exactly when the most recent sync attempt has failed / is in backoff.
[[nodiscard]] constexpr bool offline_indicator_visible(persistence::SyncStatus status) noexcept {
    return status == persistence::SyncStatus::SyncFailing;
}

// Tooltip shown on hover. ARCHITECTURE spells this with an em dash; the app loads no
// custom font, so the atlas is ImGui's default ProggyClean over Basic Latin + Latin-1
// only and U+2014 has no glyph. Rendered as authored it drops a blank where the dash
// should be. The two sibling strings in this corner ("Training tool, no real money."
// and the old inline offline line) already carry the same ASCII-only note. This is now
// the ONLY place the message appears, so the substitution matters more than it did.
inline constexpr const char* kOfflineTooltip =
    "Offline - changes saved locally and will sync when you're back online.";

// Render the offline glyph under the disclaimer, reading live sync state. Draws nothing when
// a sync is not currently failing, and nothing on a frame where no disclaimer was drawn.
// `dl` is a top-level draw list (the caller passes the foreground list so it sits above any
// open modal).
void render_offline_indicator(ImDrawList* dl);

}  // namespace poker_trainer::modal
