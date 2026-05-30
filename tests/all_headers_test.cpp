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
    // SFX set per the amended contract (AU1): Card Deal, Button Click
    // Confirmation, Chip Push, Side Pot Split, Modal Open/Close Swoosh, Frog
    // Toggle, Slide In, Slide Out. The architecturally forbidden Pass/Fail
    // performance-feedback cues (and the stray ChipLand) are gone; the count
    // coincidentally stays 9 but the membership is the corrected set.
    assert(pt::audio::kSfxCount == 9u);
    assert(static_cast<std::uint8_t>(pt::audio::SfxId::CardDeal) == 0);
    assert(static_cast<std::uint8_t>(pt::audio::SfxId::ButtonClickConfirmation) == 1);
    assert(static_cast<std::uint8_t>(pt::audio::SfxId::ChipPush) == 2);
    assert(static_cast<std::uint8_t>(pt::audio::SfxId::SidePotSplit) == 3);
    assert(static_cast<std::uint8_t>(pt::audio::SfxId::ModalSwooshOpen) == 4);
    assert(static_cast<std::uint8_t>(pt::audio::SfxId::ModalSwooshClose) == 5);
    assert(static_cast<std::uint8_t>(pt::audio::SfxId::FrogToggle) == 6);
    assert(static_cast<std::uint8_t>(pt::audio::SfxId::SlideIn) == 7);
    assert(static_cast<std::uint8_t>(pt::audio::SfxId::SlideOut) == 8);
    for (std::size_t i = 0; i < pt::audio::kSfxCount; ++i) {
        assert(!pt::audio::kSfxPaths[i].empty());
    }
    assert(pt::audio::sfx_path(pt::audio::SfxId::CardDeal)
               == "assets/audio/sfx/card_deal.ogg");

    assert(pt::audio::kMusicTrackCount == 12u);
    assert(pt::audio::kMusicGenreCount == 4u);

    // Track price is denominated in tomatoes (AU2), not USD cents: starters
    // are free, paid unlocks cost 25 tomatoes.
    const auto& starter =
        pt::audio::music_track_info(pt::audio::MusicTrackId::LoungeJazz_Starter);
    assert(starter.is_starter);
    assert(starter.price_tomatoes == 0u);
    const auto& paid =
        pt::audio::music_track_info(pt::audio::MusicTrackId::LoungeJazz_Track2);
    assert(!paid.is_starter);
    assert(paid.price_tomatoes == 25u);
}

static void test_asset_paths_and_tier_config() {
    // kAssetCount recomputed (A10) after the Frog set collapsed to 3
    // (frog_base + pass + fail), the per-theme Root backgrounds collapsed to
    // theme-independent blur variants, and the missing Tier-2 glyphs (exit /
    // copy / share / tomato / side-pot chip) were added. The header
    // static_assert pins it to the enum; mirror the value here.
    assert(pt::assets::kAssetCount == 84u);

    // Tier-1 synchronous set includes the front-facing Butler (A3) and the
    // Home icon (A4), both promoted from Tier 2.
    assert(pt::assets::asset_tier(pt::assets::AssetId::AppLogo)
               == pt::assets::AssetTier::Tier1);
    assert(pt::assets::asset_tier(pt::assets::AssetId::ButlerNeutral)
               == pt::assets::AssetTier::Tier1);
    assert(pt::assets::asset_tier(pt::assets::AssetId::IconHome)
               == pt::assets::AssetTier::Tier1);

    // Frog easter-egg set (A2) is the on-demand Tier-4 base plus two
    // expression overlays; the side-profile / overtime / perfect variants
    // were removed.
    assert(pt::assets::asset_tier(pt::assets::AssetId::FrogBase)
               == pt::assets::AssetTier::Tier4);
    assert(pt::assets::asset_tier(pt::assets::AssetId::FrogExpressionPass)
               == pt::assets::AssetTier::Tier4);
    assert(pt::assets::asset_tier(pt::assets::AssetId::FrogExpressionFail)
               == pt::assets::AssetTier::Tier4);

    // The table-side all-in marker is re-tiered to Tier 2 (A8).
    assert(pt::assets::asset_tier(pt::assets::AssetId::SidePotAllInMarker)
               == pt::assets::AssetTier::Tier2);

    // Retry schedule is the explicit [immediate, 2s, 10s] list, identical
    // across all four tiers (C1). Fatal-failure handling is a tri-state
    // policy (C3): immediate error screen (Tier 1), deferred error screen on
    // use (Tier 2), or silent degrade (Tier 3/4) — no longer a single bool.
    assert(pt::assets::tier_config(pt::assets::AssetTier::Tier1).max_retries == 3);
    assert(pt::assets::tier_config(pt::assets::AssetTier::Tier1).retry_delays
               == pt::assets::kRetryBackoff);
    assert(pt::assets::tier_config(pt::assets::AssetTier::Tier1).fatal_failure_policy
               == pt::assets::FatalFailurePolicy::ErrorScreenImmediate);
    assert(pt::assets::tier_config(pt::assets::AssetTier::Tier2).fatal_failure_policy
               == pt::assets::FatalFailurePolicy::ErrorScreenOnUse);
    assert(pt::assets::tier_config(pt::assets::AssetTier::Tier3).fatal_failure_policy
               == pt::assets::FatalFailurePolicy::Silent);
    assert(pt::assets::tier_config(pt::assets::AssetTier::Tier4).fatal_failure_policy
               == pt::assets::FatalFailurePolicy::Silent);
}

