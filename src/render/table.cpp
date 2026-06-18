#include "render/table.hpp"

#include "theme/theme_tokens.hpp"

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

    // Felt: the first-person D-table is drawn PROCEDURALLY — a foreshortened oval
    // (wide near rim at the bottom narrowing to a small far rim at the top) with the
    // dealer's STRAIGHT chord on the right — NOT via the flat rectangular table_felt
    // placeholder, so the real table SHAPE is visible and the chips / cards / seats
    // below can be validated against it. The authored first-person table lives in
    // background_game.png (the full scene above); when that lands this procedural
    // felt is what it replaces. Drawn from the SAME rim_spot the seats use, so the
    // felt and the chip stacks always agree.
    path_d_felt(dl, layout);
    dl->PathFillConvex(token_u32(theme::ColorToken::BgTableFelt));
    path_d_felt(dl, layout);
    dl->PathStroke(token_u32(theme::ColorToken::BorderDefault), ImDrawFlags_Closed, 3.0f);
}

}  // namespace poker_trainer::render
