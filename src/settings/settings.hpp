#pragma once

#include "theme/theme_tokens.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace poker_trainer::settings {

// ----- Enumerations referenced across sections -----

// Chip denomination mode (Gameplay). Purely visual; bet sizing, EV, and
// grading are unaffected. Only chip rendering and stack-height visualization
// change.
//   Stake-scaled: denominations rescale per scenario based on blind level
//                 (matches real cardroom practice). Default.
//   Fixed:        a constant $1/$5/$25/$100/$500 set across all scenarios.
enum class ChipDenominationMode : std::uint8_t {
    StakeScaled = 0,
    Fixed = 1,
};

// Music genre selection (one active genre at a time). Mirrors the
// audio::MusicGenre enum; the shuffle pool draws from this genre's unlocked
// tracks.
enum class ActiveMusicGenre : std::uint8_t {
    LoungeJazz = 0,
    Classical = 1,
    BossaNova = 2,
    Ambient = 3,
};

// Which tab is shown by default on the Post-Round Screen for a multi-tier
// Aggressor scenario. Only applies when the Bet Sizing Engine is enabled,
// since only multi-tier Aggressor scenarios have tier tabs.
enum class DefaultAggressorRecapTab : std::uint8_t {
    Tier1 = 0,
    Summary = 1,
};

// ----- Sub-structs per settings section -----

// ----- Section 1: Gameplay -----

struct GameplaySettings {
    // --- Scenario-generation distribution ---

    // Street split weights, as integer percentages summing to 100, biasing
    // scenario spawn toward betting streets. Adjusted via four coupled sliders
    // using the most-recently-touched-locks rule (the slider the user just
    // adjusted holds; the other three redistribute proportionally to keep the
    // sum at 100). Each slider clamps to [0, 100].
    //
    // These weights are seed-encoded: they are the ONLY Gameplay settings that
    // participate in deterministic scenario reconstruction (see ARCHITECTURE
    // Reproducibility). Their model and defaults are therefore locked against
    // the scenario seed format — changing either breaks every reconstructed
    // scenario.
    //
    // Defaults: 15 / 35 / 30 / 20 (Pre-flop / Flop / Turn / River).
    std::uint8_t street_weight_preflop{15};
    std::uint8_t street_weight_flop{35};
    std::uint8_t street_weight_turn{30};
    std::uint8_t street_weight_river{20};

    // Custom-mode Aggressor/Caller scenario-type weights, persisted by the
    // Mode Selection Custom popup's Save action. Integer percentages summing
    // to 100; used only when the user launches Custom mode (STANDARD is a
    // fixed 50/50, Aggressor is 100/0, Caller is 0/100 and do not read these).
    // Default 50 / 50.
    std::uint8_t custom_aggressor_weight{50};
    std::uint8_t custom_caller_weight{50};

    // Base probability that a scenario is generated as an all-in side-pot
    // split. Configurable; in [0.0, 1.0]. Default ~0.10.
    float side_pot_frequency{0.10f};

    // --- Settings-page Gameplay controls ---

    // Chip denomination mode. Default Stake-scaled.
    ChipDenominationMode chip_denomination_mode{ChipDenominationMode::StakeScaled};

    // Bet sizing engine toggle for Aggressor scenarios. Default ON. When
    // enabled, Aggressor scenarios present the same scenario at multiple bet
    // sizes (1/3, 1/2, full, overbet) — the multi-tier drill. When disabled,
    // a single bet size per scenario.
    bool bet_sizing_engine_enabled{true};

    // Difficulty / opponent baseline tendency range. Constrains the F value
    // used in the deterministic fold function. Stored as a floating-point
    // range in [0.0, 1.0]; displayed to the user as integer percentages 0-100.
    // difficulty_max must be >= difficulty_min. Default range [0.2, 0.8].
    float difficulty_min{0.2f};
    float difficulty_max{0.8f};

