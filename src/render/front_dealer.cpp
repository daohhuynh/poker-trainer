#include "render/front_dealer.hpp"

#include "theme/theme_tokens.hpp"

#include "bridge/texture_bind.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <imgui.h>

namespace poker_trainer::render {

namespace {

[[nodiscard]] int alpha_to_byte(float alpha) noexcept {
    const long a = std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
    return static_cast<int>(std::clamp<long>(a, 0, 255));
}

// Faint outlined fallback when the chosen art is not yet loaded (or no GL context),
// so the composition stays readable. Matches render_util's Icon fallback intent.
void draw_outline_fallback(ImDrawList* dl, const ImVec2& p_min, const ImVec2& p_max, float alpha) {
    ImVec4 c = theme::get_color(theme::ColorToken::TextSecondary);
    c.w *= alpha;
    dl->AddRect(p_min, p_max, ImGui::ColorConvertFloat4ToU32(c), 8.0f, 0, 1.0f);
}

}  // namespace

bool draw_image_alpha(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, assets::AssetId id,
                      float alpha) {
    if (dl == nullptr) {
        return false;
    }
    const std::uint64_t tex = bridge::asset_gl_texture(id);
    if (tex == 0) {
        return false;
    }
    const ImU32 col = IM_COL32(255, 255, 255, alpha_to_byte(alpha));
    dl->AddImage(static_cast<ImTextureID>(tex), p_min, p_max, ImVec2{0.0f, 0.0f},
                 ImVec2{1.0f, 1.0f}, col);
    return true;
}

void draw_front_dealer(ImDrawList* dl, const FrontDealerRender& params) {
    const ImVec2 tl = *params.top_left;
    const ImVec2 br = *params.bottom_right;
    const float dealer_alpha = params.dealer_alpha;

    if (params.frog_active) {
        if (!draw_image_alpha(dl, tl, br, assets::AssetId::FrogBase, dealer_alpha)) {
            draw_outline_fallback(dl, tl, br, dealer_alpha);
        }
        const assets::AssetId overlay = params.pass ? assets::AssetId::FrogExpressionPass
                                                    : assets::AssetId::FrogExpressionFail;
        // The overlay respects both the dealer fade and its own (late-load) fade.
        draw_image_alpha(dl, tl, br, overlay, dealer_alpha * params.overlay_alpha);
        return;
    }

    const assets::AssetId butler =
        params.pass ? assets::AssetId::ButlerNeutral : assets::AssetId::ButlerRaised;
    if (!draw_image_alpha(dl, tl, br, butler, dealer_alpha)) {
        draw_outline_fallback(dl, tl, br, dealer_alpha);
    }
}

}  // namespace poker_trainer::render
