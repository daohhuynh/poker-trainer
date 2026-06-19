#pragma once

#include "persistence/sync_state.hpp"

#include "animations/button_morph.hpp"

struct ImDrawList;

// Zone 11 — Offline sync indicator (ARCHITECTURE L563). A glyph-only (no button
// fill) icon rendered to the LEFT of the leftmost cluster icon, in text_secondary,
// when a server-side sync has most recently FAILED. Reads the Phase-0 sync_state
// primitive (no direct Zone 04 dependency). Appears the frame after a failure,
// persists until a sync succeeds. Not in any focus list; never on Root.
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

// Render the indicator to the left of `leftmost_icon_rect` (a cluster icon's rect),
// reading live sync state. Draws nothing when a sync is not currently failing. The
// caller passes the current screen's leftmost cluster icon rect so the glyph aligns
// with the cluster; never called on Root.
void render_offline_indicator(ImDrawList* dl, const animations::Rect& leftmost_icon_rect);

}  // namespace poker_trainer::modal