static void test_theme_tokens() {
    // Tokens were reshaped to the architecture's closed, semantic palette
    // (T1-T10): the ~27 element-specific tokens were removed in favor of the
    // semantic set, the three per-screen backgrounds collapsed to a single
    // BgPrimary (T3, no per-screen overrides), and the required text_button /
    // text_placeholder tokens were added (T5/T6). The set is now 32.
    assert(pt::theme::kColorTokenCount == 32u);
    assert(pt::theme::kThemeIdCount == 4u);
    assert(pt::theme::kFixedAcrossThemeTokens.size() == 10u);

    using pt::theme::ColorToken;
    // Single global background tint (T3): one BgPrimary at index 0.
    assert(static_cast<std::uint16_t>(ColorToken::BgPrimary) == 0);
    // Added semantic tokens (T5/T6).
    assert(static_cast<std::uint16_t>(ColorToken::TextButton) == 12);
    assert(static_cast<std::uint16_t>(ColorToken::TextPlaceholder) == 13);
    // Complementary accent (Leaderboard row tint).
    assert(static_cast<std::uint16_t>(ColorToken::AccentSecondary) == 16);
    // The fixed-across-themes chip / dealer-button block sits at the tail,
    // immediately before the sentinel.
    assert(static_cast<std::uint16_t>(ColorToken::ChipWhite) == 22);
    assert(static_cast<std::uint16_t>(ColorToken::DealerButtonGreen) == 31);
    assert(static_cast<std::uint16_t>(ColorToken::Count) == 32);
}