    // Time pressure. The default target time scales by street and scenario
    // type and is computed in Z10 (Temporal), not stored here. When
    // time_pressure_custom_enabled is true, the flat custom value below
    // overrides the street-scaled default for every scenario.
    bool time_pressure_custom_enabled{false};

    // Flat custom time pressure value in seconds, used only when
    // time_pressure_custom_enabled is true. Range 1-300. Default 30.
    std::uint16_t time_pressure_custom_seconds{30};

    // Delta Timer (the live elapsed-time display) enabled. Independent from
    // the Visual Countdown. Default true.
    //
    // NOTE: preserved from the original contract. The reconciliation report's
    // S19 over-declaration list does not name this field, so it is kept as-is;
    // however, ARCHITECTURE's Gameplay Settings list does not expose a Delta
    // Timer toggle (the Delta Timer is described as an always-present feature),
    // so whether this should remain a user setting is flagged for the final
    // reconciliation pass.
    bool delta_timer_enabled{true};

    // Show/hide HUD elements toggle. Default ON (HUD shown). When off, floating
    // numbers (call amounts, pot total, blinds, opponent stack numbers) hide
    // and the user counts chips manually; the denomination legend stays
    // visible.
    bool show_hud{true};

    // Show the Visual Countdown timer toggle. Default OFF. Independent of the
    // Show/hide HUD toggle. When off, no countdown renders, but the time
    // penalty still applies based on actual elapsed time.
    bool show_countdown{false};
};

// ----- Section 2: Units -----

struct UnitsSettings {
    // True means cash mode (dollar amounts displayed); false means big blinds
    // mode (BB multiples displayed). Switches all UI numbers between raw Cash
    // and Big Blinds.
    bool cash_mode{true};
};

// ----- Section 3: Display -----

struct DisplaySettings {
    // Active theme. One of kThemeIdNoLimit / kThemeIdSlate / kThemeIdOcean /
    // kThemeIdSage from theme_tokens.hpp. Default kThemeIdNoLimit.
    std::uint8_t active_theme_id{theme::kThemeIdNoLimit};

    // Reduce Motion accessibility toggle. Controls Tiers 1, 2, and 4 of the
    // Motion Layer. Default OFF (motion enabled). The runtime additionally
    // honors the OS prefers-reduced-motion media query.
    bool reduce_motion{false};

    // Background atmospheric movement. Controls the silhouette sprite drift in
    // the blurred background. Default ON.
    bool background_atmospheric_movement{true};

    // Particle drift. Controls Tier 1 ambient particle motion. Default ON.
    bool particle_drift{true};
};

// ----- Section 4: Audio -----

struct AudioSettings {
    // Single global volume, 0-100. Applied to both music and SFX. Default 50.
    std::uint8_t volume{50};

    // Active music genre. The shuffle pool draws from this genre's unlocked
    // tracks. Default Lounge Jazz.
    ActiveMusicGenre current_music_genre{ActiveMusicGenre::LoungeJazz};

    // Mute all audio (music and SFX together).
    bool mute_all{false};

    // Mute SFX (independent from music mute).
    bool mute_sfx{false};

    // Mute music (independent from SFX mute).
    bool mute_music{false};
};

// ----- Section 5: Recap -----

struct RecapSettings {
    // Dealer arrival animation toggle. Default ON. When enabled, the
    // Post-Round Screen renders the dealer and modal via a sequenced fade-in;
    // when disabled, all elements appear simultaneously at the end of the
    // screen slide transition.
    bool dealer_arrival_animation{true};

    // Screen transitions toggle. Default ON. When enabled, ceremonial
    // fade-to-black transitions and 350ms slide transitions render their full
    // animations; when disabled, screen-to-screen transitions are instant
    // cuts. Does not affect modal open/close animations or the Root <-> Mode
    // Selection button morph (those are governed by Reduce Motion).
    bool transitions_enabled{true};

