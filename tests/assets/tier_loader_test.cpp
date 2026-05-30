// Zone 02 tier-loading orchestration tests.
//
// All I/O and timing is injected, so these tests drive the state machine
// deterministically with no network and no real waits:
//   * CapturingFetch records each fetch and lets the test resolve it by hand.
//   * FakeClock is a time_point the test sets directly.
//   * ok_decode / fail_decode stand in for PNG decoding.
// Tier membership, serial ordering, the retry/backoff schedule, and the
// per-tier fatal-vs-skip branch are all asserted against the frozen
// asset_paths.hpp / tier_config.hpp contracts as the oracle.

#include "assets/tier_loader.hpp"

#include "assets/asset_paths.hpp"
#include "assets/loader.hpp"
#include "assets/registry.hpp"
#include "assets/texture.hpp"
#include "assets/tier_config.hpp"
#include "tools/placeholder_layout.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <future>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "asset_test_util.hpp"

namespace {

namespace pa = poker_trainer::assets;
namespace pt = poker_trainer::assets::test;
using ms = std::chrono::milliseconds;

constexpr std::array<pa::AssetTier, 4> kAllTiers{pa::AssetTier::Tier1, pa::AssetTier::Tier2,
                                                 pa::AssetTier::Tier3, pa::AssetTier::Tier4};

// The contract-defined asset set for a tier, in canonical (AssetId) order.
std::vector<pa::AssetId> tier_assets(pa::AssetTier tier) {
    std::vector<pa::AssetId> ids;
    for (std::size_t i = 0; i < pa::kAssetCount; ++i) {
        const auto id = static_cast<pa::AssetId>(i);
        if (pa::asset_tier(id) == tier) {
            ids.push_back(id);
        }
    }
    return ids;
}

struct FakeClock {
    pa::TimePoint now{};
    void set_ms(long long since_epoch) { now = pa::TimePoint{} + ms{since_epoch}; }
};

pa::ClockFn clock_fn(FakeClock& clock) {
    return [&clock] { return clock.now; };
}

// Decode that always succeeds with a 1x1 RGBA texture (orchestration tests do
// not care about pixels, only about success/failure).
pa::DecodeFn ok_decode() {
    return [](std::span<const std::byte>) -> std::expected<pa::TextureData, pa::DecodeError> {
        return pa::TextureData{1, 1, std::vector<std::byte>(4, std::byte{0})};
    };
}

// Decode that always fails, to exercise the decode-failure-is-a-failed-attempt path.
pa::DecodeFn fail_decode() {
    return [](std::span<const std::byte>) -> std::expected<pa::TextureData, pa::DecodeError> {
        return std::unexpected{pa::DecodeError::DecodeFailed};
    };
}

// A fetch the test resolves by hand. Holds the single outstanding request
// (serial loading guarantees at most one), records the full issue log, and
// flags any attempt to start a second fetch before resolving the first.
struct CapturingFetch {
    struct Issued {
        std::string path;
        pa::TimePoint at;
    };

    std::vector<Issued> log;
    std::optional<pa::FetchCallback> pending_cb;
    std::string pending_path;
    FakeClock* clock = nullptr;
    int concurrency_violations = 0;

    pa::FetchFn fn() {
        return [this](std::string_view path, pa::FetchCallback cb) {
            if (pending_cb.has_value()) {
                ++concurrency_violations;  // a prior fetch was still outstanding
            }
            log.push_back(Issued{std::string{path}, clock != nullptr ? clock->now : pa::TimePoint{}});
            pending_path.assign(path);
            pending_cb = std::move(cb);
        };
    }

    [[nodiscard]] bool has_pending() const { return pending_cb.has_value(); }

    void resolve_success(std::vector<std::byte> bytes) {
        pa::FetchCallback cb = std::move(*pending_cb);
        pending_cb.reset();
        cb(pa::FetchResult::success(std::move(bytes)));
    }

    void resolve_failure() {
        pa::FetchCallback cb = std::move(*pending_cb);
        pending_cb.reset();
        cb(pa::FetchResult::failure());
    }

