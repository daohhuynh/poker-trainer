// Phase 0 integration test: the sign-off gate.
//
// Includes every Phase 0 header, links every Phase 0 .cpp, and exercises a
// representative behavior subset of each backbone primitive plus a sanity
// check on every contract-bearing constant. Plain C++ assertions (no
// GoogleTest dependency) keep Phase 0 free of extra third-party deps; the
// emcc command line in the build script links this file directly against
// the Phase 0 .cpps.
//
// The six backbone reset_<primitive>_for_testing() functions are renamed
// from the original `reset_for_testing` (which collided across primitives
// at link time) per the per-primitive resolution chosen during sign-off.

// Force assertions live regardless of optimizer/build flags.
#undef NDEBUG

#include "engine/scenario_id.hpp"
#include "engine/rng_seed.hpp"
#include "persistence/auth0_config.hpp"
#include "persistence/sync_state.hpp"
#include "persistence/persistence_schema.hpp"
#include "audio/audio_paths.hpp"
#include "assets/tier_config.hpp"
#include "assets/asset_paths.hpp"
#include "theme/theme_tokens.hpp"
#include "settings/settings.hpp"
#include "backbone/animation_clock.hpp"
#include "backbone/screen_state.hpp"
#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"
#include "backbone/scenario_events.hpp"
#include "backbone/modal_state.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <optional>

// Compile-time FNV-1a hash sanity check. If this static_assert fires,
// every FocusableId generated downstream is corrupt — the build fails
// here rather than silently producing wrong IDs at runtime.
static_assert(poker_trainer::backbone::make_focusable_id("test").value
                  == 0xf9e6e6ef197c2b25ULL,
              "FNV-1a hash mismatch in focus_manager::make_focusable_id");

namespace pt = poker_trainer;

static void test_scenario_id() {
    const pt::engine::ScenarioId id{4729183746281ULL};
    assert(pt::engine::is_valid(id));
    assert(pt::engine::format_scenario_id(id) == "4729183746281");
    const auto parsed = pt::engine::parse_scenario_id("4729183746281");
    assert(parsed.has_value());
    assert(*parsed == id);

    assert(!pt::engine::parse_scenario_id("0").has_value());
    assert(!pt::engine::parse_scenario_id("abc").has_value());
    assert(!pt::engine::parse_scenario_id("").has_value());
}

static void test_rng_seed() {
    pt::engine::RngSeed rng{pt::engine::ScenarioId{42}};
    assert(rng.seed_id() == pt::engine::ScenarioId{42});
    const auto v1 = rng.engine()();
    const auto v2 = rng.engine()();
    assert(v1 != v2);
}

static void test_auth0_config() {
    assert(!pt::persistence::kAuth0Domain.empty());
    assert(!pt::persistence::kAuth0RedirectUri.empty());
    assert(pt::persistence::kAuth0HealthCheckTimeout.count() > 0);
}

static void test_sync_state() {
    pt::persistence::write_sync_state({});
    auto s = pt::persistence::read_sync_state();
    assert(s.status == pt::persistence::SyncStatus::Idle);

    pt::persistence::SyncStateSnapshot updated{};
    updated.status = pt::persistence::SyncStatus::SyncFailing;
    updated.consecutive_failures = 3;
    pt::persistence::write_sync_state(updated);

    s = pt::persistence::read_sync_state();
    assert(s.status == pt::persistence::SyncStatus::SyncFailing);
    assert(s.consecutive_failures == 3u);

    // Restore default for any later test.
    pt::persistence::write_sync_state({});
}

static void test_persistence_schema() {
    using pt::persistence::validate_schema_version;
    using pt::persistence::SchemaValidationResult;
    assert(validate_schema_version(1) == SchemaValidationResult::Ok);
    assert(validate_schema_version(999) == SchemaValidationResult::Unsupported);

    pt::persistence::AppState state{};
    assert(state.schema_version == pt::persistence::kCurrentSchemaVersion);
    assert(pt::persistence::is_guest_state(state));
}

