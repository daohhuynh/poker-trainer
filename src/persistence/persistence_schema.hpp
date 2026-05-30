#pragma once

#include "engine/scenario_id.hpp"
#include "persistence/sync_state.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace poker_trainer::persistence {

// Schema version. Incremented when the on-disk format of AppState
// changes incompatibly. Z04's load path checks this version and
// either accepts the blob or runs a migration to the current version.
inline constexpr std::uint32_t kCurrentSchemaVersion = 1;

// The minimum schema version this build can read. Blobs older than
// this are rejected with a fresh-state fallback. Currently equal to
// the current version because there are no prior versions to migrate
// from.
inline constexpr std::uint32_t kMinSupportedSchemaVersion = 1;

// Tomatoes accounting. The trainer has two distinct Tomatoes
// counters: Spendable (earned, spent in Shop, decremented on
// purchase) and Lifetime (earned, never decremented, used as the
// leaderboard metric).
struct TomatoesState {
    // Spendable Tomatoes balance. Earned by passing scenarios.
    // Decremented when the user purchases items from the Shop.
    std::uint64_t spendable{0};

    // Lifetime Tomatoes total. Earned by passing scenarios. Never
    // decremented. The leaderboard metric.
    std::uint64_t lifetime{0};
};

// Music library state. Tracks which music tracks the user has
// unlocked via Shop purchase, and which tracks are currently in
// each genre's shuffle pool (the user toggles tracks in and out
// of the pool exclusively via the Shop UI, not the Settings Audio
// section).
struct MusicLibraryState {
    // The set of MusicTrackId values the user has unlocked.
    // Stored as a sorted vector of raw track IDs for compact
    // serialization. Starter tracks (Track ID 0 in each genre)
    // are always considered unlocked regardless of presence in
    // this vector.
    std::vector<std::uint8_t> unlocked_track_ids;

    // The set of MusicTrackId values currently in each genre's
    // shuffle pool. Sorted vector of raw track IDs. By default,
    // each genre's pool contains only the starter track until
    // the user adds others via the Shop UI.
    std::vector<std::uint8_t> active_pool_track_ids;
};

// One entry in the scenario history list. The history is bounded
// (most-recent-N entries) and used to populate the Recap section
// and detect repeat-scenario situations.
struct ScenarioHistoryEntry {
    engine::ScenarioId scenario_id{};
    bool passed{false};
    std::uint32_t elapsed_ms{0};

    // Steady-clock timestamp of completion. Used for ordering
    // and for the Recap section's recency display.
    std::chrono::steady_clock::time_point completed_at{};
};

// The maximum number of scenario history entries retained. Older
// entries are evicted when the list exceeds this size.
inline constexpr std::size_t kMaxScenarioHistoryEntries = 256;

// Account linkage state. Either the user is unauthenticated (guest)
// or authenticated with an Auth0 user ID.
struct AccountState {
    // True when the user has signed in via Auth0. False when in
    // guest mode.
    bool is_authenticated{false};

    // Auth0 user identifier ("sub" claim from the ID token).
    // Empty when is_authenticated is false.
    std::string auth0_user_id;

    // Display name from the Auth0 profile. Used in the Account
    // section of Settings and on the Leaderboard. Empty when in
    // guest mode.
    std::string display_name;

    // Email address from the Auth0 profile. Used for sign-in
    // identification only; never displayed publicly. Empty when
    // in guest mode.
    std::string email;
};

// Tutorial state. Tracks whether the user has been prompted to
// start the tutorial and whether they've completed it.
struct TutorialState {
    // True after the user has been shown the "Take the tutorial?"
    // prompt at first launch, regardless of whether they accepted
    // or skipped. Prevents the prompt from re-appearing.
    bool has_seen_tutorial_prompt{false};

    // True after the user has completed the tutorial (reached the
    // Tutorial Complete screen). Used by Z14 to skip the prompt
    // on subsequent launches.
    bool has_completed_tutorial{false};
};

// The top-level persisted application state. Everything stored
// locally and synced to the backend lives in this struct.
struct AppState {
    // The schema version this blob conforms to. Set to
    // kCurrentSchemaVersion when writing. Checked against the
    // supported range when reading.
    std::uint32_t schema_version{kCurrentSchemaVersion};

    // Settings blob. The full settings struct is too large to
    // inline here; it's serialized separately as a flat blob.
    // Z04 stores this as raw bytes (the serialized form of the
    // Settings struct from settings.hpp); Z12 deserializes on
    // read and serializes on write. The opaque-bytes approach
    // decouples the schema from the settings layout — a settings
    // field can be added without bumping the persistence schema
    // version, as long as the settings serializer handles missing
    // fields with their defaults.
    std::vector<std::uint8_t> settings_blob;

    // Tomatoes state.
    TomatoesState tomatoes;

    // Music library state.
    MusicLibraryState music_library;

    // Scenario history (bounded, most-recent-N).
    std::vector<ScenarioHistoryEntry> scenario_history;

    // Account linkage.
    AccountState account;

    // Tutorial state.
    TutorialState tutorial;
};

// Validation result for a loaded AppState blob.
enum class SchemaValidationResult : std::uint8_t {
    // The blob is valid and matches the current schema version.
    Ok = 0,

    // The blob is valid but older than the current schema version;
    // migration is required before use.
    NeedsMigration = 1,

    // The blob's schema version is older than kMinSupportedSchemaVersion
    // or newer than kCurrentSchemaVersion. The blob cannot be loaded.
    Unsupported = 2,

    // The blob is structurally corrupt (e.g., truncated, invalid
    // checksum). The blob cannot be loaded.
    Corrupt = 3,
};

// Validate a loaded AppState's schema version field. Does not
// validate the contents of individual fields — that's Z04's job
// during the deserialization step.
[[nodiscard]] constexpr SchemaValidationResult validate_schema_version(
    std::uint32_t version) noexcept {
    if (version == kCurrentSchemaVersion) {
        return SchemaValidationResult::Ok;
    }
    if (version >= kMinSupportedSchemaVersion &&
        version < kCurrentSchemaVersion) {
        return SchemaValidationResult::NeedsMigration;
    }
    return SchemaValidationResult::Unsupported;
}

// Returns true if an AppState represents an unauthenticated session.
[[nodiscard]] constexpr bool is_guest_state(const AppState& state) noexcept {
    return !state.account.is_authenticated;
}

}  // namespace poker_trainer::persistence
