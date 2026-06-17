#include "bridge/boot.hpp"

#include "bridge/bridge_runtime.hpp"
#include "bridge/cdn_fetch.hpp"
#include "bridge/game_launch.hpp"
#include "bridge/idbfs_backend.hpp"
#include "bridge/main_loop.hpp"
#include "bridge/persistent_weights_store.hpp"
#include "bridge/platform.hpp"
#include "bridge/settings_persistence.hpp"
#include "bridge/shared_scenario.hpp"
#include "bridge/tier_orchestrator.hpp"

#include "screens/screen_registration.hpp"

#include "math/interrogator.hpp"

#include "audio/audio.hpp"

#include "settings/settings.hpp"

#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"
#include "backbone/screen_state.hpp"

#include "assets/tier_loader.hpp"

#include "engine/scenario_id.hpp"

#include "persistence/idbfs.hpp"
#include "persistence/persistence_schema.hpp"

#include "theme/theme.hpp"

#include <memory>
#include <optional>

#include <emscripten/emscripten.h>

// Boot orchestration, held to -Wall -Wextra -Werror in bridge_platform.

namespace poker_trainer::bridge {

namespace {

std::unique_ptr<BridgeRuntime> g_runtime;

// App-lifetime state owned by boot, alongside g_runtime (the bridge's app-root
// state per CLAUDE.md §10): the production IDBFS storage backend, the Zone 04
// store over it, the persistence-backed Custom-weights store, and the Zone 07
// screen runtime (morph + popup + focus tracker) threaded into the render
// registry. Populated in finish_boot_after_persistence once the initial IDBFS
// sync completes.
struct BootState {
    std::unique_ptr<persistence::StorageBackend> storage;
    std::optional<persistence::IdbfsStore> store;
    std::optional<PersistentCustomWeightsStore> weights_store;
    screens::ScreensRuntime screens;
    // The app's live settings snapshot (the one source the scenario generator
    // reads), and the Zone 09 runtime (math inputs + grading) install reads from
    // it. SEAM(Z12): runtime settings mutation lands with the Settings page (W4);
    // until then this is the boot-time snapshot built from persisted state.
    settings::Settings live_settings;
    interrogator::InterrogatorRuntime interrogator;
};
BootState g_boot;

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

// Zone 02 bring-up: construct the asset registry + tier loader (Z05 owns the CDN
// fetch wrapper) and kick the boot-time tier loads. Runs in app_init, in parallel
// with the async IDBFS sync. The tier orchestrator owns the per-tier triggers and
// the SFX-into-MEMFS delivery (Tier 2 swoosh after Root renders, Tier 3 rest on
// Root -> Mode Selection); start_tiered_loading fires Tier 1 now and, on the
// shared-scenario route, force-fires Tier 2 / Tier 3 concurrently with it.
void init_assets(BootRoute route) {
    BridgeRuntime& rt = *g_runtime;
    rt.tier_loader = std::make_unique<assets::TierLoader>(
        rt.registry, make_cdn_fetch(), assets::make_png_decoder(),
        assets::make_steady_clock());
    start_tiered_loading(route);
}

// Second half of boot, run once the initial IDBFS FS.syncfs(true) has populated
// the mount (see begin_persistence_load / pt_boot_on_idbfs_ready). Loads the
// persisted guest state, applies the saved theme before the first frame, wires
// Zone 07's screens into the render registry, and starts the main loop.
void finish_boot_after_persistence() {
    // Zone 04 guest-mode load over the production IDBFS backend. Auth0 + server
    // sync are out of V1 client scope (CLAUDE.md §1), so boot does a guest-mode
    // IdbfsStore load only — the guest's local IDBFS is authoritative, there is
    // no session-start server reconcile to run. A corrupt / missing blob yields
    // fresh guest defaults rather than bricking the app.
    g_boot.storage = make_idbfs_storage_backend();
    g_boot.store.emplace(*g_boot.storage);
    const persistence::AppState state = g_boot.store->load_state();

    // Apply the persisted theme before the first frame (default No Limit when
    // nothing valid is saved). A boot responsibility: Zone 12's Settings page
    // changes the theme at runtime, but restoring the saved one at launch is the
    // boot path's job (it owns the persistence read and runs before any frame).
    theme::set_theme(read_persisted_theme_id(state));

    // Persistence-backed Custom-weights store (Mode Selection popup Save/Reset),
    // reading/writing the custom_*_weight settings through the interim codec.
    g_boot.weights_store.emplace(*g_boot.store);

    // Zone 07 self-registers its real Root / Mode Selection renders + handlers,
    // replacing the blank default in the dispatch registry.
    screens::install_screens(g_boot.screens, *g_boot.weights_store);

    // Build the app's live settings from persisted state. Until Zone 12 (W4) ships
    // the full settings serializer, only the theme id and the Custom split
    // round-trip (the interim codec); the rest are the documented Settings{}
    // defaults. This single snapshot is what the scenario generator reads, both at
    // launch and in Z09's fallback — one source of truth for settings.
    g_boot.live_settings = settings::Settings{};
    g_boot.live_settings.display.active_theme_id = read_persisted_theme_id(state);
    if (const auto custom = read_persisted_custom_weights(state)) {
        g_boot.live_settings.gameplay.custom_aggressor_weight = custom->aggressor_weight;
        g_boot.live_settings.gameplay.custom_caller_weight = custom->caller_weight;
    }
    const auto live_settings_source = [] { return g_boot.live_settings; };

    // Wire the LIVE settings into the launch path (scenario generation) and into
    // Zone 09 (its fallback regeneration), then install Zone 09: its Game-screen
    // render hook, the Enter-submit + number-key handlers, and the scenario_spawned
    // reset subscription. The math inputs are now live on the Game screen.
    set_launch_settings_source(live_settings_source);
    // Wire the Tier-2 navigation guard: request_game_launch consults the live
    // asset registry through this before transitioning to Game (a failed table /
    // card asset shows the Error screen; an in-flight one defers the launch).
    set_launch_asset_guard(game_launch_asset_readiness);
    g_boot.interrogator.settings_source = live_settings_source;
    // Wire the shared focus-reconciliation registry (owned off BridgeRuntime) into
    // Z09 so its inputs register their text/non-text reconcile behavior and the
    // render hook reconciles ImGui through the substrate.
    g_boot.interrogator.focus_registry = &g_runtime->focus_registry;
    interrogator::install_interrogator(g_boot.interrogator);

    // Install Zone 03: subscribe to scenario_spawned for the spawn audio
    // choreography (the per-frame audio_update + first-gesture autoplay gate are
    // wired in the main loop and platform input layer). No asset/persistence
    // dependency — Z03 loads audio by path and degrades gracefully when absent.
    audio::install_audio();

    // Register the tier orchestrator's per-frame tick: the navigation-gated Tier-2
    // (after Root renders) and Tier-3 (Root -> Mode Selection) loads, plus the
    // deferred-launch poll. Uses the register_frame_tick seam, so the main loop is
    // not edited as triggers land.
    install_tier_orchestrator();

    start_main_loop();
}

// Mount IDBFS and kick the initial load. The mount's data is empty until
// FS.syncfs(true) completes (asynchronously), so the rest of boot runs in the
// completion callback (pt_boot_on_idbfs_ready) — this is what lets the saved
// theme be applied before the first frame. EXIT_RUNTIME=0 keeps the runtime alive
// after app_init / main return, so the callback (and the RAF loop it starts) fire
// from the browser event loop.
void begin_persistence_load() {
    // clang-format off
    EM_ASM({
        var dir = UTF8ToString($0);
        try { FS.mkdir(dir); } catch (e) { /* already exists */ }
        FS.mount(IDBFS, {}, dir);
        FS.syncfs(true, function(err) {
            if (err) { console.warn('IDBFS initial load failed', err); }
            _pt_boot_on_idbfs_ready();
        });
    }, kIdbfsMountDir);
    // clang-format on
}

}  // namespace

BridgeRuntime& runtime() noexcept { return *g_runtime; }

// Exported for the IDBFS FS.syncfs(true) completion callback in
// begin_persistence_load (invoked as _pt_boot_on_idbfs_ready from JS).
// EMSCRIPTEN_KEEPALIVE forces the wasm export.
extern "C" EMSCRIPTEN_KEEPALIVE void pt_boot_on_idbfs_ready() {
    finish_boot_after_persistence();
}

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

    // Kick the asset load now so it downloads in parallel with the IDBFS sync.
    init_assets(g_runtime->route);

    // Versioned asset caching for returning visitors (best-effort).
    EM_ASM({
        if ('serviceWorker' in navigator) {
            navigator.serviceWorker.register('service_worker.js').catch(
                function() {});
        }
    });

    // Begin the async persistence load; finish_boot_after_persistence (theme +
    // Zone 07 screens + main loop) runs once the initial IDBFS sync completes.
    begin_persistence_load();
}

}  // namespace poker_trainer::bridge
