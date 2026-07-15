// Native micro-benchmark for the focus-reconciliation seam.
//
// WHAT THIS MEASURES: the REAL, unmodified reconciliation functions compiled from
// the shipping source tree:
//   * poker_trainer::bridge::begin_focus_reconcile()  (src/bridge/focus_reconcile.cpp)
//   * poker_trainer::bridge::decide_focus_reconcile()  (src/bridge/focus_registry.cpp)
//   * poker_trainer::backbone::* focus state read via active_focus_or_none()
// These .cpp files are compiled and linked as-is (see build_bench.sh) — this harness
// contains NO reimplementation of the reconcile logic. It only sets up real
// FocusRegistry / focus_manager state and calls the real entry points.
//
// NATIVE-vs-WASM DISCLOSURE: begin_focus_reconcile()'s body has one #ifdef
// __EMSCRIPTEN__ block (a single ImGui::ClearActiveID() call, gated on
// io.WantTextInput). Natively that block is compiled out, so the measured cost is
// the full non-ImGui reconciliation decision (the actual algorithm: a
// focus_manager global read + a linear registry scan + branch). The omitted piece
// is one constant-time ImGui call, not part of the reconcile algorithm and not
// measurable without a live GL/browser context. This is disclosed in REPORT.md.
//
// TIMING METHOD: on Apple Silicon the OS monotonic clock ticks at ~41.7 ns (24 MHz
// timebase), which cannot resolve a single ~ns call. So each SAMPLE is the mean of
// an inner batch of kBatch calls; we collect N_SAMPLES such batch-means and report
// the distribution over them. Total real calls = N_SAMPLES * kBatch (>> 100k).

#include "bridge/focus_registry.hpp"

#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"
#include "backbone/screen_state.hpp"

#include "alloc_counter.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace br = poker_trainer::bridge;
namespace bb = poker_trainer::backbone;

namespace {

// Prevent the optimizer from discarding a value we computed.
template <class T>
inline void do_not_optimize(const T& v) {
    asm volatile("" : : "r,m"(v) : "memory");
}

using Clock = std::chrono::steady_clock;

struct Stats {
    double mean, stddev, min, max, p50, p90, p99, p999, median;
    std::size_t n;
};

Stats summarize(std::vector<double> xs) {
    Stats s{};
    s.n = xs.size();
    double sum = 0.0;
    for (double x : xs) sum += x;
    s.mean = sum / static_cast<double>(xs.size());
    double var = 0.0;
    for (double x : xs) var += (x - s.mean) * (x - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(xs.size()));
    std::sort(xs.begin(), xs.end());
    auto pct = [&](double p) {
        const double idx = p * static_cast<double>(xs.size() - 1);
        const std::size_t lo = static_cast<std::size_t>(idx);
        const std::size_t hi = std::min(lo + 1, xs.size() - 1);
        const double frac = idx - static_cast<double>(lo);
        return xs[lo] * (1.0 - frac) + xs[hi] * frac;
    };
    s.min = xs.front();
    s.max = xs.back();
    s.p50 = pct(0.50);
    s.median = s.p50;
    s.p90 = pct(0.90);
    s.p99 = pct(0.99);
    s.p999 = pct(0.999);
    return s;
}

// Real Game-screen surface: 2 text boxes (Fold%, EV) + 1 non-text bet-size group,
// mirroring the boxes_and_group() fixture in the shipping focus_reconcile_test.cpp.
constexpr bb::FocusableId kBox0 = bb::make_focusable_id("game.fold_pct");
constexpr bb::FocusableId kBox1 = bb::make_focusable_id("game.ev");
constexpr bb::FocusableId kGroup = bb::make_focusable_id("game.bet_size_group");

br::FocusRegistry game_surface() {
    br::FocusRegistry r;
    r.register_element(kBox0, br::FocusableEntry{.is_text_field = true});
    r.register_element(kBox1, br::FocusableEntry{.is_text_field = true});
    r.register_element(kGroup, br::FocusableEntry{.is_text_field = false});
    return r;
}

// Arm focus_manager so active_focus_or_none() returns `focus` (the real read path
// begin_focus_reconcile uses). Registers a base list then snaps to `focus`.
void arm_focus(std::span<const bb::FocusableId> list, bb::FocusableId focus) {
    bb::reset_focus_manager_for_testing();
    bb::register_focus_list(bb::ScreenId::Game, list);
    bb::activate_keyboard_mode();
    bb::snap_focus_to(focus);
}

constexpr int kBatch = 512;

// --- Step 1: time begin_focus_reconcile() (the real per-frame entry point) ---
// `last_synced` is fixed per config; focus_manager is armed to `focus` beforehand.
Stats time_begin(const br::FocusRegistry& reg, bb::FocusableId last_synced,
                 std::size_t n_samples, std::size_t warmup) {
    std::vector<double> samples;
    samples.reserve(n_samples);
    std::uint64_t sink = 0;
    for (std::size_t i = 0; i < n_samples + warmup; ++i) {
        const auto t0 = Clock::now();
        for (int b = 0; b < kBatch; ++b) {
            const br::FocusReconcile r = br::begin_focus_reconcile(reg, last_synced);
            sink += static_cast<std::uint64_t>(r.action) + r.target.value;
        }
        const auto t1 = Clock::now();
        do_not_optimize(sink);
        if (i >= warmup) {
            const double ns =
                std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(t1 - t0)
                    .count();
            samples.push_back(ns / static_cast<double>(kBatch));
        }
    }
    do_not_optimize(sink);
    return summarize(std::move(samples));
}

// --- Step 3: time decide_focus_reconcile() over a registry of `n` elements ---
// Worst case: focus is on the LAST-registered element, so find() scans all n slots.
Stats time_decide_scaling(std::size_t n, std::size_t n_samples, std::size_t warmup,
                          std::vector<bb::FocusableId>& ids_out) {
    br::FocusRegistry reg;
    std::vector<bb::FocusableId> ids;
    ids.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const bb::FocusableId id =
            bb::make_focusable_id("scale.elem." + std::to_string(i));
        ids.push_back(id);
        // Last element is a text field; the rest are non-text stops. The choice does
        // not change the scan length (find() walks to the matched index regardless).
        reg.register_element(
            id, br::FocusableEntry{.is_text_field = (i + 1 == n)});
    }
    ids_out = ids;
    const bb::FocusableId current = ids.back();           // worst case: full scan
    const bb::FocusableId prev = bb::kNoFocus;             // force a change decision
    std::vector<double> samples;
    samples.reserve(n_samples);
    std::uint64_t sink = 0;
    for (std::size_t i = 0; i < n_samples + warmup; ++i) {
        const auto t0 = Clock::now();
        for (int b = 0; b < kBatch; ++b) {
            const br::FocusReconcile r = br::decide_focus_reconcile(reg, prev, current);
            sink += static_cast<std::uint64_t>(r.action) + r.target.value;
        }
        const auto t1 = Clock::now();
        do_not_optimize(sink);
        if (i >= warmup) {
            const double ns =
                std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(t1 - t0)
                    .count();
            samples.push_back(ns / static_cast<double>(kBatch));
        }
    }
    do_not_optimize(sink);
    return summarize(std::move(samples));
}