    // Default tab shown on the Post-Round Screen for multi-tier Aggressor
    // scenarios. Default Tier 1. Only applies when the Bet Sizing Engine is
    // enabled.
    DefaultAggressorRecapTab default_aggressor_recap_tab{DefaultAggressorRecapTab::Tier1};
};

// ----- Section 6: Tomatoes -----

struct TomatoesSettings {
    // Shop button visibility. When false, the Shop is hidden entirely. Default
    // true (visible).
    bool shop_button_visible{true};

    // Opt into the public leaderboard. When true, the user's display name and
    // Lifetime Tomatoes are published. Requires an authenticated account
    // (guests cannot opt in). Default false.
    bool leaderboard_opt_in{false};
};

// ----- Section 7: Account -----

// Account settings are mostly action-driven (sign-in modal, etc.) rather than
// persistent flags. The only persistent field is the display name override,
// which is editable separately from the Auth0 profile display name. The actual
// auth state lives in persistence_schema.hpp's AccountState, not here.

struct AccountSettings {
    // Locally-edited display name override. When empty, the Auth0 profile
    // display name is used. When non-empty, this overrides the Auth0 name for
    // leaderboard and UI display. Max length 32 characters. The integration
    // test asserts the default value is empty.
    std::string display_name_override;
};

// ----- Section 8: General -----

struct GeneralSettings {
    // Confirm before leaving site (during an active scenario) toggle. Default
    // ON. When enabled, navigation away from the Game screen during an active
    // scenario (browser back, Cmd/Ctrl+W, tab close) is intercepted with the
    // Leave-Site confirmation modal. Only affects active-scenario states on
    // the Game screen; Root, Mode Selection, and Post-Round always exit
    // cleanly.
    bool confirm_before_leaving_site{true};
};

// ----- Section 9: Legal -----

// Legal section is entirely action-driven: it opens sub-modals showing ToS,
// Privacy Policy, and About / Credits content. It carries no persistent state.
// The struct is retained for one-to-one correspondence with the nine settings
// sections in the architecture's Settings sidebar.

struct LegalSettings {};

// ----- Master settings struct -----

// The full Settings object. Aggregates all 9 sections. Default construction
// yields a fully-valid settings state with the documented defaults;
// first-launch users start here.
struct Settings {
    GameplaySettings gameplay;
    UnitsSettings units;
    DisplaySettings display;
    AudioSettings audio;
    RecapSettings recap;
    TomatoesSettings tomatoes;
    AccountSettings account;
    GeneralSettings general;
    LegalSettings legal;
};

// ----- Validation -----

// Result of validating a Settings instance. Used by Z12 after user mutations
// to detect invariant violations. When invalid, Z12 reverts the most recent
// mutation rather than committing an unusable state.
enum class SettingsValidationResult : std::uint8_t {
    Ok = 0,

    // Street split weights (preflop + flop + turn + river) must sum to 100.
    InvalidStreetWeights = 1,

    // Custom-mode Aggressor + Caller weights must sum to 100.
    InvalidCustomModeWeights = 2,

    // side_pot_frequency out of range [0.0, 1.0].
    InvalidSidePotFrequency = 3,

    // difficulty_min must be <= difficulty_max, both in [0.0, 1.0].
    InvalidDifficultyRange = 4,

    // time_pressure_custom_seconds out of range [1, 300] when custom enabled.
    InvalidTimePressureCustom = 5,

    // Volume out of range (0-100).
    InvalidVolumeValue = 6,

    // display_name_override exceeds 32 characters.
    InvalidDisplayNameOverride = 7,
};

// Validate a Settings instance. Returns Ok on success, otherwise returns the
// first invariant violation encountered. Z12 uses this to gate mutations.
[[nodiscard]] SettingsValidationResult validate(const Settings& s) noexcept;

// Maximum length for the display name override field, in characters.
inline constexpr std::size_t kMaxDisplayNameOverrideLength = 32;

}  // namespace poker_trainer::settings
