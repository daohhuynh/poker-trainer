#pragma once

#include "bridge/game_launch.hpp"
#include "bridge/shared_scenario.hpp"

#include "assets/tier_config.hpp"

// Navigation-gated tier loading (ARCHITECTURE Module 3 "Loading Strategy").
//
// This is the live wiring that fires each loading tier at the right moment and
// applies the per-tier failure disposition. It is the Emscripten-side counterpart
// to the pure decisions in tier_schedule.hpp: it reaches the app-root runtime
// (the asset registry + Zone 02 TierLoader), drives the Zone 03 SFX into MEMFS per
// tier, reads the readable screen state for the Tier-3 navigation edge, and exposes
// the Tier-4 on-demand hook. Emscripten-only (bridge_platform); never compiled into
// the native test build (the decisions it composes are tested through tier_schedule
// and game_launch).
//
// Triggers (ARCHITECTURE Module 3):
//   * Tier 1 — fired synchronously at boot (start_tiered_loading), drives the
//     loading screen.
//   * Tier 2 — fired once the app reaches Running (Root has rendered): the
//     orchestrator's per-frame tick kicks it in the background.
//   * Tier 3 — fired on the Root -> Mode Selection navigation edge, observed by
//     the same per-frame tick; the shared-scenario route force-fires it (with
//     Tier 2) at boot instead, since it bypasses Mode Selection.
//   * Tier 4 — fired on demand via load_frog_bundle() (a SEAM Zone 08 calls on the
//     first Butler <-> Frog toggle).

namespace poker_trainer::bridge {

// Kick the boot-time loads: Tier 1 always (the loading-screen set). On the
// shared-scenario route — which lands directly on Game, bypassing Root and Mode
// Selection — also force-fire Tier 2 (table / cards) and Tier 3 (remaining SFX)
// now, so the Game screen's assets download concurrently with Tier 1. Called from
// boot's init_assets once the TierLoader exists.
void start_tiered_loading(BootRoute route);

// Register the per-frame orchestrator tick (Tier-2-on-Running, Tier-3-on-edge,
// and the deferred-launch poll). Called once at boot, after the runtime and the
// screen renderers are installed. Mirrors the install_*() convention.
void install_tier_orchestrator();

// Tier-4 on-demand SEAM: fetch the Frog easter-egg dealer bundle (frog_base +
// the two expression overlays) as one tier load. Zone 08 calls this on the first
// dealer toggle of a session; Z05 only exposes the hook and never wires the call
// site. Idempotent — the bundle is fetched at most once.
//
// SEAM(Z12/Z03): the alternate-genre starter tracks (the other Tier-4 members)
// are NOT loaded here. Music streams by URL through Zone 03's HTML5 <audio>
// (ARCHITECTURE Module 3 "Streaming Audio"), and the Service Worker caches each
// track on first stream — there is no Z05 fetch path for music, so genre-select
// pre-warming is Zone 03's streaming concern, not a tier load.
void load_frog_bundle();

// Evaluate the readiness of the Game-launch required Tier-2 assets against the
// live asset registry, for game_launch's navigation guard. Boot wires this as the
// launch asset guard. See is_game_launch_required_asset (tier_schedule.hpp).
[[nodiscard]] LaunchAssetReadiness game_launch_asset_readiness();

}  // namespace poker_trainer::bridge