void print_stats(std::FILE* f, const char* label, const Stats& s) {
    std::fprintf(f,
                 "%-28s n=%zu mean=%.3f median=%.3f p90=%.3f p99=%.3f p99.9=%.3f "
                 "min=%.3f max=%.3f stddev=%.3f (ns/call)\n",
                 label, s.n, s.mean, s.median, s.p90, s.p99, s.p999, s.min, s.max, s.stddev);
}

void json_stats(std::string& out, const char* key, const Stats& s, int batch,
                std::uint64_t total_calls) {
    char buf[512];
    std::snprintf(buf, sizeof(buf),
                  "    \"%s\": {\"samples\": %zu, \"batch\": %d, \"total_calls\": %llu, "
                  "\"mean_ns\": %.4f, \"median_ns\": %.4f, \"p50_ns\": %.4f, \"p90_ns\": %.4f, "
                  "\"p99_ns\": %.4f, \"p99_9_ns\": %.4f, \"min_ns\": %.4f, \"max_ns\": %.4f, "
                  "\"stddev_ns\": %.4f}",
                  key, s.n, batch, static_cast<unsigned long long>(total_calls), s.mean,
                  s.median, s.p50, s.p90, s.p99, s.p999, s.min, s.max, s.stddev);
    out += buf;
}

}  // namespace

