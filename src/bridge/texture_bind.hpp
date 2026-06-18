#pragma once

// Zone 05 — the art-texture GPU-upload seam that gl_renderer.hpp explicitly
// defers ("the Z02 art-texture upload is its own seam"). Zone 02 decodes PNGs to
// CPU-resident RGBA8 (assets::TextureHandle); this uploads those pixels to a
// WebGL2 texture once, caches the GL name per AssetId, and hands ImGui an
// ImTextureID for AddImage. A real-art swap is a rebuild (the placeholder PNG at
// the asset path is overwritten with authored art at the same id), so the cache
// needs no runtime invalidation — the exact same code draws the new bytes.
//
// Declared ImGui-free (returns the GL texture name as a plain integer) so the
// pure, native-testable `bridge` library can supply a no-op stub — there is no GL
// context under the native test compiler — without pulling ImGui into it. The
// ImGui-facing draw helper that turns this into an AddImage call lives in
// asset_image.hpp. The real WebGL2 implementation is in texture_bind.cpp,
// compiled only into the wasm bridge_platform layer.

#include "assets/asset_paths.hpp"

#include <cstdint>

namespace poker_trainer::bridge {

// Upload-once-and-cache the decoded pixels for `id` and return its GL texture
// name (nonzero). Returns 0 when `id` is not Loaded (still in flight, or
// Unavailable after a fatal CDN failure), so callers fall back to a procedural
// draw. Requires a current WebGL2 context in the wasm build; the native stub
// always returns 0 (no GL context under the test compiler).
[[nodiscard]] std::uint64_t asset_gl_texture(assets::AssetId id);

}  // namespace poker_trainer::bridge
