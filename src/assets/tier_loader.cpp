#include "assets/tier_loader.hpp"

#include "assets/asset_paths.hpp"
#include "assets/loader.hpp"
#include "assets/registry.hpp"
#include "assets/texture.hpp"
#include "assets/tier_config.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <future>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace poker_trainer::assets {

namespace {

// The ordered set of assets belonging to a tier, in AssetId order (which is the
// canonical fetch order for serial loading). tier_config.hpp / asset_paths.hpp
// are the single source of truth for tier membership.
[[nodiscard]] std::vector<AssetId> assets_in_tier(AssetTier tier) {
    std::vector<AssetId> ids;
    for (std::size_t i = 0; i < kAssetCount; ++i) {
        const auto id = static_cast<AssetId>(i);
        if (asset_tier(id) == tier) {
            ids.push_back(id);
        }
    }
    return ids;
}

[[nodiscard]] std::size_t count_in_tier(AssetTier tier) noexcept {
    std::size_t count = 0;
    for (std::size_t i = 0; i < kAssetCount; ++i) {
        if (asset_tier(static_cast<AssetId>(i)) == tier) {
            ++count;
        }
    }
    return count;
}

// Backoff before the (retry_index)-th retry, 0-based: initial * 2^retry_index.
// Computed by repeated doubling so the schedule follows tier_config.hpp's
// "this delay, then 2x, then 4x" without risking an integer-shift overflow for
// large (hypothetical) retry counts.
[[nodiscard]] std::chrono::milliseconds backoff_delay(std::chrono::milliseconds initial,
                                                      std::uint8_t retry_index) noexcept {
    std::chrono::milliseconds delay = initial;
    for (std::uint8_t i = 0; i < retry_index; ++i) {
        delay *= 2;
    }
    return delay;
}

}  // namespace

DecodeFn make_png_decoder() {
    return [](std::span<const std::byte> bytes) { return decode_png(bytes); };
}

ClockFn make_steady_clock() {
    return [] { return std::chrono::steady_clock::now(); };
}

std::size_t TierLoader::tier_slot(AssetTier tier) noexcept {
    // AssetTier is 1-based (Tier1..Tier4); map to a 0-based array slot.
    return static_cast<std::size_t>(tier) - 1;
}

TierLoader::TierLoader(AssetRegistry& registry, FetchFn fetch, DecodeFn decode, ClockFn clock)
    : registry_{registry},
      fetch_{std::move(fetch)},
      decode_{std::move(decode)},
      clock_{std::move(clock)} {}

std::future<void> TierLoader::load_tier(AssetTier tier) {
    std::optional<TierJob>& slot = jobs_[tier_slot(tier)];

    std::promise<void> promise;
    std::future<void> future = promise.get_future();

    if (slot.has_value() && slot->finished) {
        // Already fully resolved: satisfy the new waiter immediately.
        promise.set_value();
        return future;
    }

    if (!slot.has_value()) {
        TierJob job;
        job.tier = tier;
        job.queue = assets_in_tier(tier);
        job.finished = job.queue.empty();
        slot.emplace(std::move(job));
    }

    slot->waiters.push_back(std::move(promise));

    if (slot->finished) {
        finish_job(*slot);  // empty tier: nothing to load
    } else {
        pump();
    }
    return future;
}

void TierLoader::poll() { pump(); }

void TierLoader::pump() {
    if (pumping_) {
        // A synchronous fetch callback re-entered pump(); the outer pump loop
        // will pick up the state change. Avoids unbounded recursion when fetches
        // resolve synchronously (as test fakes do).
        return;
    }
    pumping_ = true;
    const TimePoint now = clock_();
    for (std::optional<TierJob>& slot : jobs_) {
        if (slot.has_value() && !slot->finished) {
            advance_job(*slot, now);
        }
    }
    pumping_ = false;
}

void TierLoader::advance_job(TierJob& job, TimePoint now) {
    // Drive this job as far as it can synchronously progress, then return when it
    // is blocked on an outstanding fetch, an unelapsed backoff, or completion.
    while (true) {
        if (job.index >= job.queue.size()) {
            finish_job(job);
            return;
        }
        switch (job.phase) {
            case Phase::Idle:
                issue_fetch(job);
                if (job.phase == Phase::InFlight) {
                    return;  // async fetch outstanding; resume from its callback
                }
                break;  // a synchronous callback already advanced state; loop on
            case Phase::InFlight:
                return;  // awaiting the fetch callback
            case Phase::Backoff:
                if (now >= job.backoff_deadline) {
                    job.phase = Phase::Idle;  // backoff elapsed; reissue
                    break;
                }
                return;  // backoff not yet elapsed
        }
    }
}

