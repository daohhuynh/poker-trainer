#pragma once

// Zone 12 — Settings search. A pure, case-insensitive SUBSTRING matcher over a
// per-entry keyword set, plus the catalog that maps every searchable control to its
// display name, section, and keywords. The architecture suggests "a fuzzy-match
// library such as Fuse.js"; that is a JS library with no place in a C++/wasm build
// and no vendored equivalent, so this is a dependency-free substring matcher.
//
// Substring (not subsequence): a query matches when it appears CONTIGUOUSLY within an
// entry's keyword blob, case-insensitively. Subsequence matching ("c-a-s-h" inside
// "ba**c**kground **a**tmo**s**p**h**eric") produced false positives on short queries
// and has no scoring discipline; contiguous substring kills those while still matching
// prefixes, infixes, and whole words. Each entry's keyword blob carries its section
// name, control name, on-screen labels, option values, sub-labels, and synonyms, so a
// section term ("gameplay"), an option value ("cash"/"fixed"), or a sub-label ("flop")
// all resolve to the right entry. Pure logic — unit-tested, no ImGui.

#include <array>
#include <cstdint>
#include <string_view>

namespace poker_trainer::settings {

// The nine settings sections, in sidebar / body order (ARCHITECTURE: Gameplay ->
// Units -> Display -> Audio -> Recap -> Tomatoes -> Account -> General -> Legal).
enum class SettingsSection : std::uint8_t {
    Gameplay = 0,
    Units = 1,
    Display = 2,
    Audio = 3,
    Recap = 4,
    Tomatoes = 5,
    Account = 6,
    General = 7,
    Legal = 8,
};

inline constexpr std::size_t kSectionCount = 9;

// Every searchable control. Body order within a section matches this enum's order.
enum class SettingId : std::uint16_t {
    // Gameplay
    StreetWeights = 0,
    ChipDenomination,
    BetSizingEngine,
    DifficultyRange,
    TimePressure,
    ShowHud,
    ShowCountdown,
    // Units
    UnitToggle,
    // Display
    Theme,
    ReduceMotion,
    BackgroundMovement,
    ParticleDrift,
    // Audio
    MusicType,
    Volume,
    MuteAll,
    MuteSfx,
    MuteMusic,
    // Recap
    DealerArrival,
    ScreenTransitions,
    DefaultRecapTab,
    // Tomatoes
    ShopVisibility,
    ResetTomatoes,
    LeaderboardOptIn,
    // Account
    Account,
    // General
    ConfirmLeaveSite,
    ResetAll,
    ResetSection,
    // Legal
    TermsOfService,
    PrivacyPolicy,
    AboutCredits,
};

struct SettingEntry {
    SettingId id;
    std::string_view name;       // display name (sidebar / fallback label)
    SettingsSection section;
    std::string_view keywords;   // lowercased searchable blob: section + name + labels +
                                 // option values + sub-labels + synonyms
};

// The full searchable catalog, in body order. Every blob is authored lowercase and
// begins with the section name so a section query ("gameplay", "account") resolves to
// every control in that section (kills the "account hits / gameplay misses" asymmetry).
inline constexpr std::array<SettingEntry, 30> kSettingCatalog{{
    {SettingId::StreetWeights, "Street split weights", SettingsSection::Gameplay,
     "gameplay street split weights pre-flop preflop flop turn river save reset sum applies next game"},
    {SettingId::ChipDenomination, "Chip denomination mode", SettingsSection::Gameplay,
     "gameplay chip denomination mode fixed chip denominations off stake-scaled stake scaled"},
    {SettingId::BetSizingEngine, "Bet sizing engine", SettingsSection::Gameplay,
     "gameplay bet sizing engine aggressor multi-tier tier"},
    {SettingId::DifficultyRange, "Difficulty range", SettingsSection::Gameplay,
     "gameplay difficulty range opponent fold tendency low high"},
    {SettingId::TimePressure, "Time pressure", SettingsSection::Gameplay,
     "gameplay time pressure custom flat seconds clock timer"},
    {SettingId::ShowHud, "Show HUD", SettingsSection::Gameplay,
     "gameplay show hud numbers heads up display"},
    {SettingId::ShowCountdown, "Show countdown timer", SettingsSection::Gameplay,
     "gameplay show countdown timer clock"},
    {SettingId::UnitToggle, "Units (Cash / Big Blinds)", SettingsSection::Units,
     "units cash big blinds bb currency stake"},
    {SettingId::Theme, "Color theme", SettingsSection::Display,
     "display color colour theme palette no limit slate ocean sage"},
    {SettingId::ReduceMotion, "Reduce motion", SettingsSection::Display,
     "display reduce motion accessibility"},
    {SettingId::BackgroundMovement, "Background atmospheric movement", SettingsSection::Display,
     "display background atmospheric movement ambient"},
    {SettingId::ParticleDrift, "Particle drift", SettingsSection::Display,
     "display particle drift particles"},
    {SettingId::MusicType, "Music type", SettingsSection::Audio,
     "audio music type genre lounge jazz classical bossa nova ambient track"},
    {SettingId::Volume, "Volume", SettingsSection::Audio, "audio volume loudness level"},
    {SettingId::MuteAll, "Mute all", SettingsSection::Audio, "audio mute all unmute silence"},
    {SettingId::MuteSfx, "Mute sound effects", SettingsSection::Audio,
     "audio mute sound effects sfx"},
    {SettingId::MuteMusic, "Mute music", SettingsSection::Audio, "audio mute music"},
    {SettingId::DealerArrival, "Dealer arrival animation", SettingsSection::Recap,
     "recap dealer arrival animation"},
    {SettingId::ScreenTransitions, "Screen transitions", SettingsSection::Recap,
     "recap screen transitions transition"},
    {SettingId::DefaultRecapTab, "Default Aggressor recap tab", SettingsSection::Recap,
     "recap default aggressor tab tier 1 summary"},
    {SettingId::ShopVisibility, "Shop button visibility", SettingsSection::Tomatoes,
     "tomatoes shop button visibility show visible"},
    {SettingId::ResetTomatoes, "Reset tomatoes", SettingsSection::Tomatoes,
     "tomatoes reset tomatoes wallet currency"},
    {SettingId::LeaderboardOptIn, "Leaderboard opt-in", SettingsSection::Tomatoes,
     "tomatoes leaderboard opt-in opt in"},
    {SettingId::Account, "Account", SettingsSection::Account,
     "account sign in sign up login log in auth"},
    {SettingId::ConfirmLeaveSite, "Confirm before leaving site", SettingsSection::General,
     "general confirm before leaving site exit active scenario"},
    {SettingId::ResetAll, "Reset all settings", SettingsSection::General,
     "general reset all settings defaults"},
    {SettingId::ResetSection, "Reset section", SettingsSection::General,
     "general reset section"},
    {SettingId::TermsOfService, "Terms of Service", SettingsSection::Legal,
     "legal terms of service tos"},
    {SettingId::PrivacyPolicy, "Privacy Policy", SettingsSection::Legal,
     "legal privacy policy"},
    {SettingId::AboutCredits, "About / Credits", SettingsSection::Legal,
     "legal about credits version attributions"},
}};

// Display label for a section (sidebar + body header).
[[nodiscard]] std::string_view section_label(SettingsSection s) noexcept;

// Display name for a control, or "" for an unknown id.
[[nodiscard]] std::string_view setting_name(SettingId id) noexcept;

// The keyword blob for a control, or "" for an unknown id.
[[nodiscard]] std::string_view setting_keywords(SettingId id) noexcept;

// Case-insensitive SUBSTRING match: true when `query` appears contiguously within
// `keywords`. An empty query matches everything.
[[nodiscard]] bool keyword_match(std::string_view keywords, std::string_view query) noexcept;

// True when this control's keyword blob matches the query (empty query => visible).
[[nodiscard]] bool setting_visible(SettingId id, std::string_view query) noexcept;

// True when at least one control in `section` matches the query.
[[nodiscard]] bool section_has_match(SettingsSection section, std::string_view query) noexcept;

// True when no control anywhere matches the query (the body renders blank).
[[nodiscard]] bool search_is_empty_result(std::string_view query) noexcept;

}  // namespace poker_trainer::settings