static void test_audio_paths() {
    assert(pt::audio::kSfxCount > 0);
    for (std::size_t i = 0; i < pt::audio::kSfxCount; ++i) {
        assert(!pt::audio::kSfxPaths[i].empty());
    }
    assert(pt::audio::kMusicTrackCount == 12u);
    assert(pt::audio::kMusicGenreCount == 4u);
}

static void test_asset_paths_and_tier_config() {
    assert(pt::assets::kAssetCount == 90u);
    assert(pt::assets::asset_tier(pt::assets::AssetId::AppLogo)
               == pt::assets::AssetTier::Tier1);
    assert(pt::assets::asset_tier(pt::assets::AssetId::FrogSideProfile)
               == pt::assets::AssetTier::Tier4);

    assert(pt::assets::tier_config(pt::assets::AssetTier::Tier1).max_retries
               == 3);
    assert(pt::assets::tier_config(pt::assets::AssetTier::Tier1)
               .fatal_failure_shows_error_screen);
    assert(!pt::assets::tier_config(pt::assets::AssetTier::Tier3)
               .fatal_failure_shows_error_screen);
}

static void test_theme_tokens() {
    // accent_secondary was appended to the enum post-seal (additive). The
    // count grew 61 -> 62; the pre-existing tokens kept their integer values.
    assert(pt::theme::kColorTokenCount == 62u);
    assert(pt::theme::kThemeIdCount == 4u);
    assert(pt::theme::kFixedAcrossThemeTokens.size() == 10u);

    // Additivity guarantees: no pre-existing token renumbered, the new token
    // sits at the end, and the sentinel moved to 62.
    using pt::theme::ColorToken;
    assert(static_cast<std::uint16_t>(ColorToken::DealerButtonGreen) == 60u);
    assert(static_cast<std::uint16_t>(ColorToken::AccentSecondary) == 61u);
    assert(static_cast<std::uint16_t>(ColorToken::Count) == 62u);
}

static void test_settings_defaults() {
    pt::settings::Settings s{};
    assert(s.display.active_theme_id == pt::theme::kThemeIdNoLimit);
    assert(s.display.show_hud);
    assert(s.gameplay.scenario_types_enabled[0]);
    assert(s.gameplay.scenario_types_enabled[1]);
    assert(s.gameplay.difficulty_min == 0.2f);
    assert(s.gameplay.difficulty_max == 0.8f);
    assert(s.account.display_name_override.empty());
}

static void test_animation_clock() {
    pt::backbone::reset_animation_clock_for_testing();
    // Stub implementation: wall_clock_ms() and animation_time_ms() return 0,
    // is_animation_paused() always returns false. Pause/resume/tick are
    // no-ops in the stub. The real implementation lives in Z05.
    assert(pt::backbone::wall_clock_ms() == 0u);
    assert(pt::backbone::animation_time_ms() == 0u);
    assert(!pt::backbone::is_animation_paused());

    // Drive the API surface to prove it links; the stub does not honor
    // the calls but they must be callable.
    pt::backbone::tick(100);
    pt::backbone::pause();
    pt::backbone::resume();
}

static void test_modal_state() {
    pt::backbone::reset_modal_state_for_testing();
    assert(!pt::backbone::is_any_modal_open());
    assert(pt::backbone::modal_stack_depth() == 0u);
    assert(pt::backbone::topmost_modal() == pt::backbone::kNoModal);

    pt::backbone::notify_modal_opened(pt::backbone::ModalId{42});
    assert(pt::backbone::is_any_modal_open());
    assert(pt::backbone::modal_stack_depth() == 1u);
    assert(pt::backbone::topmost_modal().value == 42u);

    pt::backbone::notify_modal_opened(pt::backbone::ModalId{7});
    assert(pt::backbone::modal_stack_depth() == 2u);

    pt::backbone::notify_modal_closed(pt::backbone::ModalId{7});
    pt::backbone::notify_modal_closed(pt::backbone::ModalId{42});
    assert(!pt::backbone::is_any_modal_open());
    assert(pt::backbone::modal_stack_depth() == 0u);
}

