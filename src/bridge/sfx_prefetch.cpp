#include "bridge/sfx_prefetch.hpp"

#include "bridge/cdn_fetch.hpp"

#include "assets/tier_loader.hpp"

#include "audio/audio_paths.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <emscripten/emscripten.h>

// Binding code: compiled only into the wasm app, held to the reduced binding
// baseline (the EM_ASM FS boundary), same as cdn_fetch.cpp.

namespace poker_trainer::bridge {

namespace {

// Write the downloaded sample into MEMFS at its asset-root-relative path, creating
// the parent directory tree first. miniaudio's fopen then finds it (paths resolve
// against the MEMFS root, "/"). Best-effort: a filesystem error leaves the sample
// absent, which Zone 03 already degrades on (a missing sample plays no sound).
void write_sample_to_memfs(const std::string& path, const std::vector<std::byte>& bytes) {
    // clang-format off
    EM_ASM({
        var path = UTF8ToString($0);
        var ptr = $1;
        var len = $2;
        var slash = path.lastIndexOf('/');
        if (slash > 0) {
            try { FS.mkdirTree(path.substring(0, slash)); } catch (e) { /* exists */ }
        }
        try { FS.writeFile(path, HEAPU8.subarray(ptr, ptr + len)); } catch (e) {}
    }, path.c_str(), bytes.data(), bytes.size());
    // clang-format on
}

}  // namespace

void prefetch_sfx_into_memfs() {
    // Reuse the exact production CDN fetch the PNG tier loader uses; the only
    // difference is the destination — bytes go to MEMFS (for miniaudio's fopen)
    // rather than into a decoded texture. Each sample is an independent async fetch.
    const assets::FetchFn fetch = make_cdn_fetch();
    for (const std::string_view sfx : audio::kSfxPaths) {
        const std::string path{sfx};
        fetch(path, [path](assets::FetchResult result) {
            if (!result.ok) {
                return;  // 404 / network error: leave the sample absent (silent skip)
            }
            write_sample_to_memfs(path, result.bytes);
        });
    }
}

}  // namespace poker_trainer::bridge
