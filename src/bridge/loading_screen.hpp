#pragma once

#include <cstddef>

// The Tier-1 loading screen (ARCHITECTURE Module 3 Tier 1, and Notes — Color
// Tint Theme "Loading screen dashed progress arc: accent_primary").
//
// Visual: the heavily-blurred Root background behind the dealer button centered
// large (~150-200px diameter); a progress arc traces clockwise along the
// button's inset dashed ring, filling proportionally to Tier-1 download
// progress; the button does not rotate and there is no separate progress bar.
// At a full 360deg the app transitions to Root. The arc uses the accent_primary
// theme token (the Module 3 prose calls it a "white arc", but the theme system
// forbids hardcoded colors and the token rules pin it to accent_primary — the
// token wins).
//
// Only the arc-fraction math is pure / testable; the ImGui rendering is gated to
// the emscripten build (declared in the wasm-only section below).

namespace poker_trainer::bridge {

// Fraction of Tier-1 loading complete, in [0, 1], from resolved/total asset
// counts. An empty tier (total == 0) reports 1.0 ("fully loaded"), matching
// assets::TierLoader::tier_progress. The arc sweeps from the top, clockwise,
// across fraction * 360 degrees.
[[nodiscard]] float loading_arc_fraction(std::size_t resolved,
                                         std::size_t total) noexcept;

#ifdef __EMSCRIPTEN__
// Render the loading screen for this frame and, when progress reaches 1.0,
// transition the screen state to Root. Reads the active AssetRegistry and
// TierLoader through the bridge runtime (see bridge_runtime.hpp).
void render_loading_screen();

// Tier-1 download progress in [0, 1] (ZONES.md Z05 export loading_progress()).
// The same fraction that fills the loading arc; reads the live TierLoader. Reports
// 1.0 before the loader exists (nothing to load yet) so callers never see a stall.
[[nodiscard]] float loading_progress();
#endif

}  // namespace poker_trainer::bridge