    [[nodiscard]] std::vector<long long> issue_times_ms(std::string_view path) const {
        std::vector<long long> times;
        for (const Issued& e : log) {
            if (e.path == path) {
                times.push_back(std::chrono::duration_cast<ms>(e.at - pa::TimePoint{}).count());
            }
        }
        return times;
    }
};

pa::FetchFn auto_success_fetch() {
    return [](std::string_view, pa::FetchCallback cb) { cb(pa::FetchResult::success({})); };
}

// ---------------------------------------------------------------------------

TEST(TierLoader, LoadsExactlyTheTierAssetsSeriallyInOrder) {
    for (pa::AssetTier tier : kAllTiers) {
        SCOPED_TRACE(static_cast<int>(tier));
        pa::AssetRegistry reg;
        FakeClock clock;
        CapturingFetch fetch;
        fetch.clock = &clock;
        pa::TierLoader loader(reg, fetch.fn(), ok_decode(), clock_fn(clock));

        std::future<void> done = loader.load_tier(tier);

        std::vector<std::string> fetched_order;
        while (fetch.has_pending()) {
            fetched_order.push_back(fetch.pending_path);
            fetch.resolve_success({});
        }

        std::vector<std::string> expected;
        for (pa::AssetId id : tier_assets(tier)) {
            expected.emplace_back(pa::asset_path(id));
        }

        EXPECT_EQ(fetched_order, expected);
        EXPECT_EQ(fetch.concurrency_violations, 0);  // never two fetches at once
        EXPECT_TRUE(loader.is_tier_complete(tier));
        EXPECT_EQ(done.wait_for(ms{0}), std::future_status::ready);
    }
}

TEST(TierLoader, AllAssetsLoadedOnFullSuccess) {
    for (pa::AssetTier tier : kAllTiers) {
        SCOPED_TRACE(static_cast<int>(tier));
        pa::AssetRegistry reg;
        FakeClock clock;
        pa::TierLoader loader(reg, auto_success_fetch(), ok_decode(), clock_fn(clock));

        std::future<void> done = loader.load_tier(tier);

        EXPECT_EQ(done.wait_for(ms{0}), std::future_status::ready);
        EXPECT_TRUE(loader.is_tier_complete(tier));
        EXPECT_EQ(loader.total_count(tier), tier_assets(tier).size());
        EXPECT_EQ(loader.loaded_count(tier), loader.total_count(tier));
        EXPECT_EQ(loader.unavailable_count(tier), 0u);
        EXPECT_FLOAT_EQ(loader.tier_progress(tier), 1.0f);
        for (pa::AssetId id : tier_assets(tier)) {
            EXPECT_TRUE(reg.is_loaded(id));
        }
    }
    // No error-screen failure when nothing failed.
}

TEST(TierLoader, RetriesThreeTimesWithContractBackoffThenFatallyFails) {
    pa::AssetRegistry reg;
    FakeClock clock;
    CapturingFetch fetch;
    fetch.clock = &clock;
    pa::TierLoader loader(reg, fetch.fn(), ok_decode(), clock_fn(clock));

    const pa::AssetId first = tier_assets(pa::AssetTier::Tier1).front();
    const std::string first_path{pa::asset_path(first)};

    std::future<void> done = loader.load_tier(pa::AssetTier::Tier1);
    ASSERT_TRUE(fetch.has_pending());
    EXPECT_EQ(fetch.pending_path, first_path);

    // Contract retry schedule (tier_config.hpp): delays [immediate, 2s, 10s]
    // measured from each failure, i.e. absolute retry issue times 0, 2000,
    // 12000 ms (the immediate retry fires in the same poll as the failure).
    fetch.resolve_failure();  // initial attempt (t=0) fails -> immediate retry at t=0
    ASSERT_TRUE(fetch.has_pending()) << "immediate (0ms) retry did not fire";
    EXPECT_EQ(fetch.pending_path, first_path);
    fetch.resolve_failure();  // first retry (t=0) fails -> next retry scheduled for t=2000

    const std::array<long long, 2> deadlines{2000, 12000};
    for (long long deadline : deadlines) {
        SCOPED_TRACE(deadline);
        clock.set_ms(deadline - 1);
        loader.poll();
        EXPECT_FALSE(fetch.has_pending()) << "retry fired before its backoff elapsed";

        clock.set_ms(deadline);
        loader.poll();
        ASSERT_TRUE(fetch.has_pending()) << "retry did not fire at its backoff deadline";
        EXPECT_EQ(fetch.pending_path, first_path);
        fetch.resolve_failure();
    }

    // Retries exhausted -> the asset is fatally failed (Unavailable).
    EXPECT_TRUE(reg.is_unavailable(first));

    // Exactly one initial attempt plus max_retries retries, on the contract schedule.
    const std::vector<long long> times = fetch.issue_times_ms(first_path);
    ASSERT_EQ(times.size(), 1u + pa::tier_config(pa::AssetTier::Tier1).max_retries);
    EXPECT_EQ(times, (std::vector<long long>{0, 0, 2000, 12000}));

    // Loading continues past the failed asset; drain the rest successfully.
    while (fetch.has_pending()) {
        fetch.resolve_success({});
    }
    EXPECT_TRUE(loader.is_tier_complete(pa::AssetTier::Tier1));
    EXPECT_EQ(done.wait_for(ms{0}), std::future_status::ready);

    // Tier 1 is an error-screen tier, so the fatal failure is exposed to Z05.
    EXPECT_TRUE(loader.has_error_screen_failure());
    EXPECT_EQ(loader.unavailable_count(pa::AssetTier::Tier1), 1u);
    EXPECT_EQ(loader.loaded_count(pa::AssetTier::Tier1),
              loader.total_count(pa::AssetTier::Tier1) - 1u);
}

TEST(TierLoader, SilentTierFatalFailureIsSilentlySkipped) {
    pa::AssetRegistry reg;
    FakeClock clock;
    CapturingFetch fetch;
    fetch.clock = &clock;
    pa::TierLoader loader(reg, fetch.fn(), ok_decode(), clock_fn(clock));

    // Tier 4 carries the Silent fatal-failure policy (the Frog easter-egg
    // assets). asset_paths.hpp puts no PNG in Tier 3 — its members are SFX and
    // music owned by Z03 — so Tier 4 is the Silent tier exercised here.
    const pa::AssetId first = tier_assets(pa::AssetTier::Tier4).front();

    std::future<void> done = loader.load_tier(pa::AssetTier::Tier4);
    fetch.resolve_failure();  // initial attempt fails -> immediate retry fires
    ASSERT_TRUE(fetch.has_pending());
    fetch.resolve_failure();  // immediate retry fails -> backoff to 2000
    for (long long deadline : {2000, 12000}) {
        clock.set_ms(deadline);
        loader.poll();
        ASSERT_TRUE(fetch.has_pending());
        fetch.resolve_failure();
    }

    EXPECT_TRUE(reg.is_unavailable(first));
    // A Silent tier fails silently: no error screen is signalled.
    EXPECT_FALSE(loader.has_error_screen_failure());

    while (fetch.has_pending()) {
        fetch.resolve_success({});
    }
    EXPECT_TRUE(loader.is_tier_complete(pa::AssetTier::Tier4));
    EXPECT_EQ(done.wait_for(ms{0}), std::future_status::ready);
    EXPECT_EQ(loader.unavailable_count(pa::AssetTier::Tier4), 1u);
    EXPECT_EQ(loader.loaded_count(pa::AssetTier::Tier4),
              loader.total_count(pa::AssetTier::Tier4) - 1u);
}

TEST(TierLoader, DecodeFailureCountsAsAFailedAttempt) {
    pa::AssetRegistry reg;
    FakeClock clock;
    CapturingFetch fetch;
    fetch.clock = &clock;
    // Fetch always succeeds, but decode always fails: the asset should still
    // retry on the backoff schedule and then fatally fail.
    pa::TierLoader loader(reg, fetch.fn(), fail_decode(), clock_fn(clock));

    const pa::AssetId first = tier_assets(pa::AssetTier::Tier4).front();
    const std::string first_path{pa::asset_path(first)};

    [[maybe_unused]] std::future<void> started = loader.load_tier(pa::AssetTier::Tier4);
    ASSERT_TRUE(fetch.has_pending());
    fetch.resolve_success({std::byte{1}});  // fetched, but undecodable -> immediate retry
    ASSERT_TRUE(fetch.has_pending());
    fetch.resolve_success({std::byte{1}});  // immediate retry also undecodable -> backoff to 2000
    for (long long deadline : {2000, 12000}) {
        clock.set_ms(deadline);
        loader.poll();
        ASSERT_TRUE(fetch.has_pending());
        fetch.resolve_success({std::byte{1}});
    }

    EXPECT_TRUE(reg.is_unavailable(first));
    EXPECT_FALSE(reg.is_loaded(first));
    EXPECT_FALSE(loader.has_error_screen_failure());  // Tier 4 is silent
    EXPECT_EQ(fetch.issue_times_ms(first_path).size(),
              1u + pa::tier_config(pa::AssetTier::Tier4).max_retries);
}

TEST(TierLoader, ProgressReflectsResolvedFraction) {
    pa::AssetRegistry reg;
    FakeClock clock;
    CapturingFetch fetch;
    fetch.clock = &clock;
    pa::TierLoader loader(reg, fetch.fn(), ok_decode(), clock_fn(clock));

    EXPECT_FALSE(loader.is_tier_started(pa::AssetTier::Tier1));
    EXPECT_EQ(loader.total_count(pa::AssetTier::Tier1), tier_assets(pa::AssetTier::Tier1).size());
    EXPECT_FLOAT_EQ(loader.tier_progress(pa::AssetTier::Tier1), 0.0f);

    [[maybe_unused]] std::future<void> started = loader.load_tier(pa::AssetTier::Tier1);
    EXPECT_TRUE(loader.is_tier_started(pa::AssetTier::Tier1));

    const std::size_t total = loader.total_count(pa::AssetTier::Tier1);
    ASSERT_GE(total, 3u);
    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(fetch.has_pending());
        fetch.resolve_success({});
    }
    EXPECT_EQ(loader.loaded_count(pa::AssetTier::Tier1), 3u);
    EXPECT_FLOAT_EQ(loader.tier_progress(pa::AssetTier::Tier1),
                    3.0f / static_cast<float>(total));
    EXPECT_FALSE(loader.is_tier_complete(pa::AssetTier::Tier1));

