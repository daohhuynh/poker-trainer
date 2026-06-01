#include "bridge/boot.hpp"

#include "bridge/bridge_runtime.hpp"
#include "bridge/cdn_fetch.hpp"
#include "bridge/main_loop.hpp"
#include "bridge/platform.hpp"
#include "bridge/shared_scenario.hpp"

#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"
#include "backbone/screen_state.hpp"

#include "assets/tier_config.hpp"
#include "assets/tier_loader.hpp"

#include "engine/scenario_id.hpp"

#include "persistence/idbfs.hpp"
#include "persistence/persistence_schema.hpp"

#include "theme/theme.hpp"
#include "theme/theme_tokens.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <emscripten/emscripten.h>

// Boot orchestration, held to -Wall -Wextra -Werror in bridge_platform.

namespace poker_trainer::bridge {

namespace {

std::unique_ptr<BridgeRuntime> g_runtime;

// SEAM: production IDBFS StorageBackend (Emscripten FS + FS.syncfs) deferred.
// Auth0 + server sync are out of V1 client scope (CLAUDE.md §1), so boot does a
// guest-mode reconcile only. Until the durable IDBFS backend lands, read()
// returns no blob and load_state() yields fresh guest defaults.
class NullStorageBackend final : public persistence::StorageBackend {
 public:
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read() const override {
        return std::nullopt;
    }
    void write(std::span<const std::uint8_t> /*bytes*/) override {}
    void clear() override {}
};

// Universal Tab / Shift-Tab focus navigation: the backbone spec routes Tab to
// focus_manager.advance_focus and activates keyboard mode on first Tab. This is
// the screen-independent fallback at the lowest priority; screens/modals install
// higher-priority handlers above it.
bool on_focus_nav_key(const backbone::KeyEvent& e) {
    if (e.type != backbone::KeyEventType::KeyDown ||
        e.code != backbone::KeyCode::Tab) {
        return false;
    }
    backbone::activate_keyboard_mode();
    backbone::advance_focus(backbone::has_mod(e.mods, backbone::ModMask::Shift));
    return true;
}

void init_backbone() {
    // Backbone init order (ARCHITECTURE Notes — Communication Backbone):
    //   1. Animation clock        (no deps; advanced once/frame by the main loop)
    //   2. Screen state singleton (no deps)
    //   3. Event router           (depends on screen state for context eval)
    //   4. Focus manager          (depends on event router for key routing)
    //   5. Scenario lifecycle bus (no deps)
    //   6. Modal state observer   (depends on event router + focus manager)
    // The primitives are zero-initialized global state, so steps 1-3 and 5 need
    // no explicit call. Step 4's router<->focus integration is concrete: install
    // the universal Tab/Shift-Tab navigation handler.
    (void)backbone::register_key_handler(
        {}, on_focus_nav_key, backbone::HandlerPriority::BackgroundCatchAll,
        "bridge.focus_nav");
    // SEAM(Z11): the modal-state observer is initialized here in the fixed order.
    // Zone 11 owns modal_state.cpp; its query/notify API needs no boot-time call
    // (the observer is stateless global state, supplied this wave by
    // modal_state_stub.cpp), so this step is intentionally a no-op for now.
}

void init_zones(BootRoute route) {
    BridgeRuntime& rt = *g_runtime;

    // Zone 02: construct the asset registry + tier loader (Z05 owns the CDN
    // fetch wrapper) and kick the synchronous Tier-1 load.
    rt.tier_loader = std::make_unique<assets::TierLoader>(
        rt.registry, make_cdn_fetch(), assets::make_png_decoder(),
        assets::make_steady_clock());
    (void)rt.tier_loader->load_tier(assets::AssetTier::Tier1);
    if (route == BootRoute::SharedScenario) {
        // Force the Tier-3 on-click load now so SFX + default music are ready
        // when the user lands directly on Game (Notes — Shared Scenario URL).
        (void)rt.tier_loader->load_tier(assets::AssetTier::Tier3);
    }

    // Zone 04: guest-mode persistence reconcile via load_state().
    static NullStorageBackend storage;  // SEAM: see NullStorageBackend above.
    persistence::IdbfsStore store{storage};
    (void)store.load_state();

    // Seed the ImGui style with the default theme. Applying the *persisted* theme
    // needs Zone 12's settings (de)serialization; deferred there. // SEAM(Z12)
    theme::set_theme(theme::kThemeIdNoLimit);
}

}  // namespace

BridgeRuntime& runtime() noexcept { return *g_runtime; }

void app_init() {
    g_runtime = std::make_unique<BridgeRuntime>();

    // Backbone first, in the fixed order.
    init_backbone();

    // Bring up the Emscripten platform (WebGL2 + ImGui + GL renderer + input)
    // before any screen draws. If this fails the browser has no WebGL2 (or the
    // renderer could not initialize) and there is no ImGui context — so there is
    // nothing to render to and no safe main loop to run (the frame callback
    // dereferences the ImGui context). Stop here rather than entering the loop.
    if (!platform_init()) {
        return;
    }

    // Parse ?scenario= and resolve the boot route (malformed -> normal Root).
    const char* search = emscripten_run_script_string("location.search");
    const std::optional<engine::ScenarioId> shared =
        parse_shared_scenario(search != nullptr ? search : "");
    g_runtime->route = resolve_boot_route(shared);
    if (shared.has_value()) {
        g_runtime->shared_id = *shared;
    }

    // Then zone-level init (assets, persistence).
    init_zones(g_runtime->route);

    // Versioned asset caching for returning visitors (best-effort).
    EM_ASM({
        if ('serviceWorker' in navigator) {
            navigator.serviceWorker.register('service_worker.js').catch(
                function() {});
        }
    });

    start_main_loop();
}

}  // namespace poker_trainer::bridge
