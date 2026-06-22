// Pass-1 wiring tests for the interim settings-blob codec (theme + custom
// weights), the theme-at-boot read, the boot persistence load, and the
// persistence-backed Custom-weights round-trip — exercised against Zone 04's
// IdbfsStore over the in-memory MemoryStorage backend, exactly as they behave
// over the real IDBFS backend in the app.

#include "bridge/persistent_weights_store.hpp"
#include "bridge/settings_persistence.hpp"

#include "settings/settings.hpp"

#include "screens/custom_popup.hpp"

#include "backbone/game_mode.hpp"
#include "persistence/idbfs.hpp"
#include "persistence/persistence_schema.hpp"
#include "theme/theme.hpp"
#include "theme/theme_tokens.hpp"

#include "persistence_mocks.hpp"

#include <cstdint>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

namespace br = poker_trainer::bridge;
namespace pe = poker_trainer::persistence;
namespace th = poker_trainer::theme;
namespace bb = poker_trainer::backbone;
namespace sc = poker_trainer::screens;

namespace {

[[nodiscard]] int aggr(const bb::CustomConfig& c) { return static_cast<int>(c.aggressor_weight); }
[[nodiscard]] int call(const bb::CustomConfig& c) { return static_cast<int>(c.caller_weight); }

}  // namespace

// ----- interim codec --------------------------------------------------------

TEST(InterimSettingsCodec, RoundTripsThemeAndWeights) {
    const br::InterimSettings in{th::kThemeIdOcean, bb::CustomConfig{70, 30}};
    const std::vector<std::uint8_t> blob = br::encode_interim_settings(in);
    const std::optional<br::InterimSettings> out = br::decode_interim_settings(blob);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->theme_id, th::kThemeIdOcean);
    EXPECT_EQ(aggr(out->custom_weights), 70);
    EXPECT_EQ(call(out->custom_weights), 30);
}

TEST(InterimSettingsCodec, DecodeRejectsAbsentTooShortOrForeignBlob) {
    const std::vector<std::uint8_t> empty;
    EXPECT_FALSE(br::decode_interim_settings(empty).has_value());

    const std::vector<std::uint8_t> too_short{'P', 'T', 'S', '0'};
    EXPECT_FALSE(br::decode_interim_settings(too_short).has_value());

    // The Zone 04 OUTER AppState magic ('PTAS') is NOT our INNER settings magic.
    const std::vector<std::uint8_t> foreign{'P', 'T', 'A', 'S', 1, 2, 3, 4};
    EXPECT_FALSE(br::decode_interim_settings(foreign).has_value());
}

// ----- theme-at-boot --------------------------------------------------------

TEST(ThemeAtBoot, ReadsPersistedThemeAndAppliesItBeforeFirstFrame) {
    for (const std::uint8_t id :
         {th::kThemeIdNoLimit, th::kThemeIdSlate, th::kThemeIdOcean, th::kThemeIdSage}) {
        pe::AppState state{};
        state.settings_blob = br::encode_interim_settings(br::InterimSettings{id, {}});
        EXPECT_EQ(br::read_persisted_theme_id(state), id);

        // The boot path applies it via set_theme before the first frame.
        th::set_theme(br::read_persisted_theme_id(state));
        EXPECT_EQ(th::get_active_theme().theme_id, id);
    }
}

TEST(ThemeAtBoot, DefaultsToNoLimitWhenNothingSaved) {
    const pe::AppState fresh{};  // empty settings_blob
    EXPECT_EQ(br::read_persisted_theme_id(fresh), th::kThemeIdNoLimit);
}

// ----- boot persistence (load saved state at launch) ------------------------

TEST(BootPersistence, SavedThemeAndWeightsLoadAtBoot) {
    pe::test::MemoryStorage backend;

    // A prior session persists a state carrying a non-default theme + split.
    {
        pe::IdbfsStore store{backend};
        store.load_state();
        pe::AppState state = store.state();
        state.settings_blob = br::encode_interim_settings(
            br::InterimSettings{th::kThemeIdSage, bb::CustomConfig{65, 35}});
        store.save_state(state);
    }

    // A fresh launch reloads from the same durable backend.
    pe::IdbfsStore reloaded{backend};
    const pe::AppState loaded = reloaded.load_state();
    EXPECT_EQ(br::read_persisted_theme_id(loaded), th::kThemeIdSage);
    const std::optional<bb::CustomConfig> weights = br::read_persisted_custom_weights(loaded);
    ASSERT_TRUE(weights.has_value());
    EXPECT_EQ(aggr(*weights), 65);
    EXPECT_EQ(call(*weights), 35);
}

