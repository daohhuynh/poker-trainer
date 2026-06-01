#include "bridge/cdn_fetch.hpp"

#include "assets/tier_loader.hpp"

#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <emscripten/emscripten.h>

// Production CDN fetch via emscripten_async_wget_data. This is binding code: it
// is compiled only into the WebAssembly app target and held to -Wall -Wextra
// -Werror (not the full -Wconversion baseline) per the gen_placeholders
// precedent in CMakeLists — the C-style emscripten callback boundary would
// otherwise bury the logic in casts without catching real defects.

namespace poker_trainer::bridge {

namespace {

// Heap-carried context for one in-flight fetch: the user's callback plus the
// path (kept alive for the C callback, which only sees a void* arg).
struct FetchContext {
    assets::FetchCallback on_complete;
    std::string path;
};

void on_load(void* arg, void* data, int num_bytes) {
    std::unique_ptr<FetchContext> ctx{static_cast<FetchContext*>(arg)};
    std::vector<std::byte> bytes;
    if (num_bytes > 0 && data != nullptr) {
        const std::size_t n = static_cast<std::size_t>(num_bytes);
        bytes.resize(n);
        std::memcpy(bytes.data(), data, n);
    }
    ctx->on_complete(assets::FetchResult::success(std::move(bytes)));
}

void on_error(void* arg) {
    std::unique_ptr<FetchContext> ctx{static_cast<FetchContext*>(arg)};
    ctx->on_complete(assets::FetchResult::failure());
}

}  // namespace

assets::FetchFn make_cdn_fetch() {
    return [](std::string_view path, assets::FetchCallback on_complete) {
        // The TierLoader passes asset-root-relative paths (e.g.
        // "assets/images/tier1/dealer_button.png"); the CDN/dev server serves
        // them at the same relative URL.
        auto ctx = std::make_unique<FetchContext>(
            FetchContext{std::move(on_complete), std::string{path}});
        const char* url = ctx->path.c_str();
        // async_wget_data takes ownership of `ctx` until exactly one of the
        // callbacks fires (which reclaims it via unique_ptr).
        emscripten_async_wget_data(url, ctx.release(), on_load, on_error);
    };
}

}  // namespace poker_trainer::bridge
