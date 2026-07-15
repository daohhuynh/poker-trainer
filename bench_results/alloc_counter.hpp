#pragma once

// Global operator new / delete instrumentation for the reconciliation benchmark.
//
// Overriding the global allocation functions is the only way to observe EVERY heap
// allocation on a code path, including ones hidden inside std::function / std::vector
// growth. Counting is gated by g_alloc_track so we tally allocations that happen
// ONLY inside the timed reconcile loop, not during setup. Single-threaded bench, so
// a plain bool / uint64_t is sufficient (no atomics needed).

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

namespace bench {
inline bool g_alloc_track = false;      // set true only around the measured region
inline std::uint64_t g_alloc_count = 0; // number of operator new / new[] calls
inline std::uint64_t g_alloc_bytes = 0; // total bytes requested
inline void reset_alloc_counters() { g_alloc_count = 0; g_alloc_bytes = 0; }
}  // namespace bench

// --- global replacement operators (C++ [new.delete]) ---

void* operator new(std::size_t n) {
    if (bench::g_alloc_track) { ++bench::g_alloc_count; bench::g_alloc_bytes += n; }
    void* p = std::malloc(n != 0 ? n : 1);
    if (p == nullptr) throw std::bad_alloc{};
    return p;
}
void* operator new[](std::size_t n) {
    if (bench::g_alloc_track) { ++bench::g_alloc_count; bench::g_alloc_bytes += n; }
    void* p = std::malloc(n != 0 ? n : 1);
    if (p == nullptr) throw std::bad_alloc{};
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
