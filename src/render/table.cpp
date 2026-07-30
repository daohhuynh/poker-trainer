#include "render/table.hpp"

#include "theme/theme_tokens.hpp"

#include <algorithm>

#include <imgui.h>

#include "assets/asset_paths.hpp"
#include "bridge/asset_image.hpp"

namespace poker_trainer::render {

namespace {

[[nodiscard]] ImU32 token_u32(theme::ColorToken token) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(token));
}

// Trace the D-felt outline into the draw-list path: sample the rim from the flat
// wedge's lower edge around the curve to its upper edge, so the open span across
// the right (angle 0) becomes the dealer's STRAIGHT chord when the path closes.
void path_d_felt(ImDrawList* dl, const GameLayout& layout) {
    constexpr int kSteps = 64;
    const float lo = kFeltFlatHalfAngleDeg;            // lower edge of the right wedge
    const float hi = 360.0f - kFeltFlatHalfAngleDeg;   // upper edge of the right wedge
    for (int i = 0; i <= kSteps; ++i) {
        const float a = lo + (hi - lo) * static_cast<float>(i) / static_cast<float>(kSteps);
        const Pt p = rim_spot(layout, a).pos;
        dl->PathLineTo(ImVec2{p.x, p.y});
    }
}

struct FeltBounds {
    ImVec2 min;
    ImVec2 max;
};

// The rim's bounding box, sampled from the same rim_spot() that path_d_felt and
// the seats use. The felt art is authored in this exact projection and stretched
// into this box, so the painted felt edge and the chip stacks agree by
// construction rather than by two sets of hand-tuned numbers agreeing by luck.
//
// Sampling only the curve is sufficient: the dealer's chord is a straight line
// between the first and last sample, so it cannot extend the box.
[[nodiscard]] FeltBounds d_felt_bounds(const GameLayout& layout) {
    constexpr int kSteps = 64;
    const float lo = kFeltFlatHalfAngleDeg;
    const float hi = 360.0f - kFeltFlatHalfAngleDeg;
    const Pt first = rim_spot(layout, lo).pos;
    float min_x = first.x;
    float max_x = first.x;
    float min_y = first.y;
    float max_y = first.y;
    for (int i = 1; i <= kSteps; ++i) {
        const float a = lo + (hi - lo) * static_cast<float>(i) / static_cast<float>(kSteps);
        const Pt p = rim_spot(layout, a).pos;
        min_x = std::min(min_x, p.x);
        max_x = std::max(max_x, p.x);
        min_y = std::min(min_y, p.y);
        max_y = std::max(max_y, p.y);
    }
    return FeltBounds{ImVec2{min_x, min_y}, ImVec2{max_x, max_y}};
}

}  // namespace

void draw_table(ImDrawList* dl, const GameLayout& layout) {
    if (dl == nullptr) {
        return;
    }
    // Room/background scene (background_game.png): the authored first-person scene
    // swaps in here over the full canvas. Placeholder: a dim bg_primary wash.
    if (!bridge::draw_asset_image(dl, ImVec2{0.0f, 0.0f}, ImVec2{layout.w, layout.h},
                                  assets::AssetId::BackgroundGame)) {
        dl->AddRectFilled(ImVec2{0.0f, 0.0f}, ImVec2{layout.w, layout.h},
                          token_u32(theme::ColorToken::BgPrimary));
    }

    // Felt: the authored table art (table_felt.png). It is drawn in the renderer's
    // own first-person D projection — a foreshortened oval, wide at the near rim and
    // narrowing to a small far rim, with the dealer's STRAIGHT chord on the right —
    // and is generated from the same rim_spot() formula by tools/gen_table_felt.py.
    //
    // Because that shape scales only with the canvas width horizontally and only
    // with its height vertically, it is a pure axis-aligned scaling of one
    // normalized form. Stretching the image into the rim's bounding box therefore
    // reproduces it exactly at every window aspect — no distortion, no re-export.
    const FeltBounds felt = d_felt_bounds(layout);
    if (bridge::draw_asset_image(dl, felt.min, felt.max, assets::AssetId::TableFelt)) {
        // Per-theme wash over the authored surface. BgTableFelt is an overlay tint
        // by definition (theme_tokens.hpp) and the architecture lists table felt
        // tint among the themed elements, so the art supplies the surface and the
        // theme supplies its colour cast.
        path_d_felt(dl, layout);
        dl->PathFillConvex(token_u32(theme::ColorToken::BgTableFelt));
        return;
    }

    // Fallback while the texture has not arrived: the asset is still loading, is
    // Unavailable, or there is no GL context at all (the native test build). Draw
    // the procedural D silhouette so the table's SHAPE is still on screen and the
    // chips and seats remain readable against it.
    path_d_felt(dl, layout);
    dl->PathFillConvex(token_u32(theme::ColorToken::BgTableFelt));
    path_d_felt(dl, layout);
    dl->PathStroke(token_u32(theme::ColorToken::BorderDefault), ImDrawFlags_Closed, 3.0f);
}

}  // namespace poker_trainer::render