static void test_settings_defaults() {
    pt::settings::Settings s{};

    // Gameplay — seed-encoded street split defaults 15/35/30/20 (S1); the
    // StreetWeightPreset enum was removed in favor of four coupled weights.
    assert(s.gameplay.street_weight_preflop == 15);
    assert(s.gameplay.street_weight_flop == 35);
    assert(s.gameplay.street_weight_turn == 30);
    assert(s.gameplay.street_weight_river == 20);
    // Custom-mode Aggressor/Caller weights default 50/50 (S10).
    assert(s.gameplay.custom_aggressor_weight == 50);
    assert(s.gameplay.custom_caller_weight == 50);
    // Side-pot frequency ~10% (S9); bet sizing engine ON (S8); chip
    // denomination Stake-scaled (S2).
    assert(s.gameplay.side_pot_frequency == 0.10f);
    assert(s.gameplay.bet_sizing_engine_enabled);
    assert(s.gameplay.chip_denomination_mode
               == pt::settings::ChipDenominationMode::StakeScaled);
    assert(s.gameplay.difficulty_min == 0.2f);
    assert(s.gameplay.difficulty_max == 0.8f);
    // Time pressure — street-scaled default with the flat custom override off
    // by default; custom value 30s within the corrected 1-300 range (S4/S5).
    assert(!s.gameplay.time_pressure_custom_enabled);
    assert(s.gameplay.time_pressure_custom_seconds == 30);
    // HUD shown by default; Visual Countdown OFF by default (S3). show_hud is
    // a Gameplay control now, not Display.
    assert(s.gameplay.show_hud);
    assert(!s.gameplay.show_countdown);

    // Units.
    assert(s.units.cash_mode);

    // Display — theme plus the three motion toggles (S11/S12/S13).
    assert(s.display.active_theme_id == pt::theme::kThemeIdNoLimit);
    assert(!s.display.reduce_motion);                   // Reduce Motion OFF
    assert(s.display.background_atmospheric_movement);  // ambient drift ON
    assert(s.display.particle_drift);                   // particle drift ON

    // Audio — single Volume slider default 50 (S7); the three mutes default off.
    assert(s.audio.volume == 50);
    assert(s.audio.current_music_genre
               == pt::settings::ActiveMusicGenre::LoungeJazz);
    assert(!s.audio.mute_all);
    assert(!s.audio.mute_sfx);
    assert(!s.audio.mute_music);

    // Recap — dealer arrival animation ON (S14); default tab Tier 1 (S15).
    assert(s.recap.dealer_arrival_animation);
    assert(s.recap.default_aggressor_recap_tab
               == pt::settings::DefaultAggressorRecapTab::Tier1);

    // Tomatoes — Shop button visible by default (S17).
    assert(s.tomatoes.shop_button_visible);
    assert(!s.tomatoes.leaderboard_opt_in);

    // General — confirm-before-leaving-site ON (S16).
    assert(s.general.confirm_before_leaving_site);

    // Account.
    assert(s.account.display_name_override.empty());
}

static void test_animation_clock() {
    pt::backbone::reset_animation_clock_for_testing();
    // Stub implementation: total_ms_since_app_start() returns 0 and
    // delta_ms_since_last_frame() returns 0.0f; tick() is a no-op. The real
    // implementation lives in Z05. Per the amendment (B9/B10/B11) the clock
    // no longer pauses and no longer exposes a separate pausable
    // animation-time counter — modal-driven pause of scenario time is the
    // Z10 Delta Timer's responsibility, so the clock keeps running while a
    // modal is open.
    assert(pt::backbone::total_ms_since_app_start() == 0u);
    assert(pt::backbone::delta_ms_since_last_frame() == 0.0f);

    // Drive the tick API surface to prove it links; the stub ignores it.
    pt::backbone::tick(100);
    assert(pt::backbone::total_ms_since_app_start() == 0u);
}