    while (fetch.has_pending()) {
        fetch.resolve_success({});
    }
    EXPECT_FLOAT_EQ(loader.tier_progress(pa::AssetTier::Tier1), 1.0f);
    EXPECT_TRUE(loader.is_tier_complete(pa::AssetTier::Tier1));
}

TEST(TierLoader, LoadTierAgainAfterCompletionReturnsReadyFuture) {
    pa::AssetRegistry reg;
    FakeClock clock;
    pa::TierLoader loader(reg, auto_success_fetch(), ok_decode(), clock_fn(clock));

    std::future<void> first = loader.load_tier(pa::AssetTier::Tier3);
    EXPECT_EQ(first.wait_for(ms{0}), std::future_status::ready);
    ASSERT_TRUE(loader.is_tier_complete(pa::AssetTier::Tier3));

    std::future<void> second = loader.load_tier(pa::AssetTier::Tier3);
    EXPECT_EQ(second.wait_for(ms{0}), std::future_status::ready);
    EXPECT_TRUE(loader.is_tier_complete(pa::AssetTier::Tier3));
    EXPECT_EQ(loader.loaded_count(pa::AssetTier::Tier3), loader.total_count(pa::AssetTier::Tier3));
}

TEST(TierLoader, MultipleTiersOnOneLoaderAreIndependent) {
    pa::AssetRegistry reg;
    FakeClock clock;
    pa::TierLoader loader(reg, auto_success_fetch(), ok_decode(), clock_fn(clock));

    [[maybe_unused]] std::future<void> t1 = loader.load_tier(pa::AssetTier::Tier1);
    [[maybe_unused]] std::future<void> t4 = loader.load_tier(pa::AssetTier::Tier4);

    EXPECT_TRUE(loader.is_tier_complete(pa::AssetTier::Tier1));
    EXPECT_TRUE(loader.is_tier_complete(pa::AssetTier::Tier4));
    EXPECT_FALSE(loader.is_tier_started(pa::AssetTier::Tier2));
    EXPECT_FALSE(loader.is_tier_started(pa::AssetTier::Tier3));

    for (pa::AssetId id : tier_assets(pa::AssetTier::Tier1)) {
        EXPECT_TRUE(reg.is_loaded(id));
    }
    for (pa::AssetId id : tier_assets(pa::AssetTier::Tier4)) {
        EXPECT_TRUE(reg.is_loaded(id));
    }
}

