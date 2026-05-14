#pragma once

#include <chrono>
#include <cstdint>

namespace poker_trainer::assets {

// Asset loading tiers. Defines when an asset is fetched and how
// failures are handled.
enum class AssetTier : std::uint8_t {
    // Synchronously loaded before the app renders. Required for
    // the loading screen and the Root screen. Failure shows the
    // Error Screen with a Retry button (Z05).
    Tier1 = 1,

    // Asynchronously loaded after Root screen renders. Required
    // before scenarios begin. Failure shows the Error Screen with
    // a Retry button (Z05).
    Tier2 = 2,

    // Loaded when the user clicks from Root to Mode Selection.
    // Required for scenarios but not for the Root screen itself.
    // Failure is silent — the asset is marked unavailable and the
    // feature degrades gracefully (e.g., a missing SFX simply plays
    // no sound; the scenario still runs).
    Tier3 = 3,

    // On-demand loaded when a triggering user action occurs.
    // Used for the Frog easter egg assets and the front-facing
    // dealer assets that appear on the Post-Round Screen. Failure
    // is silent.
    Tier4 = 4,
};

// Loading behavior parameters per tier.
struct TierConfig {
    // Number of retry attempts per asset within this tier before
    // declaring the asset fatally failed (Tier 1/2) or skipping
    // it (Tier 3/4).
    std::uint8_t max_retries;

    // Initial delay before the first retry. Subsequent retries use
    // exponential backoff: this delay, then 2x, then 4x.
    std::chrono::milliseconds initial_retry_delay;

    // If true, a fatal failure in this tier triggers the Error
    // Screen (Z05). If false, the failure is silent and the asset
    // is marked unavailable.
    bool fatal_failure_shows_error_screen;
};

inline constexpr TierConfig kTier1Config{
    .max_retries = 3,
    .initial_retry_delay = std::chrono::milliseconds{500},
    .fatal_failure_shows_error_screen = true,
};

inline constexpr TierConfig kTier2Config{
    .max_retries = 3,
    .initial_retry_delay = std::chrono::milliseconds{500},
    .fatal_failure_shows_error_screen = true,
};

inline constexpr TierConfig kTier3Config{
    .max_retries = 3,
    .initial_retry_delay = std::chrono::milliseconds{500},
    .fatal_failure_shows_error_screen = false,
};

inline constexpr TierConfig kTier4Config{
    .max_retries = 3,
    .initial_retry_delay = std::chrono::milliseconds{500},
    .fatal_failure_shows_error_screen = false,
};

// Helper to look up the config for a given tier.
[[nodiscard]] constexpr const TierConfig& tier_config(AssetTier tier) noexcept {
    switch (tier) {
        case AssetTier::Tier1: return kTier1Config;
        case AssetTier::Tier2: return kTier2Config;
        case AssetTier::Tier3: return kTier3Config;
        case AssetTier::Tier4: return kTier4Config;
    }
    // Unreachable; the switch is exhaustive.
    return kTier1Config;
}

}  // namespace poker_trainer::assets
