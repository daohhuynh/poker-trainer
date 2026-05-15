#pragma once

#include "theme/theme_tokens.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace poker_trainer::settings {

// ----- Enumerations referenced across sections -----

// Scenario type filter. The user can toggle which scenario types
// are eligible for spawning. At least one must be enabled.
enum class ScenarioTypeFilter : std::uint8_t {
    Aggressor = 0,
    Caller = 1,
};

inline constexpr std::size_t kScenarioTypeCount = 2;

// Street weight preset. The user picks one preset (or custom) to
// bias scenario spawn toward specific betting streets.
enum class StreetWeightPreset : std::uint8_t {
    Uniform = 0,        // Equal weight across preflop/flop/turn/river
    FlopHeavy = 1,      // Weighted toward flop scenarios
    TurnHeavy = 2,      // Weighted toward turn scenarios
    RiverHeavy = 3,     // Weighted toward river scenarios
    Custom = 4,         // Use street_weights_custom_* fields
};

// Time pressure preset. The user picks a preset or "Off" or "Custom".
enum class TimePressurePreset : std::uint8_t {
    Off = 0,            // No time pressure; countdown hidden
    Beginner = 1,       // Generous time (target * 2.0)
    Standard = 2,       // Target time (1.0x)
    Aggressive = 3,     // Tight time (target * 0.75)
    Brutal = 4,         // Very tight (target * 0.5)
    Custom = 5,         // Use time_pressure_custom_seconds
};

// Music genre selection (one active genre at a time).
enum class ActiveMusicGenre : std::uint8_t {
    LoungeJazz = 0,
    Classical = 1,
    BossaNova = 2,
    Ambient = 3,
    // Special: no music plays. Distinct from muted (muted preserves
    // the active genre selection so unmuting resumes; None means
    // the user explicitly does not want music playing).
    None = 255,
};

// Language. English is the only supported language in V1; the enum
// is here for forward compatibility.
enum class Language : std::uint8_t {
    English = 0,
};

// ----- Sub-structs per settings section -----

// ----- Section 1: Gameplay -----

struct GameplaySettings {
    // Lower bound of the difficulty range, 0.0 to 1.0 (display
    // shows 0-100 to the user; internal storage is 0-1).
    // Default 0.2.
    float difficulty_min{0.2f};

    // Upper bound of the difficulty range, 0.0 to 1.0.
    // Must be >= difficulty_min. Default 0.8.
    float difficulty_max{0.8f};

    // Time pressure preset.
    TimePressurePreset time_pressure_preset{TimePressurePreset::Standard};

    // Custom time pressure value in seconds (only used when
    // time_pressure_preset == Custom). Range: 5-300. Default 30.
    std::uint16_t time_pressure_custom_seconds{30};

    // Which scenario types are enabled. At least one must be true.
    // The integration test asserts that at least one is true on
    // construction.
    std::array<bool, kScenarioTypeCount> scenario_types_enabled{{
        true,  // Aggressor
        true,  // Caller
    }};

    // Include multi-tier Aggressor scenarios (Pure Bluff with
    // multiple bet-size tiers to evaluate).
    bool include_multi_tier{true};

    // Include side-pot scenarios (multi-way all-in situations
    // with side pot calculations).
    bool include_side_pots{true};

    // Street weight preset.
    StreetWeightPreset street_weights_preset{StreetWeightPreset::Uniform};

    // Custom street weights (only used when street_weights_preset
    // == Custom). Each value 0.0 to 1.0; the four values are
    // normalized to sum to 1.0 at use time. Defaults: uniform.
    float street_weights_custom_preflop{0.25f};
    float street_weights_custom_flop{0.25f};
    float street_weights_custom_turn{0.25f};
    float street_weights_custom_river{0.25f};

    // Delta Timer (the live elapsed-time display) enabled.
    // Independent from the Visual Countdown. Default true.
    bool delta_timer_enabled{true};

    // Include the bet sizing input row in Aggressor scenarios.
    // When false, scenarios skip the bet-size component (still
    // valid for solo math practice). Default true.
    bool include_bet_sizing_inputs{true};
};

// ----- Section 2: Units -----

struct UnitsSettings {
    // True means cash mode (dollar amounts displayed); false
    // means big blinds mode (BB multiples displayed). Affects
    // every numeric value shown on the Game screen and in
    // math inputs.
    bool cash_mode{true};

    // Big blind value in cents, used in cash mode to convert
    // between cash and BB representations internally. Always
    // 100 cents ($1) for V1. Range: 25-10000. Default 100.
    std::uint16_t big_blind_value_cents{100};
};

// ----- Section 3: Display -----

struct DisplaySettings {
    // Active theme. One of kThemeIdNoLimit / kThemeIdSlate /
    // kThemeIdOcean / kThemeIdSage from theme_tokens.hpp.
    // Default kThemeIdNoLimit.
    std::uint8_t active_theme_id{theme::kThemeIdNoLimit};

    // Show the HUD overlay (pot, blinds, floating bet amount).
    // Default true.
    bool show_hud{true};

    // Show the Visual Countdown timer (top-right below cluster).
    // Independent from time pressure being active. Default true.
    bool show_countdown{true};

    // Show position indicators (UTG/HJ/CO/BTN/SB/BB) at each
    // opponent seat. Default true.
    bool show_position_indicators{true};

    // Auto-activate keyboard focus mode on first Tab press.
    // When false, the user must explicitly toggle keyboard mode
    // via a separate keybind. Default true.
    bool keyboard_mode_auto_activate{true};
};

// ----- Section 4: Audio -----

struct AudioSettings {
    // Master volume, 0-100. Applied as a multiplier on both
    // music and SFX. Default 80.
    std::uint8_t master_volume{80};