TEST(BootPersistence, DefaultsWhenNothingSaved) {
    pe::test::MemoryStorage backend;  // empty: first-ever launch
    pe::IdbfsStore store{backend};
    const pe::AppState loaded = store.load_state();
    EXPECT_EQ(br::read_persisted_theme_id(loaded), th::kThemeIdNoLimit);
    EXPECT_FALSE(br::read_persisted_custom_weights(loaded).has_value());
}

// ----- persistence-backed Custom-weights store (Mode Selection popup) -------

TEST(PersistentWeightsStore, SaveThenReloadPersists) {
    pe::test::MemoryStorage backend;

    pe::IdbfsStore store{backend};
    store.load_state();
    br::PersistentCustomWeightsStore weights_store{store};
    EXPECT_FALSE(weights_store.load().has_value());  // Save has never run

    weights_store.save(bb::CustomConfig{70, 30});
    const std::optional<bb::CustomConfig> same_session = weights_store.load();
    ASSERT_TRUE(same_session.has_value());
    EXPECT_EQ(aggr(*same_session), 70);
    EXPECT_EQ(call(*same_session), 30);

    // Fresh launch: a new store over the same durable backend reads it back.
    pe::IdbfsStore store2{backend};
    store2.load_state();
    br::PersistentCustomWeightsStore weights_store2{store2};
    const std::optional<bb::CustomConfig> reloaded = weights_store2.load();
    ASSERT_TRUE(reloaded.has_value());
    EXPECT_EQ(aggr(*reloaded), 70);
    EXPECT_EQ(call(*reloaded), 30);
}

TEST(PersistentWeightsStore, ResetWithoutPriorSaveIs5050) {
    pe::test::MemoryStorage backend;
    pe::IdbfsStore store{backend};
    store.load_state();
    br::PersistentCustomWeightsStore weights_store{store};

    // Reset semantics (custom_popup): last-saved, or 50/50 when never saved.
    const bb::CustomConfig reset = sc::reset_to_saved(weights_store);
    EXPECT_EQ(aggr(reset), 50);
    EXPECT_EQ(call(reset), 50);
}

// ----- full settings codec ('PTS1') -----

namespace st = poker_trainer::settings;

TEST(FullSettingsCodec, RoundTripsEveryField) {
    st::Settings in{};
    in.gameplay.street_weight_preflop = 10;
    in.gameplay.street_weight_flop = 40;
    in.gameplay.street_weight_turn = 30;
    in.gameplay.street_weight_river = 20;
    in.gameplay.custom_aggressor_weight = 65;
    in.gameplay.custom_caller_weight = 35;
    in.gameplay.side_pot_frequency = 0.25f;
    in.gameplay.chip_denomination_mode = st::ChipDenominationMode::Fixed;
    in.gameplay.bet_sizing_engine_enabled = false;
    in.gameplay.difficulty_min = 0.3f;
    in.gameplay.difficulty_max = 0.6f;
    in.gameplay.time_pressure_custom_enabled = true;
    in.gameplay.time_pressure_custom_seconds = 123;
    in.gameplay.show_hud = false;
    in.gameplay.show_countdown = true;
    in.units.cash_mode = false;
    in.display.active_theme_id = th::kThemeIdOcean;
    in.display.reduce_motion = true;
    in.display.particle_drift = false;
    in.audio.volume = 73;
    in.audio.current_music_genre = st::ActiveMusicGenre::BossaNova;
    in.audio.mute_sfx = true;
    in.recap.transitions_enabled = false;
    in.recap.default_aggressor_recap_tab = st::DefaultAggressorRecapTab::Summary;
    in.tomatoes.shop_button_visible = false;
    in.tomatoes.leaderboard_opt_in = true;
    in.account.display_name_override = "Stack_Jack";
    in.general.confirm_before_leaving_site = false;

    const std::vector<std::uint8_t> blob = br::encode_settings(in);
    const std::optional<st::Settings> out = br::decode_settings(blob);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->gameplay.street_weight_flop, 40);
    EXPECT_EQ(out->gameplay.custom_aggressor_weight, 65);
    EXPECT_FLOAT_EQ(out->gameplay.side_pot_frequency, 0.25f);
    EXPECT_EQ(out->gameplay.chip_denomination_mode, st::ChipDenominationMode::Fixed);
    EXPECT_FALSE(out->gameplay.bet_sizing_engine_enabled);
    EXPECT_FLOAT_EQ(out->gameplay.difficulty_min, 0.3f);
    EXPECT_FLOAT_EQ(out->gameplay.difficulty_max, 0.6f);
    EXPECT_TRUE(out->gameplay.time_pressure_custom_enabled);
    EXPECT_EQ(out->gameplay.time_pressure_custom_seconds, 123);
    EXPECT_FALSE(out->gameplay.show_hud);
    EXPECT_TRUE(out->gameplay.show_countdown);
    EXPECT_FALSE(out->units.cash_mode);
    EXPECT_EQ(out->display.active_theme_id, th::kThemeIdOcean);
    EXPECT_TRUE(out->display.reduce_motion);
    EXPECT_FALSE(out->display.particle_drift);
    EXPECT_EQ(out->audio.volume, 73);
    EXPECT_EQ(out->audio.current_music_genre, st::ActiveMusicGenre::BossaNova);
    EXPECT_TRUE(out->audio.mute_sfx);
    EXPECT_FALSE(out->recap.transitions_enabled);
    EXPECT_EQ(out->recap.default_aggressor_recap_tab, st::DefaultAggressorRecapTab::Summary);
    EXPECT_FALSE(out->tomatoes.shop_button_visible);
    EXPECT_TRUE(out->tomatoes.leaderboard_opt_in);
    EXPECT_EQ(out->account.display_name_override, "Stack_Jack");
    EXPECT_FALSE(out->general.confirm_before_leaving_site);
}

