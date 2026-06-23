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

#include "screens/game_screen.hpp"
#include "screens/post_round_screen.hpp"
#include "screens/screen_registration.hpp"

#include "math/interrogator.hpp"

#include "modal/modals.hpp"

#include "audio/audio.hpp"
#include "audio/audio_paths.hpp"

#include "temporal/delta_timer.hpp"

#include "settings/account_modal.hpp"
#include "settings/settings.hpp"
#include "settings/settings_modal.hpp"

#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"
#include "backbone/screen_state.hpp"

#include "assets/tier_loader.hpp"

#include "engine/scenario_id.hpp"

#include "bridge/auth_outcome.hpp"

#include "persistence/auth.hpp"
#include "persistence/auth0_backend.hpp"
#include "persistence/clock.hpp"
#include "persistence/idbfs.hpp"
#include "persistence/persistence_schema.hpp"
#include "persistence/persistence_service.hpp"
#include "persistence/sync.hpp"

#include "theme/theme.hpp"

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <emscripten/emscripten.h>

// Boot orchestration, held to -Wall -Wextra -Werror in bridge_platform.

namespace poker_trainer::bridge {

namespace {

std::unique_ptr<BridgeRuntime> g_runtime;

// SEAM(server sync): the server-side app-state backend does not exist yet (same as the
// leaderboard). This local-only stub keeps AUTH non-blocking: fetch reports NotFound so a
// sign-in/up seeds from local state (identity association + the local migration build run),
// and push / upload_initial report success so the reconcile gate opens without a real
// server. NOTHING is actually uploaded — the server upload is the stubbed part. Replace
// with the real authenticated-HTTP SyncBackend when the backend lands.
struct LocalOnlySyncBackend final : persistence::SyncBackend {
    [[nodiscard]] persistence::FetchResult fetch(std::string_view /*user*/) override {
        return persistence::FetchResult{persistence::FetchOutcome::NotFound,
                                        persistence::AppState{}};
    }
    [[nodiscard]] bool push(std::string_view /*user*/,
                            std::span<const persistence::AppState> /*writes*/) override {
        return true;
    }
    [[nodiscard]] bool upload_initial(
        std::string_view /*user*/,
        const persistence::AccountMigrationState& /*initial*/) override {
        return true;
    }
};

// App-lifetime state owned by boot, alongside g_runtime (the bridge's app-root
// state per CLAUDE.md §10): the production IDBFS storage backend, the Zone 04
// store over it, the persistence-backed Custom-weights store, and the Zone 07
// screen runtime (morph + popup + focus tracker) threaded into the render
// registry. Populated in finish_boot_after_persistence once the initial IDBFS
// sync completes.
struct BootState {
    // Zone 04 persistence stack. The PersistenceService is the single owner of the IDBFS
    // store; it is wired over the production storage backend, the real Auth0 backend, the
    // local-only sync stub, and the steady clock. The auth/sync backends + clock must
    // outlive the service (it holds references), so they live here alongside it.
    std::unique_ptr<persistence::StorageBackend> storage;
    persistence::SteadyClock clock;
    std::optional<persistence::Auth0Backend> auth_backend;
    LocalOnlySyncBackend sync_backend;
    std::optional<persistence::PersistenceService> service;
    std::optional<PersistentCustomWeightsStore> weights_store;
    screens::ScreensRuntime screens;
    // The app's live settings snapshot (the one source the scenario generator
    // reads), and the Zone 09 runtime (math inputs + grading) install reads from
    // it. SEAM(Z12): runtime settings mutation lands with the Settings page (W4);
    // until then this is the boot-time snapshot built from persisted state.
    settings::Settings live_settings;
    interrogator::InterrogatorRuntime interrogator;
    // Zone 08 Game-screen runtime (Frog state + chip-push spawn timestamp + the
    // live-settings source it reads for Show/Hide HUD, Units, and the chip
    // denomination mode). Threaded into install_game_screen below.
    screens::GameScreenRuntime game_screen;
    // Zone 13 Post-Round runtime (captured recap snapshot + Again machine +
    // clipboard fallback + active recap tab + the live-settings source it reads for
    // the Recap dealer-arrival toggle and default tab). Threaded into
    // install_post_round_screen below.
    screens::PostRoundRuntime post_round;
    // Zone 11 modal infrastructure runtime (banner state, active confirm spec,
    // cluster geometry cache). install_modals registers the overlay renderer + the
    // modal/cluster event handlers and stores a pointer to this.
    modal::ModalRuntime modals;
    // Zone 12 Settings page view-state. Wired with the mutable live-settings handle,
    // the persist + apply-audio + reset-tomatoes callbacks, and the shared focus
    // registry; install_settings_content registers its content provider with Zone 11.
    settings::SettingsModalState settings;
    // Zone 12 Account section auth seams + Sign In / Sign Up modal state. The read seams
    // (account identity + wallet) are wired to the store below; the auth-OPERATION seams
    // (sign in/up/out, delete, reset) are a Zone 04 / Zone 05 integration SEAM — see the
    // wiring block in finish_boot_after_persistence.
    settings::AccountModalState account;
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
    // Modal-state observer: zero-initialized global state (Zone 11's modal_state.cpp
    // supplies the stack). Its query/notify API needs no boot-time call here; Zone
    // 11's overlay renderer + event handlers are installed in
    // finish_boot_after_persistence (install_modals), after the screens.
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
    // Zone 04 persistence service: the single owner of the IDBFS store, wired over the
    // production storage backend, the real Auth0 backend (embedded login), the local-only
    // sync stub, and the steady clock. Boot loads the persisted state (guest on first
    // launch); a corrupt / missing blob yields fresh defaults rather than bricking the app.
    // Server-side reconcile only runs once a user signs in (it is a no-op for guests).
    g_boot.storage = make_idbfs_storage_backend();
    g_boot.auth_backend.emplace();
    g_boot.service.emplace(*g_boot.storage, *g_boot.auth_backend, g_boot.sync_backend,
                           g_boot.clock);
    const persistence::AppState state = g_boot.service->load_state();