    // Music stream volume, 0-100. Default 60.
    std::uint8_t music_volume{60};

    // SFX volume, 0-100. Default 75.
    std::uint8_t sfx_volume{75};

    // Active music genre. The shuffle pool draws from this
    // genre's unlocked tracks.
    ActiveMusicGenre current_music_genre{ActiveMusicGenre::LoungeJazz};

    // Music globally muted. When true, music is silenced
    // regardless of music_volume.
    bool music_muted{false};

    // SFX globally muted. When true, all SFX are silenced
    // regardless of sfx_volume.
    bool sfx_muted{false};

    // Defer audio playback until the user makes their first
    // gesture (click or keypress). Required by browser autoplay
    // policies. Default true (recommended for browser deployment).
    bool defer_until_user_gesture{true};
};

// ----- Section 5: Recap -----

struct RecapSettings {
    // Enable smooth screen transitions (slide between Game and
    // Post-Round, ceremonial transition between Mode Selection
    // and Game). When false, transitions are instant cuts.
    // Default true.
    bool transitions_enabled{true};

    // Show the detailed breakdown row in the Post-Round stat
    // modal. When false, only the summary row is shown.
    // Default true.
    bool show_detailed_breakdown{true};

    // Auto-advance from Post-Round to next scenario after the
    // configured delay. When false, the user must click Again
    // explicitly. Default false (explicit click is the architecture
    // default; auto-advance is an opt-in).
    bool auto_advance{false};

    // Auto-advance delay in seconds (only used when auto_advance
    // is true). Range: 1-30. Default 5.
    std::uint8_t auto_advance_delay_seconds{5};
};

// ----- Section 6: Tomatoes -----

struct TomatoesSettings {
    // Show the user's Lifetime Tomatoes total in the Account /
    // Profile view. Spendable Tomatoes always show in the Shop;
    // this toggle only affects the lifetime counter display.
    // Default true.
    bool show_lifetime_total{true};

    // Opt into the public leaderboard. When true, the user's
    // display name and Lifetime Tomatoes are published. When
    // false, the user can still view the leaderboard but is
    // not listed on it. Requires an authenticated account
    // (guests cannot opt in). Default false.
    bool leaderboard_opt_in{false};
};

// ----- Section 7: Account -----

// Account settings are mostly action-driven (sign-in modal, etc.)
// rather than persistent flags. The only persistent field is
// display name, which is editable separately from the Auth0
// profile display name. The actual auth state lives in
// persistence_schema.hpp's AccountState, not here.

struct AccountSettings {
    // Locally-edited display name override. When empty, the
    // Auth0 profile display name is used. When non-empty, this
    // overrides the Auth0 name for leaderboard and UI display.
    // Max length 32 characters. The integration test asserts the
    // default value is empty.
    std::string display_name_override;
};

// ----- Section 8: General -----

struct GeneralSettings {
    // UI language. English is the only supported value in V1.
    Language language{Language::English};

    // Maximum scenario history entries to retain (mirrors the
    // kMaxScenarioHistoryEntries cap in persistence_schema.hpp
    // but allows the user to set a lower bound for privacy).
    // Range: 0 (no history) to 256 (full). Default 256.
    std::uint16_t scenario_history_retention{256};
};

// ----- Section 9: Legal -----

// Legal section is action-driven: it opens sub-modals showing
// ToS, Privacy Policy, and About content. No persistent fields
// beyond the version of the policies the user has acknowledged.

struct LegalSettings {
    // The latest Terms of Service version the user has read or
    // acknowledged. Used to surface a re-acknowledgment prompt
    // when the ToS is updated. Default 0 (never acknowledged).
    std::uint32_t acknowledged_tos_version{0};

    // The latest Privacy Policy version the user has read or
    // acknowledged. Default 0.
    std::uint32_t acknowledged_privacy_version{0};
};

// ----- Master settings struct -----

// The full Settings object. Aggregates all 9 sections. Default
// construction yields a fully-valid settings state with the
// documented defaults; first-launch users start here.
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

// Result of validating a Settings instance. Used by Z12 after
// user mutations to detect invariant violations (e.g., user
// disabled both scenario types). When invalid, Z12 reverts the
// most recent mutation rather than committing an unusable state.
enum class SettingsValidationResult : std::uint8_t {
    Ok = 0,

    // At least one scenario type must be enabled.
    NoScenarioTypeEnabled = 1,

    // difficulty_min must be <= difficulty_max, both in [0.0, 1.0].
    InvalidDifficultyRange = 2,

    // time_pressure_custom_seconds out of range when preset is Custom.
    InvalidTimePressureCustom = 3,

    // big_blind_value_cents out of range.
    InvalidBigBlindValue = 4,

    // Volume out of range (master/music/sfx, 0-100).
    InvalidVolumeValue = 5,

    // display_name_override exceeds 32 characters.
    InvalidDisplayNameOverride = 6,

    // scenario_history_retention exceeds 256.
    InvalidScenarioHistoryRetention = 7,

    // auto_advance_delay_seconds out of range (1-30).
    InvalidAutoAdvanceDelay = 8,

    // Street weight preset is Custom but the four custom values
    // sum to zero (cannot normalize).
    InvalidStreetWeightsCustom = 9,
};

// Validate a Settings instance. Returns Ok on success, otherwise
// returns the first invariant violation encountered. Z12 uses
// this to gate mutations.
[[nodiscard]] SettingsValidationResult validate(const Settings& s) noexcept;

// Maximum length for the display name override field, in characters.
inline constexpr std::size_t kMaxDisplayNameOverrideLength = 32;

}  // namespace poker_trainer::settings
