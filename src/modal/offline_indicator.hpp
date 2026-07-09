#pragma once

#include "persistence/sync_state.hpp"

#include "animations/button_morph.hpp"

struct ImDrawList;

// Zone 11 — Offline sync indicator (ARCHITECTURE L563). An informational text line — NOT a
// button — in the bottom-right corner of the screen: the message then a small cloud glyph,
// styled to match the "Training tool, no real money." disclaimer exactly (the 0.8x muted
// font in text_secondary at ~70% alpha). Shown when a server-side sync has most recently
// FAILED / is in backoff. Reads the Phase-0 sync_state primitive (no direct Zone 04
// dependency). Rendered as a top-level overlay element (via render_modal_overlay), so it
// appears the frame the failure occurs, on EVERY screen — Root included, now that it is a
// screen-corner line rather than a cluster element — and above an open modal.
//
// The visibility decision is pure (unit-tested); the render is ImGui (browser).

namespace poker_trainer::modal {

// Visible exactly when the most recent sync attempt has failed / is in backoff.
[[nodiscard]] constexpr bool offline_indicator_visible(persistence::SyncStatus status) noexcept {
    return status == persistence::SyncStatus::SyncFailing;
}

// Tooltip shown on hover (ARCHITECTURE-exact text).
inline constexpr const char* kOfflineTooltip =
    "Offline — changes saved locally and will sync when you're back online.";

// Render the offline line in the bottom-right corner, reading live sync state. Draws nothing
// when a sync is not currently failing. `dl` is a top-level draw list (the caller passes the
// foreground list so it sits above any open modal); the position is computed from the
// viewport, so it needs no cluster geometry.
void render_offline_indicator(ImDrawList* dl);

}  // namespace poker_trainer::modal
