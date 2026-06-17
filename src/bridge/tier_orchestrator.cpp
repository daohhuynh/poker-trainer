#include "bridge/tier_orchestrator.hpp"

#include "bridge/bridge_runtime.hpp"
#include "bridge/frame_tick.hpp"
#include "bridge/game_launch.hpp"
#include "bridge/sfx_prefetch.hpp"
#include "bridge/shared_scenario.hpp"
#include "bridge/tier_schedule.hpp"

#include "assets/asset_paths.hpp"
#include "assets/registry.hpp"
#include "assets/tier_config.hpp"
#include "assets/tier_loader.hpp"

#include "backbone/screen_state.hpp"

#include <array>
#include <cstddef>
#include <optional>

// Binding-adjacent orchestration (touches the app-root runtime + MEMFS), compiled
// only into the wasm app's bridge_platform layer and held to the reduced binding
// baseline, like the other Z05 boot / main-loop translation units.

namespace poker_trainer::bridge {

namespace {

// Idempotency: each tier is kicked at most once for the whole session. A tier's
// PNG members go through Zone 02's TierLoader (itself idempotent), and its SFX
// members go to MEMFS once. Indexed by AssetTier (1..4); slot 0 is unused.
std::array<bool, 5> g_tier_kicked{};

// The screen observed on the previous orchestrator tick, used to detect the
// Root -> Mode Selection edge that fires Tier 3. Unset until the first tick so we
// never treat the initial state as a transition.
std::optional<backbone::ScreenId> g_prev_screen;

[[nodiscard]] std::size_t tier_index(assets::AssetTier tier) noexcept {
    return static_cast<std::size_t>(tier);
}

// Fire a tier exactly once: its PNG members via the TierLoader, its SFX members
// into MEMFS. Tier 1 / Tier 4 carry no SFX and Tier 3 carries no PNGs, so the
// respective half is a no-op there.
void ensure_tier_loading(assets::AssetTier tier) {
    const std::size_t idx = tier_index(tier);
    if (g_tier_kicked[idx]) {
        return;
    }
    BridgeRuntime& rt = runtime();
    if (rt.tier_loader == nullptr) {
        return;  // defensive: the loader is constructed before any trigger fires
    }
    g_tier_kicked[idx] = true;
    (void)rt.tier_loader->load_tier(tier);
    prefetch_sfx_tier_into_memfs(tier);
}

// The per-frame orchestrator work (registered as a frame tick). Reads the
// already-advanced state: once the app is Running, kick Tier 2 in the background;
// on the Root -> Mode Selection edge, kick Tier 3; and drive any deferred launch.
void orchestrator_tick() {
    const BridgeRuntime& rt = runtime();

    // Tier 2: background load after Root renders. ensure_tier_loading is
    // idempotent, so calling it every Running frame kicks the load exactly once.
    if (rt.phase == BootPhase::Running) {
        ensure_tier_loading(assets::AssetTier::Tier2);
    }

    // Tier 3: fired on the Root -> Mode Selection navigation edge.
    const backbone::ScreenId current = backbone::read_screen_state().current;
    if (g_prev_screen.has_value() &&
        is_root_to_mode_transition(*g_prev_screen, current)) {
        ensure_tier_loading(assets::AssetTier::Tier3);
    }
    g_prev_screen = current;

    // Complete a launch that was deferred while its Tier-2 assets downloaded.
    poll_pending_launch();
}

}  // namespace

void start_tiered_loading(BootRoute route) {
    ensure_tier_loading(assets::AssetTier::Tier1);
    if (route == BootRoute::SharedScenario) {
        // Shared-scenario lands directly on Game (bypassing Root and Mode
        // Selection), so the normal Tier-2 / Tier-3 triggers never fire — force
        // both now, concurrently with Tier 1.
        ensure_tier_loading(assets::AssetTier::Tier2);
        ensure_tier_loading(assets::AssetTier::Tier3);
    }
}

void install_tier_orchestrator() { register_frame_tick(orchestrator_tick); }

void load_frog_bundle() { ensure_tier_loading(assets::AssetTier::Tier4); }

LaunchAssetReadiness game_launch_asset_readiness() {
    const assets::AssetRegistry& reg = runtime().registry;
    bool any_pending = false;
    for (std::size_t i = 0; i < assets::kAssetCount; ++i) {
        const auto id = static_cast<assets::AssetId>(i);
        if (!is_game_launch_required_asset(id)) {
            continue;
        }
        if (reg.is_unavailable(id)) {
            return LaunchAssetReadiness::Failed;  // fail fast: cannot ever succeed
        }
        if (!reg.is_loaded(id)) {
            any_pending = true;
        }
    }
    return any_pending ? LaunchAssetReadiness::Pending : LaunchAssetReadiness::Ready;
}

}  // namespace poker_trainer::bridge
