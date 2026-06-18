#pragma once

// Zone 05 — the single AddImage point shared by every image slot in the app:
// Zone 07 backgrounds / logo / Home icon (via screens/render_util.hpp) and Zone
// 08 table / cards / chips / dealer / markers (via the src/render TUs). Routes
// through the texture-bind seam (asset_gl_texture, texture_bind.hpp) so dropping
// real art in later is a zero-code-change file swap.
//
// ImGui-facing, so this header is included only by render translation units
// (which already depend on ImGui in both the wasm and native-test builds); the
// pure `bridge` library never includes it and stays ImGui-free.

#include "bridge/texture_bind.hpp"

#include "assets/asset_paths.hpp"

#include <cstdint>

#include <imgui.h>

namespace poker_trainer::bridge {

// Draw `id`'s art into `dl`, filling the rect [p_min, p_max]. Returns true when
// the texture was available and drawn; false (drawing nothing) when no texture
// exists yet — the asset is still loading, or Unavailable, or there is no GL
// context (native test build) — so the caller draws its own procedural fallback.
inline bool draw_asset_image(ImDrawList* dl, const ImVec2& p_min, const ImVec2& p_max,
                             assets::AssetId id) {
    if (dl == nullptr) {
        return false;
    }
    const std::uint64_t tex = asset_gl_texture(id);
    if (tex == 0) {
        return false;
    }
    dl->AddImage(static_cast<ImTextureID>(tex), p_min, p_max);
    return true;
}

}  // namespace poker_trainer::bridge