void TierLoader::issue_fetch(TierJob& job) {
    const AssetId id = job.queue[job.index];
    const AssetTier tier = job.tier;
    registry_.mark_loading(id);
    job.phase = Phase::InFlight;
    // Key the callback by (tier, id) rather than capturing the job pointer: the
    // job lives in the stable jobs_ array, and the (tier, id) pair lets the
    // handler reject a stale callback if state has moved on.
    fetch_(asset_path(id),
           [this, tier, id](FetchResult result) { on_fetch_result(tier, id, std::move(result)); });
}

void TierLoader::on_fetch_result(AssetTier tier, AssetId id, FetchResult result) {
    std::optional<TierJob>& slot = jobs_[tier_slot(tier)];
    if (!slot.has_value() || slot->finished) {
        return;  // stale callback for a finished/absent job
    }
    TierJob& job = *slot;
    if (job.phase != Phase::InFlight || job.index >= job.queue.size() ||
        job.queue[job.index] != id) {
        return;  // stale callback (already advanced or duplicated)
    }

    bool loaded = false;
    if (result.ok) {
        std::expected<TextureData, DecodeError> decoded =
            decode_(std::span<const std::byte>{result.bytes});
        if (decoded.has_value()) {
            registry_.store(id, std::move(decoded).value());
            loaded = true;
        }
    }

    if (loaded) {
        job.index += 1;
        job.retries_used = 0;
        job.phase = Phase::Idle;
    } else {
        handle_attempt_failure(job, id);
    }

    // In the synchronous-fetch path pumping_ is set and the outer pump loop
    // continues; in the async path it is clear and we resume the machine here.
    if (!pumping_) {
        pump();
    }
}

void TierLoader::handle_attempt_failure(TierJob& job, AssetId id) {
    const TierConfig& cfg = tier_config(job.tier);
    if (job.retries_used < cfg.max_retries) {
        job.backoff_deadline = clock_() + backoff_delay(cfg.initial_retry_delay, job.retries_used);
        job.retries_used += 1;
        job.phase = Phase::Backoff;
        return;
    }

    // Retries exhausted: this asset has fatally failed. Mark it Unavailable
    // regardless of tier; the error-screen-vs-silent distinction is read back
    // from the tier config by has_error_screen_failure(). Loading then continues
    // with the next asset in the tier.
    registry_.mark_unavailable(id);
    job.index += 1;
    job.retries_used = 0;
    job.phase = Phase::Idle;
}

void TierLoader::finish_job(TierJob& job) {
    job.finished = true;
    for (std::promise<void>& waiter : job.waiters) {
        waiter.set_value();
    }
    job.waiters.clear();
}

bool TierLoader::is_tier_started(AssetTier tier) const noexcept {
    return jobs_[tier_slot(tier)].has_value();
}

bool TierLoader::is_tier_complete(AssetTier tier) const noexcept {
    const std::optional<TierJob>& slot = jobs_[tier_slot(tier)];
    return slot.has_value() && slot->finished;
}

std::size_t TierLoader::total_count(AssetTier tier) const noexcept {
    return count_in_tier(tier);
}

std::size_t TierLoader::loaded_count(AssetTier tier) const noexcept {
    std::size_t count = 0;
    for (std::size_t i = 0; i < kAssetCount; ++i) {
        const auto id = static_cast<AssetId>(i);
        if (asset_tier(id) == tier && registry_.is_loaded(id)) {
            ++count;
        }
    }
    return count;
}

std::size_t TierLoader::unavailable_count(AssetTier tier) const noexcept {
    std::size_t count = 0;
    for (std::size_t i = 0; i < kAssetCount; ++i) {
        const auto id = static_cast<AssetId>(i);
        if (asset_tier(id) == tier && registry_.is_unavailable(id)) {
            ++count;
        }
    }
    return count;
}

std::size_t TierLoader::resolved_count(AssetTier tier) const noexcept {
    return loaded_count(tier) + unavailable_count(tier);
}

float TierLoader::tier_progress(AssetTier tier) const noexcept {
    const std::size_t total = total_count(tier);
    if (total == 0) {
        return 1.0f;
    }
    return static_cast<float>(resolved_count(tier)) / static_cast<float>(total);
}

bool TierLoader::has_error_screen_failure() const noexcept {
    for (std::size_t i = 0; i < kAssetCount; ++i) {
        const auto id = static_cast<AssetId>(i);
        if (registry_.is_unavailable(id) &&
            tier_config(asset_tier(id)).fatal_failure_shows_error_screen) {
            return true;
        }
    }
    return false;
}

}  // namespace poker_trainer::assets