TEST(FullSettingsCodec, DecodeRejectsInterimAndForeignBlobs) {
    const std::vector<std::uint8_t> interim =
        br::encode_interim_settings(br::InterimSettings{th::kThemeIdSlate, bb::CustomConfig{60, 40}});
    EXPECT_FALSE(br::decode_settings(interim).has_value());  // 'PTS0' is not 'PTS1'
    const std::vector<std::uint8_t> empty;
    EXPECT_FALSE(br::decode_settings(empty).has_value());
}

TEST(FullSettingsCodec, ReadPersistedSettingsMigratesInterimBlob) {
    pe::AppState state{};
    state.settings_blob =
        br::encode_interim_settings(br::InterimSettings{th::kThemeIdSage, bb::CustomConfig{70, 30}});
    const st::Settings s = br::read_persisted_settings(state);
    EXPECT_EQ(s.display.active_theme_id, th::kThemeIdSage);          // migrated theme
    EXPECT_EQ(s.gameplay.custom_aggressor_weight, 70);              // migrated split
    EXPECT_EQ(s.gameplay.custom_caller_weight, 30);
    EXPECT_TRUE(s.gameplay.show_hud);                              // everything else: defaults
    EXPECT_EQ(s.audio.volume, 50);
}

TEST(FullSettingsCodec, ReadPersistedSettingsRoundTripsFullBlob) {
    st::Settings in{};
    in.audio.volume = 88;
    in.display.active_theme_id = th::kThemeIdSlate;
    pe::AppState state{};
    state = br::with_settings(state, in);
    const st::Settings out = br::read_persisted_settings(state);
    EXPECT_EQ(out.audio.volume, 88);
    EXPECT_EQ(out.display.active_theme_id, th::kThemeIdSlate);
}

TEST(PersistentWeightsStore, SavingWeightsPreservesPersistedTheme) {
    pe::test::MemoryStorage backend;
    pe::IdbfsStore store{backend};
    store.load_state();

    // Pre-seed a non-default theme into the settings blob.
    pe::AppState state = store.state();
    state.settings_blob = br::encode_interim_settings(
        br::InterimSettings{th::kThemeIdOcean, bb::CustomConfig{50, 50}});
    store.save_state(state);

    // Saving the weights must not clobber the saved theme.
    br::PersistentCustomWeightsStore weights_store{store};
    weights_store.save(bb::CustomConfig{80, 20});
    EXPECT_EQ(br::read_persisted_theme_id(store.state()), th::kThemeIdOcean);
    const std::optional<bb::CustomConfig> weights = weights_store.load();
    ASSERT_TRUE(weights.has_value());
    EXPECT_EQ(aggr(*weights), 80);
    EXPECT_EQ(call(*weights), 20);
}