static void test_screen_state() {
    pt::backbone::reset_screen_state_for_testing();
    auto snap = pt::backbone::read_screen_state();
    assert(snap.current == pt::backbone::ScreenId::Root);
    assert(!snap.active_scenario.has_value());
    assert(!pt::backbone::is_in_scenario());

    pt::backbone::set_screen(pt::backbone::ScreenId::ModeSelection,
                             std::nullopt);
    snap = pt::backbone::read_screen_state();
    assert(snap.current == pt::backbone::ScreenId::ModeSelection);
    assert(!snap.active_scenario.has_value());

    pt::backbone::set_screen(pt::backbone::ScreenId::Game,
                             pt::engine::ScenarioId{12345});
    snap = pt::backbone::read_screen_state();
    assert(snap.current == pt::backbone::ScreenId::Game);
    assert(snap.active_scenario.has_value());
    assert(*snap.active_scenario == pt::engine::ScenarioId{12345});
    assert(pt::backbone::is_in_scenario());

    // Per the §12 contract: transitioning back to Root clears active_scenario
    // even if a value was passed.
    pt::backbone::set_screen(pt::backbone::ScreenId::Root, std::nullopt);
    snap = pt::backbone::read_screen_state();
    assert(snap.current == pt::backbone::ScreenId::Root);
    assert(!snap.active_scenario.has_value());
    assert(!pt::backbone::is_in_scenario());

    pt::backbone::set_screen(pt::backbone::ScreenId::PostRound,
                             pt::engine::ScenarioId{12345});
    snap = pt::backbone::read_screen_state();
    assert(snap.current == pt::backbone::ScreenId::PostRound);
    assert(*snap.active_scenario == pt::engine::ScenarioId{12345});

    pt::backbone::set_screen(pt::backbone::ScreenId::Error, std::nullopt);
    snap = pt::backbone::read_screen_state();
    assert(snap.current == pt::backbone::ScreenId::Error);
    assert(!snap.active_scenario.has_value());
}

static void test_event_router() {
    pt::backbone::reset_event_router_for_testing();

    bool top_fired = false;
    bool bottom_fired = false;

    // Install BackgroundCatchAll first; it will run last in priority order.
    const auto bottom_handle = pt::backbone::install_key_handler(
        [&](const pt::backbone::KeyEvent&) {
            bottom_fired = true;
            return true;  // Would consume.
        },
        pt::backbone::HandlerPriority::BackgroundCatchAll,
        "test.bottom");

    // Install ModalLayer; it runs first and consumes.
    const auto top_handle = pt::backbone::install_key_handler(
        [&](const pt::backbone::KeyEvent& e) {
            if (e.code == pt::backbone::KeyCode::Enter) {
                top_fired = true;
                return true;
            }
            return false;
        },
        pt::backbone::HandlerPriority::ModalLayer,
        "test.top");

    pt::backbone::dispatch_key_event({
        pt::backbone::KeyEventType::KeyDown,
        pt::backbone::KeyCode::Enter,
        pt::backbone::ModMask::None,
    });
    assert(top_fired);
    assert(!bottom_fired);  // Top consumed; bottom never ran.

    // Uninstall top; now bottom should fire.
    pt::backbone::uninstall_handler(top_handle);
    top_fired = false;
    pt::backbone::dispatch_key_event({
        pt::backbone::KeyEventType::KeyDown,
        pt::backbone::KeyCode::Enter,
        pt::backbone::ModMask::None,
    });
    assert(!top_fired);
    assert(bottom_fired);

    // Uninstall bottom; no handler should fire.
    pt::backbone::uninstall_handler(bottom_handle);
    bottom_fired = false;
    pt::backbone::dispatch_key_event({
        pt::backbone::KeyEventType::KeyDown,
        pt::backbone::KeyCode::Enter,
        pt::backbone::ModMask::None,
    });
    assert(!bottom_fired);
}