static void test_modal_state() {
    pt::backbone::reset_modal_state_for_testing();
    // is_modal_locked() derives from the screen-state tutorial phase, so
    // start from a clean screen state too.
    pt::backbone::reset_screen_state_for_testing();

    assert(!pt::backbone::is_any_modal_open());
    assert(pt::backbone::modal_stack_depth() == 0u);
    // topmost_modal() was renamed to current_modal_id() and now returns an
    // optional (B7): empty when no modal is open.
    assert(pt::backbone::current_modal_id() == std::nullopt);
    assert(!pt::backbone::is_modal_locked());

    pt::backbone::notify_modal_opened(pt::backbone::ModalId{42});
    assert(pt::backbone::is_any_modal_open());
    assert(pt::backbone::modal_stack_depth() == 1u);
    assert(pt::backbone::current_modal_id().has_value());
    assert(pt::backbone::current_modal_id()->value == 42u);

    pt::backbone::notify_modal_opened(pt::backbone::ModalId{7});
    assert(pt::backbone::modal_stack_depth() == 2u);
    assert(pt::backbone::current_modal_id()->value == 7u);

    pt::backbone::notify_modal_closed(pt::backbone::ModalId{7});
    pt::backbone::notify_modal_closed(pt::backbone::ModalId{42});
    assert(!pt::backbone::is_any_modal_open());
    assert(pt::backbone::modal_stack_depth() == 0u);
    assert(pt::backbone::current_modal_id() == std::nullopt);

    // is_modal_locked() (B8) is true only while the tutorial walkthrough is
    // Active; Z10 observes it to gate the Delta Timer and Z11/Z14 to lock
    // modal interaction.
    pt::backbone::set_tutorial_state(
        pt::backbone::TutorialState{pt::backbone::TutorialPhase::Active, 1});
    assert(pt::backbone::is_modal_locked());
    pt::backbone::set_tutorial_state(
        pt::backbone::TutorialState{pt::backbone::TutorialPhase::Complete, 0});
    assert(!pt::backbone::is_modal_locked());
    pt::backbone::reset_screen_state_for_testing();
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

    // TutorialComplete is the architecture's enumerated post-tutorial screen
    // (B5); Error (above) is the 6th, boot-failure value retained for Z05.
    // kScreenCount covers all six.
    assert(pt::backbone::kScreenCount == 6u);
    pt::backbone::set_screen(pt::backbone::ScreenId::TutorialComplete,
                             std::nullopt);
    snap = pt::backbone::read_screen_state();
    assert(snap.current == pt::backbone::ScreenId::TutorialComplete);
    assert(!snap.active_scenario.has_value());
    assert(!pt::backbone::is_in_scenario());

    // Tutorial state (B6) is orthogonal to the screen: it defaults to
    // Inactive after a reset and survives a screen transition.
    pt::backbone::reset_screen_state_for_testing();
    snap = pt::backbone::read_screen_state();
    assert(snap.tutorial_state.phase == pt::backbone::TutorialPhase::Inactive);
    assert(snap.tutorial_state.active_step == 0);

    pt::backbone::set_tutorial_state(
        pt::backbone::TutorialState{pt::backbone::TutorialPhase::Active, 3});
    pt::backbone::set_screen(pt::backbone::ScreenId::Game,
                             pt::engine::ScenarioId{777});
    snap = pt::backbone::read_screen_state();
    assert(snap.current == pt::backbone::ScreenId::Game);
    assert(snap.tutorial_state.phase == pt::backbone::TutorialPhase::Active);
    assert(snap.tutorial_state.active_step == 3);
    assert(snap.active_scenario.has_value());
    assert(*snap.active_scenario == pt::engine::ScenarioId{777});

    pt::backbone::reset_screen_state_for_testing();
}