    // Build the app's live settings from persisted state via Zone 12's full codec
    // ('PTS1'), migrating a legacy interim blob (theme + custom split) when present.
    // This single snapshot is the one source the scenario generator + every consumer
    // reads; Zone 12's Settings page mutates it in place through &g_boot.live_settings.
    g_boot.live_settings = read_persisted_settings(state);

    // Apply the persisted theme before the first frame (default No Limit when nothing
    // valid is saved). A boot responsibility: Zone 12 changes the theme at runtime, but
    // restoring the saved one at launch is the boot path's job (it owns the persistence
    // read and runs before any frame).
    theme::set_theme(g_boot.live_settings.display.active_theme_id);

    // Persistence-backed Custom-weights store (Mode Selection popup Save/Reset),
    // reading/writing the custom_*_weight settings through the full settings codec. The
    // service owns the store; this borrows the raw handle the weights store needs.
    g_boot.weights_store.emplace(g_boot.service->store());

    // Zone 07 self-registers its real Root / Mode Selection renders + handlers,
    // replacing the blank default in the dispatch registry.
    screens::install_screens(g_boot.screens, *g_boot.weights_store);

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
    // SEAM(Z10): Zone 10's Delta Timer supplies the elapsed-time source Z09 reads at
    // submit (for the GradingComplete payload and the math-and-time pass conjunction).
    g_boot.interrogator.elapsed_ms_source =
        [] { return static_cast<std::uint32_t>(temporal::timer_elapsed_ms()); };
    // Wire the shared focus-reconciliation registry (owned off BridgeRuntime) into
    // Z09 so its inputs register their text/non-text reconcile behavior and the
    // render hook reconciles ImGui through the substrate.
    g_boot.interrogator.focus_registry = &g_runtime->focus_registry;
    interrogator::install_interrogator(g_boot.interrogator);

    // Install Zone 10 (Temporal): wires its live-settings source, the scenario-
    // lifecycle subscriptions (start on spawn / freeze on submit / clear on exit),
    // and the per-frame timer_update tick. Reads the same single live-settings source.
    temporal::install_temporal(live_settings_source);

    // Install Zone 08 AFTER Zone 09: the render-dispatch registry is single-slot
    // last-writer-wins, so Z08's Game renderer must register last to take over the
    // slot (it composes Z09 by calling render_math_inputs itself). Z08 reads the
    // same live settings source for Show/Hide HUD, Units, and the denomination mode.
    g_boot.game_screen.settings_source = live_settings_source;
    screens::install_game_screen(g_boot.game_screen, g_boot.interrogator);

    // Install Zone 13: it takes ScreenId::PostRound's renderer slot (no conflict
    // with Z08's Game slot), subscribes to GradingComplete to capture Z09's grading
    // result + drive the transition into Post-Round, and reads the Recap settings
    // (dealer-arrival animation + default tab) from the same live source.
    g_boot.post_round.settings_source = live_settings_source;
    screens::install_post_round_screen(g_boot.post_round, g_boot.interrogator);

    // Install Zone 11 AFTER every screen: it registers the modal + outage-banner
    // overlay renderer (drawn above the active screen each frame) and its event
    // handlers. The cluster keyboard handler is ScreenContext and must register after
    // the screens' own activate handlers, which leave a focused cluster icon's key
    // unconsumed so this one activates it. It reaches the shared focus registry (off
    // BridgeRuntime) for the text-field modals (leaderboard search).
    g_boot.modals.focus_registry = &g_runtime->focus_registry;
    modal::install_modals(g_boot.modals);

