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

// Number of rim samples for both the traced path and its bounding box. The rim is
// a closed loop, so sample i == kSteps would repeat i == 0 and is skipped.
constexpr int kRimSteps = 96;

[[nodiscard]] float rim_angle(int i) {
    return 360.0f * static_cast<float>(i) / static_cast<float>(kRimSteps);
}

// Trace the felt outline into the draw-list path: the full closed rim, sampled all
// the way round. The shape is symmetric about the vertical axis — no flat chord.
void path_felt(ImDrawList* dl, const GameLayout& layout) {
    for (int i = 0; i < kRimSteps; ++i) {
        const Pt p = rim_spot(layout, rim_angle(i)).pos;
        dl->PathLineTo(ImVec2{p.x, p.y});
    }
}

struct FeltBounds {
    ImVec2 min;
    ImVec2 max;
};

// The rim's bounding box, sampled from the same rim_spot() that path_felt and the
// seats use. The felt art is authored in this exact projection and stretched into
// this box, so the painted felt edge and the chip stacks agree by construction
// rather than by two sets of hand-tuned numbers agreeing by luck.
[[nodiscard]] FeltBounds felt_bounds(const GameLayout& layout) {
    const Pt first = rim_spot(layout, 0.0f).pos;
    float min_x = first.x;
    float max_x = first.x;
    float min_y = first.y;
    float max_y = first.y;
    for (int i = 1; i < kRimSteps; ++i) {
        const Pt p = rim_spot(layout, rim_angle(i)).pos;
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
    // own first-person projection — a symmetric racetrack oval in mild perspective,
    // its far rim modestly narrower than its near one — and is generated from the
    // same rim_spot() formula by tools/gen_table_felt.py.
    //
    // Because that shape scales only with the canvas width horizontally and only
    // with its height vertically, it is a pure axis-aligned scaling of one
    // normalized form. Stretching the image into the rim's bounding box therefore
    // reproduces it exactly at every window aspect — no distortion, no re-export.
    const FeltBounds felt = felt_bounds(layout);
    if (bridge::draw_asset_image(dl, felt.min, felt.max, assets::AssetId::TableFelt)) {
        // Per-theme wash over the authored surface. BgTableFelt is an overlay tint
        // by definition (theme_tokens.hpp) and the architecture lists table felt
        // tint among the themed elements, so the art supplies the surface and the
        // theme supplies its colour cast.
        path_felt(dl, layout);
        dl->PathFillConvex(token_u32(theme::ColorToken::BgTableFelt));
        return;
    }

    // Fallback while the texture has not arrived: the asset is still loading, is
    // Unavailable, or there is no GL context at all (the native test build). Draw
    // the procedural silhouette so the table's SHAPE is still on screen and the
    // chips and seats remain readable against it.
    path_felt(dl, layout);
    dl->PathFillConvex(token_u32(theme::ColorToken::BgTableFelt));
    path_felt(dl, layout);
    dl->PathStroke(token_u32(theme::ColorToken::BorderDefault), ImDrawFlags_Closed, 3.0f);
}

}  // namespace poker_trainer::render
