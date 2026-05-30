#pragma once

#include <array>
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

    // Asynchronously loaded after the Root screen renders. Required
    // before scenarios begin. On fatal failure the asset is marked
    // unavailable and the Error Screen is shown later — only when a
    // navigation or interaction needs the missing asset (Z05).
    Tier2 = 2,

    // Loaded when the user navigates from Root to Mode Selection.
    // Carries no PNG assets: its members are the remaining SFX and the
    // default music track, owned by Z03/audio_paths.hpp. Failure is
    // silent — the asset is marked unavailable and the feature degrades
    // gracefully (e.g., a missing SFX simply plays no sound).
    Tier3 = 3,

    // On-demand loaded when a triggering user action occurs. Used for
    // the Frog easter-egg dealer assets (loaded on the Butler <-> Frog
    // toggle) and the on-demand music tracks (alternate-genre starter
    // tracks and purchased tracks). Failure is silent. The front-facing
    // Butler assets are Tier 1, not Tier 4.
    Tier4 = 4,
};

// What a tier does when an asset fatally fails (all retries exhausted).
enum class FatalFailurePolicy : std::uint8_t {
    // Transition the loading screen to the Error Screen immediately
    // (Tier 1 — assets required before the app can render).
    ErrorScreenImmediate,

    // Mark the asset unavailable; show the Error Screen only when a
    // later navigation or interaction needs it (Tier 2 — deferred).
    ErrorScreenOnUse,

    // Mark the asset unavailable and degrade gracefully with no
    // user-facing error (Tier 3 / Tier 4).
    Silent,
};

// The retry schedule, identical across all four tiers (ARCHITECTURE
// Module 3): on a failed fetch, retry up to three times with the delays
// below, each measured from the preceding failure — immediate, then
// 2 seconds, then 10 seconds.
inline constexpr std::size_t kMaxRetries = 3;
inline constexpr std::array<std::chrono::milliseconds, kMaxRetries> kRetryBackoff{
    std::chrono::milliseconds{0},
    std::chrono::milliseconds{2000},
    std::chrono::milliseconds{10000},
};

// Loading behavior parameters per tier.
struct TierConfig {
    // Number of retry attempts per asset within this tier before
    // declaring the asset fatally failed.
    std::uint8_t max_retries;

    // Delay before each retry, measured from the preceding failure.
    // retry_delays[i] is applied before the (i)-th retry, 0-based.
    std::array<std::chrono::milliseconds, kMaxRetries> retry_delays;

    // How a fatal failure (all retries exhausted) is surfaced.
    FatalFailurePolicy fatal_failure_policy;
};

inline constexpr TierConfig kTier1Config{
    .max_retries = static_cast<std::uint8_t>(kMaxRetries),
    .retry_delays = kRetryBackoff,
    .fatal_failure_policy = FatalFailurePolicy::ErrorScreenImmediate,
};

inline constexpr TierConfig kTier2Config{
    .max_retries = static_cast<std::uint8_t>(kMaxRetries),
    .retry_delays = kRetryBackoff,
    .fatal_failure_policy = FatalFailurePolicy::ErrorScreenOnUse,
};

inline constexpr TierConfig kTier3Config{
    .max_retries = static_cast<std::uint8_t>(kMaxRetries),
    .retry_delays = kRetryBackoff,
    .fatal_failure_policy = FatalFailurePolicy::Silent,
};

inline constexpr TierConfig kTier4Config{
    .max_retries = static_cast<std::uint8_t>(kMaxRetries),
    .retry_delays = kRetryBackoff,
    .fatal_failure_policy = FatalFailurePolicy::Silent,
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
