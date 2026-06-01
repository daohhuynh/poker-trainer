#pragma once

#include "assets/tier_loader.hpp"

// The production CDN fetch wrapper (ZONES.md: "Zone 05 owns the CDN fetch
// wrappers"). Returns an assets::FetchFn that fetches an asset-root-relative
// path from the CDN and delivers the bytes through the callback. Emscripten-only
// (compiled into the WebAssembly app target, never the native test build): the
// TierLoader's fetch seam is injected with a fake in tests.

namespace poker_trainer::bridge {

// Build the production FetchFn backed by emscripten_async_wget_data. Each fetch
// resolves asynchronously and invokes the FetchCallback exactly once with the
// downloaded bytes (success) or an empty failure result (network / 404 error),
// matching the TierLoader's retry expectations.
[[nodiscard]] assets::FetchFn make_cdn_fetch();

}  // namespace poker_trainer::bridge
