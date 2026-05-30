#pragma once

// Zone 02 tier-loading orchestration.
//
// TierLoader drives the four-tier lazy-loading strategy (ARCHITECTURE Module 3):
// it loads every asset in a tier serially, retries failed fetches with the
// per-tier exponential backoff defined in tier_config.hpp, and resolves a
// fatally-failed asset per its tier's policy (Tier 1/2 expose an error-screen
// failure for Z05; Tier 3/4 fail silently, leaving the asset Unavailable).
//
// All I/O and timing is injected behind seams so the orchestration logic is
// unit-testable natively without a network or real wall-clock waits:
//   * FetchFn   -- fetch the bytes at a path (production: a CDN fetch wrapper,
//                  which Zone 05 owns; tests: a controllable fake).
//   * DecodeFn  -- turn fetched bytes into a texture (production: decode_png;
//                  tests: a fake, to drive orchestration without real PNGs).
//   * ClockFn   -- a monotonic clock (production: steady_clock; tests: a fake
//                  clock the test advances by hand).
//
// The state machine is single-threaded and pump-driven, matching Emscripten's
// callback model: poll() re-issues retries whose backoff has elapsed. Zone 05
// calls poll() each frame from the main loop; tests call it after advancing the
// fake clock.

#include "assets/asset_paths.hpp"
#include "assets/loader.hpp"
#include "assets/registry.hpp"
#include "assets/texture.hpp"
#include "assets/tier_config.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <future>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace poker_trainer::assets {

// ----- Injected seams -----

// The outcome of one fetch attempt for a single asset.
struct FetchResult {
    bool ok = false;
    std::vector<std::byte> bytes{};  // populated iff ok

    [[nodiscard]] static FetchResult success(std::vector<std::byte> data) {
        return FetchResult{true, std::move(data)};
    }
    [[nodiscard]] static FetchResult failure() { return FetchResult{}; }
};

// Invoked exactly once per fetch with the result. May be called synchronously
// from within the FetchFn (tests) or later (Emscripten's async callback).
using FetchCallback = std::function<void(FetchResult)>;

// Fetch the bytes at an asset-root-relative path, delivering the result through
// the callback. The production implementation wraps a CDN fetch (Zone 05 owns
// the "CDN fetch wrappers" per ZONES.md); tests inject a fake.
using FetchFn = std::function<void(std::string_view path, FetchCallback on_complete)>;

// Decode fetched bytes into a texture. A decode failure is treated exactly like
// a fetch failure (it consumes a retry), so a corrupt download is retried.
using DecodeFn = std::function<std::expected<TextureData, DecodeError>(std::span<const std::byte>)>;

// A monotonic clock. Production: steady_clock::now. Tests: a fake.
using TimePoint = std::chrono::steady_clock::time_point;
using ClockFn = std::function<TimePoint()>;

// Production wiring helpers (native-safe; no Emscripten dependency).
[[nodiscard]] DecodeFn make_png_decoder();
[[nodiscard]] ClockFn make_steady_clock();

class TierLoader {
public:
    // The registry must outlive the loader. fetch/decode/clock must be valid
    // callables.
    TierLoader(AssetRegistry& registry, FetchFn fetch, DecodeFn decode, ClockFn clock);

    TierLoader(const TierLoader&) = delete;
    TierLoader& operator=(const TierLoader&) = delete;
    TierLoader(TierLoader&&) = delete;
    TierLoader& operator=(TierLoader&&) = delete;
    ~TierLoader() = default;

    // Begin serial loading of every asset in `tier`. The returned future becomes
    // ready when the tier finishes (every asset reached Loaded or Unavailable).
    // Expected failures (404 / timeout / decode error) never throw through the
    // future; query has_error_screen_failure() and registry.is_unavailable()
    // for failure outcomes. Calling load_tier again for a tier already complete
    // returns an immediately-ready future; calling it while a tier is in flight
    // attaches an additional waiter to the same in-progress load.
    [[nodiscard]] std::future<void> load_tier(AssetTier tier);

    // Pump the state machine: re-issue any retries whose backoff has elapsed and
    // advance in-flight work. Drive this every frame in production.
    void poll();

    // --- Progress / failure exposure (for Z05's loading and error screens) ---

    [[nodiscard]] bool is_tier_started(AssetTier tier) const noexcept;
    [[nodiscard]] bool is_tier_complete(AssetTier tier) const noexcept;

    [[nodiscard]] std::size_t total_count(AssetTier tier) const noexcept;
    [[nodiscard]] std::size_t loaded_count(AssetTier tier) const noexcept;
    [[nodiscard]] std::size_t unavailable_count(AssetTier tier) const noexcept;

    // Assets that have reached a terminal state (Loaded or Unavailable).
    [[nodiscard]] std::size_t resolved_count(AssetTier tier) const noexcept;

    // Fraction of the tier resolved, in [0, 1]. Z05 fills the loading arc from
    // this. An empty tier reports 1.0.
    [[nodiscard]] float tier_progress(AssetTier tier) const noexcept;

    // True once any asset in an error-screen tier (a tier whose TierConfig sets
    // fatal_failure_policy to anything other than Silent — Tier 1 immediate or
    // Tier 2 deferred-on-use) has fatally failed. Z05 polls this and reads
    // tier_config(...).fatal_failure_policy to decide whether to present the
    // error screen now (Tier 1) or only when a blocked navigation needs the
    // missing asset (Tier 2).
    [[nodiscard]] bool has_error_screen_failure() const noexcept;

private:
    enum class Phase : std::uint8_t {
        Idle,      // ready to issue the next fetch for the current asset
        InFlight,  // a fetch is outstanding, awaiting its callback
        Backoff,   // waiting for the current asset's retry backoff to elapse
    };

    struct TierJob {
        AssetTier tier{AssetTier::Tier1};
        std::vector<AssetId> queue{};
        std::size_t index = 0;           // index into queue of the current asset
        std::uint8_t retries_used = 0;   // retries spent on the current asset
        Phase phase = Phase::Idle;
        TimePoint backoff_deadline{};
        std::vector<std::promise<void>> waiters{};
        bool finished = false;
    };

    static constexpr std::size_t kTierCount = 4;
    static std::size_t tier_slot(AssetTier tier) noexcept;

    void pump();
    void advance_job(TierJob& job, TimePoint now);
    void issue_fetch(TierJob& job);
    void on_fetch_result(AssetTier tier, AssetId id, FetchResult result);
    void handle_attempt_failure(TierJob& job, AssetId id);
    static void finish_job(TierJob& job);

    AssetRegistry& registry_;
    FetchFn fetch_;
    DecodeFn decode_;
    ClockFn clock_;

    std::array<std::optional<TierJob>, kTierCount> jobs_{};
    bool pumping_ = false;  // reentrancy guard for synchronous fetch callbacks
};

}  // namespace poker_trainer::assets