static void test_scenario_events() {
    pt::backbone::reset_scenario_events_for_testing();

    bool spawned_received = false;
    pt::engine::ScenarioId spawned_id{0};
    const auto spawn_handle = pt::backbone::subscribe_scenario_spawned(
        [&](const pt::backbone::ScenarioSpawnedEvent& e) {
            spawned_received = true;
            spawned_id = e.scenario_id;
        },
        "test.spawn_subscriber");

    pt::backbone::fire_scenario_spawned({pt::engine::ScenarioId{99}});
    assert(spawned_received);
    assert(spawned_id == pt::engine::ScenarioId{99});

    pt::backbone::unsubscribe(spawn_handle);
    spawned_received = false;
    pt::backbone::fire_scenario_spawned({pt::engine::ScenarioId{100}});
    assert(!spawned_received);  // Unsubscribed; should not fire.

    // Cross-event-type isolation: a scenario_spawned subscriber should NOT
    // receive an answers_submitted fire (and vice versa).
    bool other_received = false;
    const auto other_handle = pt::backbone::subscribe_answers_submitted(
        [&](const pt::backbone::AnswersSubmittedEvent&) {
            other_received = true;
        },
        "test.other_subscriber");
    pt::backbone::fire_scenario_spawned({pt::engine::ScenarioId{101}});
    assert(!other_received);

    pt::backbone::fire_answers_submitted({pt::engine::ScenarioId{102}});
    assert(other_received);

    pt::backbone::unsubscribe(other_handle);
}

static void test_focus_manager() {
    pt::backbone::reset_focus_manager_for_testing();

    // Initial state: no focus, keyboard mode inactive, no contexts.
    assert(pt::backbone::current_focus() == pt::backbone::kNoFocus);
    assert(!pt::backbone::is_keyboard_mode_active());
    assert(pt::backbone::context_depth() == 0u);

    pt::backbone::activate_keyboard_mode();
    assert(pt::backbone::is_keyboard_mode_active());

    constexpr auto kA = pt::backbone::make_focusable_id("test.a");
    constexpr auto kB = pt::backbone::make_focusable_id("test.b");
    constexpr auto kC = pt::backbone::make_focusable_id("test.c");

    const pt::backbone::FocusableId base_list[] = {kA, kB, kC};
    pt::backbone::register_focus_list(base_list);
    assert(pt::backbone::current_focus() == kA);

    pt::backbone::advance_focus(false);  // forward
    assert(pt::backbone::current_focus() == kB);

    pt::backbone::advance_focus(true);  // reverse
    assert(pt::backbone::current_focus() == kA);

    pt::backbone::snap_focus_to(kC);
    assert(pt::backbone::current_focus() == kC);

    // Push a new context for a modal.
    constexpr auto kJ = pt::backbone::make_focusable_id("modal.j");
    constexpr auto kK = pt::backbone::make_focusable_id("modal.k");
    const pt::backbone::FocusableId modal_list[] = {kJ, kK};
    pt::backbone::push_focus_context(modal_list, kJ, "test.modal_ctx");
    assert(pt::backbone::current_focus() == kJ);
    assert(pt::backbone::context_depth() == 1u);

    pt::backbone::advance_focus(false);
    assert(pt::backbone::current_focus() == kK);

    // Pop: the prior context (with focus on kC) is restored.
    pt::backbone::pop_focus_context();
    assert(pt::backbone::current_focus() == kC);
    assert(pt::backbone::context_depth() == 0u);
}

int main() {
    test_scenario_id();
    test_rng_seed();
    test_auth0_config();
    test_sync_state();
    test_persistence_schema();
    test_audio_paths();
    test_asset_paths_and_tier_config();
    test_theme_tokens();
    test_settings_defaults();
    test_animation_clock();
    test_modal_state();
    test_screen_state();
    test_event_router();
    test_scenario_events();
    test_focus_manager();

    std::printf("all_headers_test: all assertions passed\n");
    return 0;
}
