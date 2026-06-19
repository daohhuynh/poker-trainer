#pragma once

#include "assets/asset_paths.hpp"

// Zone 13 — the front-facing dealer on the Post-Round Screen, centered behind the
// stat modal. Butler neutral (pass) / raised (fail); or, when the Frog easter egg
// is active, frog_base with the pass (blush) / fail (tongue) expression overlay
// composited over it. All slots route through the texture-bind seam so a real-art
// swap is a zero-code-change file overwrite.
//
// Forward-declared ImGui types keep this header ImGui-free (mirrors Zone 08's
// render headers); the unit tests never include it.

struct ImDrawList;
struct ImVec2;

namespace poker_trainer::render {

// Draw `id`'s art into [p_min, p_max] at `alpha` (0..1), used for the sequenced
// dealer / modal fade-in that the shared draw_image_slot helper can't express
// (it draws at full opacity). Routes through bridge::asset_gl_texture — the same
// seam draw_asset_image uses — so the swap invariant holds. Returns true when the
// texture was available and drawn; false (drawing nothing) when it is not yet
// loaded or there is no GL context (native build).
bool draw_image_alpha(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, assets::AssetId id,
                      float alpha);

// Parameters for one front-dealer render pass.
struct FrontDealerRender {
    ImVec2* top_left;       // dealer rect, screen pixels
    ImVec2* bottom_right;
    bool pass;              // true -> neutral / blush; false -> raised / tongue
    bool frog_active;       // true -> Frog (base + overlay); false -> Butler
    float dealer_alpha;     // Phase-2 dealer fade-in (1.0 once arrived)
    float overlay_alpha;    // Frog expression overlay fade-in (~200ms if late-loading)
};

// Render the front-facing dealer (Butler variant, or Frog base + expression
// overlay). When the chosen art is unavailable, draws a faint outlined box so the
// composition stays readable.
void draw_front_dealer(ImDrawList* dl, const FrontDealerRender& params);

}  // namespace poker_trainer::render