TEST(TierLoader, IntegratesRealDecodeOnPlaceholderBytes) {
    pa::AssetRegistry reg;

    // A synchronous fetch that reads the committed placeholder PNG from disk.
    pa::FetchFn disk_fetch = [](std::string_view path, pa::FetchCallback cb) {
        const std::filesystem::path file = pt::source_dir() / std::filesystem::path{std::string{path}};
        if (!std::filesystem::exists(file)) {
            cb(pa::FetchResult::failure());
            return;
        }
        cb(pa::FetchResult::success(pt::read_file_bytes(file)));
    };

    pa::TierLoader loader(reg, disk_fetch, pa::make_png_decoder(), pa::make_steady_clock());
    std::future<void> done = loader.load_tier(pa::AssetTier::Tier1);

    EXPECT_EQ(done.wait_for(ms{0}), std::future_status::ready);
    EXPECT_TRUE(loader.is_tier_complete(pa::AssetTier::Tier1));
    EXPECT_FALSE(loader.has_error_screen_failure());

    for (pa::AssetId id : tier_assets(pa::AssetTier::Tier1)) {
        SCOPED_TRACE(std::string{pa::asset_path(id)});
        EXPECT_TRUE(reg.is_loaded(id));
        const pa::TextureHandle handle = reg.get_texture(id);
        ASSERT_TRUE(handle.valid());
        const pa::placeholder::Size expected = pa::placeholder::size_for(id);
        EXPECT_EQ(handle.width(), expected.width);
        EXPECT_EQ(handle.height(), expected.height);
    }
}

}  // namespace