    // Install Zone 12 (Settings page) AFTER Zone 11: it registers its content provider
    // with the modal runtime (so the cog's Settings modal renders the Z12 body instead
    // of the placeholder shell). Wire the seams: the mutable live-settings handle, the
    // autosave (full-codec write-through to IDBFS), the immediate audio apply (Z03), the
    // tomato-wallet reset, and the shared focus registry.
    g_boot.settings.focus_registry = &g_runtime->focus_registry;
    g_boot.settings.live = &g_boot.live_settings;
    g_boot.settings.persist = [] {
        if (g_boot.service.has_value()) {
            g_boot.service->save_state(
                with_settings(g_boot.service->state(), g_boot.live_settings));
        }
    };
    g_boot.settings.apply_audio = [](const settings::AudioSettings& a) {
        audio::set_volume(static_cast<int>(a.volume));
        audio::set_mute_all(a.mute_all);
        audio::set_mute_sfx(a.mute_sfx);
        audio::set_mute_music(a.mute_music);
        audio::set_active_genre(
            static_cast<audio::MusicGenre>(static_cast<std::uint8_t>(a.current_music_genre)));
    };
    g_boot.settings.reset_tomatoes = [] {
        if (g_boot.service.has_value()) {
            persistence::AppState next = g_boot.service->state();
            next.tomatoes = persistence::TomatoesState{};  // wipe spendable + lifetime
            g_boot.service->save_state(next);
        }
    };
    settings::install_settings_content(g_boot.settings);

    // Zone 12 Account section + Sign In / Sign Up modal. The READ seams reflect the live
    // account identity + wallet from the store (guest until a sign-in), so the Account
    // section renders the true logged-out/logged-in state and View Profile shows the live
    // Tomatoes totals.
    g_boot.settings.account = &g_boot.account;
    g_boot.account.seams.account = [] {
        settings::AccountSnapshot snap{};
        if (g_boot.service.has_value()) {
            const persistence::AccountState& a = g_boot.service->state().account;
            snap.is_authenticated = a.is_authenticated;
            snap.display_name = a.display_name;
            snap.email = a.email;
        }
        return snap;
    };
    g_boot.account.seams.wallet = [] {
        settings::WalletSnapshot w{};
        if (g_boot.service.has_value()) {
            const persistence::TomatoesState& t = g_boot.service->state().tomatoes;
            w.spendable = t.spendable;
            w.lifetime = t.lifetime;
        }
        return w;
    };

    // The seven auth-operation seams, pointed at the PersistenceService (real Auth0 backend).
    // sign_in / sign_up return the categorized AuthError; bridge::to_auth_outcome translates
    // it to the AuthOutcome the form layer renders. The guest->account migration runs inside
    // Z04's sign-in/up reconcile (server upload stubbed — see LocalOnlySyncBackend). The
    // void operations ignore their result here (the UI seams are void).
    g_boot.account.seams.health_check = [] {
        return g_boot.service.has_value() && g_boot.service->auth0_health_check();
    };
    g_boot.account.seams.sign_in = [](std::string_view id, std::string_view pw) {
        if (!g_boot.service.has_value()) {
            return settings::AuthOutcome::ServiceUnavailable;
        }
        const std::expected<void, persistence::AuthError> r =
            g_boot.service->sign_in(persistence::AuthCredentials{std::string{id}, std::string{pw}});
        return r.has_value() ? settings::AuthOutcome::Success
                             : bridge::to_auth_outcome(r.error());
    };
    g_boot.account.seams.sign_up = [](std::string_view username, std::string_view email,
                                      std::string_view pw) {
        if (!g_boot.service.has_value()) {
            return settings::AuthOutcome::ServiceUnavailable;
        }
        const std::expected<void, persistence::AuthError> r = g_boot.service->sign_up(
            persistence::AuthCredentials{std::string{email}, std::string{pw}}, username);
        return r.has_value() ? settings::AuthOutcome::Success
                             : bridge::to_auth_outcome(r.error());
    };
    g_boot.account.seams.sign_out = [] {
        if (g_boot.service.has_value()) {
            static_cast<void>(g_boot.service->sign_out());
        }
    };
    g_boot.account.seams.delete_account = [] {
        if (g_boot.service.has_value()) {
            static_cast<void>(g_boot.service->delete_account());
        }
    };
    g_boot.account.seams.change_password = [] {
        if (g_boot.service.has_value()) {
            static_cast<void>(g_boot.service->change_password());
        }
    };
    g_boot.account.seams.reset_password = [](std::string_view email) {
        if (g_boot.service.has_value()) {
            static_cast<void>(g_boot.service->send_password_reset(email));
        }
    };
    // install_account_content wires the consent links to the legal-doc modal and registers
    // the kAuthModalId content provider with Zone 11.
    settings::install_account_content(g_boot.account, g_boot.settings);

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