int main() {
    const std::size_t kSamples = 200000;  // batch-samples -> 200000*512 = 102.4M calls
    const std::size_t kWarmup = 10000;

    const br::FocusRegistry reg = game_surface();
    const bb::FocusableId list[] = {kBox0, kBox1, kGroup};

    // ---- Step 1: headline per-frame reconcile cost, two real frame cases ----
    // A) steady frame: focus unchanged on a text box -> decision None (the ~99% case)
    arm_focus(list, kBox0);
    const Stats steady = time_begin(reg, /*last_synced=*/kBox0, kSamples, kWarmup);
    // B) focus-change frame: focus just moved onto the non-text bet group ->
    //    YieldKeyboard, full 3-element scan (the heavier real per-frame case)
    arm_focus(list, kGroup);
    const Stats change = time_begin(reg, /*last_synced=*/kBox0, kSamples, kWarmup);

    // ---- Step 2: allocations during the reconcile loop (expect ZERO) ----
    arm_focus(list, kBox0);
    bench::reset_alloc_counters();
    bench::g_alloc_track = true;
    std::uint64_t sink = 0;
    const std::size_t kAllocIters = 1000000;  // 1M real calls, tracked
    for (std::size_t i = 0; i < kAllocIters; ++i) {
        const br::FocusReconcile r = br::begin_focus_reconcile(reg, kBox0);
        sink += static_cast<std::uint64_t>(r.action) + r.target.value;
    }
    bench::g_alloc_track = false;
    do_not_optimize(sink);
    const std::uint64_t alloc_count = bench::g_alloc_count;
    const std::uint64_t alloc_bytes = bench::g_alloc_bytes;

    // ---- Step 3: scaling sweep over registry size ----
    const std::size_t counts[] = {1, 4, 8, 16, 32, 64, 128};
    std::vector<std::pair<std::size_t, Stats>> sweep;
    for (std::size_t n : counts) {
        std::vector<bb::FocusableId> ids;
        // Fewer samples for the sweep is fine; still >> 100k calls each.
        const Stats s = time_decide_scaling(n, /*n_samples=*/100000, kWarmup, ids);
        sweep.emplace_back(n, s);
    }

    // ---- console output ----
    std::printf("=== Step 1: begin_focus_reconcile (real per-frame entry) ===\n");
    print_stats(stdout, "steady-frame (None)", steady);
    print_stats(stdout, "change-frame (YieldKeyboard)", change);
    std::printf("\n=== Step 2: allocations during %zu tracked reconcile calls ===\n", kAllocIters);
    std::printf("alloc_count=%llu  alloc_bytes=%llu  (per call: %.6f allocs)\n",
                static_cast<unsigned long long>(alloc_count),
                static_cast<unsigned long long>(alloc_bytes),
                static_cast<double>(alloc_count) / static_cast<double>(kAllocIters));
    std::printf("\n=== Step 3: scaling sweep (decide_focus_reconcile, worst-case full scan) ===\n");
    for (auto& [n, s] : sweep) {
        char lbl[32];
        std::snprintf(lbl, sizeof(lbl), "n=%zu", n);
        print_stats(stdout, lbl, s);
    }

    // ---- CSV: scaling series ----
    {
        std::FILE* f = std::fopen("bench_results/scaling_sweep.csv", "w");
        std::fprintf(f, "n_elements,mean_ns,median_ns,p90_ns,p99_ns,min_ns,max_ns,stddev_ns\n");
        for (auto& [n, s] : sweep) {
            std::fprintf(f, "%zu,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n", n, s.mean, s.median,
                         s.p90, s.p99, s.min, s.max, s.stddev);
        }
        std::fclose(f);
    }

    // ---- JSON: steps 1-3 ----
    {
        std::string j = "{\n  \"step1_reconcile\": {\n";
        json_stats(j, "steady_frame_none", steady, kBatch,
                   static_cast<std::uint64_t>(kSamples) * kBatch);
        j += ",\n";
        json_stats(j, "change_frame_yield", change, kBatch,
                   static_cast<std::uint64_t>(kSamples) * kBatch);
        j += "\n  },\n";
        char ab[256];
        std::snprintf(ab, sizeof(ab),
                      "  \"step2_allocation\": {\"tracked_calls\": %zu, \"alloc_count\": %llu, "
                      "\"alloc_bytes\": %llu},\n",
                      kAllocIters, static_cast<unsigned long long>(alloc_count),
                      static_cast<unsigned long long>(alloc_bytes));
        j += ab;
        j += "  \"step3_scaling\": [\n";
        for (std::size_t i = 0; i < sweep.size(); ++i) {
            char row[512];
            std::snprintf(row, sizeof(row),
                          "    {\"n\": %zu, \"mean_ns\": %.4f, \"median_ns\": %.4f, "
                          "\"p99_ns\": %.4f, \"stddev_ns\": %.4f}%s\n",
                          sweep[i].first, sweep[i].second.mean, sweep[i].second.median,
                          sweep[i].second.p99, sweep[i].second.stddev,
                          i + 1 == sweep.size() ? "" : ",");
            j += row;
        }
        j += "  ]\n}\n";
        std::FILE* f = std::fopen("bench_results/reconcile_bench.json", "w");
        std::fputs(j.c_str(), f);
        std::fclose(f);
    }

    std::printf("\nWrote bench_results/reconcile_bench.json and bench_results/scaling_sweep.csv\n");
    return 0;
}
