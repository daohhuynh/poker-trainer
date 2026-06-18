#include "render/dealer.hpp"

#include "theme/theme_tokens.hpp"

#include <imgui.h>

#include "assets/asset_paths.hpp"
#include "bridge/asset_image.hpp"

namespace poker_trainer::render {

namespace {

[[nodiscard]] ImU32 token_u32(theme::ColorToken token) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(token));
}

}  // namespace

void draw_dealer(ImDrawList* dl, const GameLayout& layout, bool frog_active) {
    if (dl == nullptr) {
        return;
    }
    const ImVec2 tl{layout.dealer_tl.x, layout.dealer_tl.y};
    const ImVec2 br{layout.dealer_tl.x + layout.dealer_w, layout.dealer_tl.y + layout.dealer_h};

    // Dealer art via the shared texture-bind seam: the side-profile Butler, or the
    // Frog base when the 22-click easter egg is active. The Butler<->Frog swap is
    // a change of which asset id this slot draws, so the swap renders today's
    // placeholder and tomorrow's real art with no code change.
    const assets::AssetId id =
        frog_active ? assets::AssetId::FrogBase : assets::AssetId::ButlerProfile;
    if (bridge::draw_asset_image(dl, tl, br, id)) {
        return;
    }

    // Silhouette body (placeholder for the authored Butler/Frog art). Fixed-token
    // fill so it does not crash or shift when art is absent.
    dl->AddRectFilled(tl, br, token_u32(theme::ColorToken::ButtonBg), 10.0f);
    dl->AddRect(tl, br, token_u32(theme::ColorToken::BorderDefault), 10.0f, 0, 1.5f);

    // A head disc atop the body to read as a figure.
    const float head_r = layout.dealer_w * 0.28f;
    const ImVec2 head{(tl.x + br.x) * 0.5f, tl.y + head_r + 8.0f};
    dl->AddCircleFilled(head, head_r, token_u32(theme::ColorToken::BgModalSurface), 32);
    dl->AddCircle(head, head_r, token_u32(theme::ColorToken::BorderDefault), 32, 1.5f);

    const char* label = frog_active ? "FROG" : "BUTLER";
    const ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2{(tl.x + br.x) * 0.5f - ts.x * 0.5f, br.y - ts.y - 8.0f},
                token_u32(theme::ColorToken::TextButton), label);
}

}  // namespace poker_trainer::render