static void test_event_router() {
    pt::backbone::reset_event_router_for_testing();

    bool top_fired = false;
    bool bottom_fired = false;

    // install_*_handler was reshaped to register_*_handler(context, handler,
    // priority, tag) (B2): the leading HandlerContext predicate gates
    // eligibility by global state. An empty (default-constructed) context
    // means the handler is always eligible.
    //
    // Install BackgroundCatchAll first; it will run last in priority order.
    const auto bottom_handle = pt::backbone::register_key_handler(
        {},
        [&](const pt::backbone::KeyEvent&) {
            bottom_fired = true;
            return true;  // Would consume.
        },
        pt::backbone::HandlerPriority::BackgroundCatchAll,
        "test.bottom");

    // Install ModalLayer; it outranks BackgroundCatchAll and consumes Enter.
    const auto top_handle = pt::backbone::register_key_handler(
        {},
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

    // Priority-inversion fix (B1): the tutorial overlay now outranks a modal,
    // so a tutorial-active Escape handler captures the key before any modal
    // sees it. (Before the amendment, ModalLayer=0 would have swallowed it.)
    pt::backbone::reset_event_router_for_testing();
    bool tutorial_fired = false;
    bool modal_fired = false;
    pt::backbone::register_key_handler(
        {},
        [&](const pt::backbone::KeyEvent&) {
            modal_fired = true;
            return true;
        },
        pt::backbone::HandlerPriority::ModalLayer,
        "test.modal");
    pt::backbone::register_key_handler(
        {},
        [&](const pt::backbone::KeyEvent&) {
            tutorial_fired = true;
            return true;
        },
        pt::backbone::HandlerPriority::TutorialOverlay,
        "test.tutorial");
    pt::backbone::dispatch_key_event({
        pt::backbone::KeyEventType::KeyDown,
        pt::backbone::KeyCode::Escape,
        pt::backbone::ModMask::None,
    });
    assert(tutorial_fired);  // TutorialOverlay = 0 runs first ...
    assert(!modal_fired);    // ... and consumes before the modal layer.

    // Context predicate (B2): a handler whose context evaluates false is
    // skipped, and a lower-priority but eligible handler runs instead.
    pt::backbone::reset_event_router_for_testing();
    bool gated_fired = false;
    bool fallthrough_fired = false;
    pt::backbone::register_key_handler(
        []() { return false; },  // context: never eligible
        [&](const pt::backbone::KeyEvent&) {
            gated_fired = true;
            return true;
        },
        pt::backbone::HandlerPriority::ModalLayer,
        "test.gated");
    pt::backbone::register_key_handler(
        []() { return true; },  // context: always eligible
        [&](const pt::backbone::KeyEvent&) {
            fallthrough_fired = true;
            return true;
        },
        pt::backbone::HandlerPriority::ScreenContext,
        "test.fallthrough");
    pt::backbone::dispatch_key_event({
        pt::backbone::KeyEventType::KeyDown,
        pt::backbone::KeyCode::Enter,
        pt::backbone::ModMask::None,
    });
    assert(!gated_fired);       // gated out by its false context predicate
    assert(fallthrough_fired);  // lower-priority but eligible handler runs

    pt::backbone::reset_event_router_for_testing();
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

    // current_focus() was renamed get_focused_element() (B3); it returns
    // kNoFocus while keyboard mode is inactive or no element is focused.
    assert(pt::backbone::get_focused_element() == pt::backbone::kNoFocus);
    assert(!pt::backbone::is_keyboard_mode_active());
    assert(pt::backbone::context_depth() == 0u);

    pt::backbone::activate_keyboard_mode();
    assert(pt::backbone::is_keyboard_mode_active());

    constexpr auto kA = pt::backbone::make_focusable_id("test.a");
    constexpr auto kB = pt::backbone::make_focusable_id("test.b");
    constexpr auto kC = pt::backbone::make_focusable_id("test.c");

    const pt::backbone::FocusableId base_list[] = {kA, kB, kC};
    // register_focus_list() now takes the owning ScreenId (B4).
    pt::backbone::register_focus_list(pt::backbone::ScreenId::Root, base_list);
    assert(pt::backbone::get_focused_element() == kA);

    pt::backbone::advance_focus(false);  // forward
    assert(pt::backbone::get_focused_element() == kB);

    pt::backbone::advance_focus(true);  // reverse
    assert(pt::backbone::get_focused_element() == kA);

    pt::backbone::snap_focus_to(kC);
    assert(pt::backbone::get_focused_element() == kC);

    // Push a new context for a modal.
    constexpr auto kJ = pt::backbone::make_focusable_id("modal.j");
    constexpr auto kK = pt::backbone::make_focusable_id("modal.k");
    const pt::backbone::FocusableId modal_list[] = {kJ, kK};
    pt::backbone::push_focus_context(modal_list, kJ, "test.modal_ctx");
    assert(pt::backbone::get_focused_element() == kJ);
    assert(pt::backbone::context_depth() == 1u);

    pt::backbone::advance_focus(false);
    assert(pt::backbone::get_focused_element() == kK);

    // Pop: the prior context (with focus on kC) is restored.
    pt::backbone::pop_focus_context();
    assert(pt::backbone::get_focused_element() == kC);
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
