#include "modal/offline_indicator.hpp"

#include "persistence/sync_state.hpp"

#include "assets/asset_paths.hpp"
#include "theme/theme_tokens.hpp"

#include <imgui.h>

#include "bridge/asset_image.hpp"

namespace poker_trainer::modal {

namespace {

[[nodiscard]] ImU32 token_u32(theme::ColorToken token) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(token));
}

// Procedural cloud-with-slash glyph. ARCHITECTURE leaves the exact icon to the
// visual pass and the sealed asset_paths.hpp has no offline-glyph AssetId, so this
// is drawn rather than an image slot. (Reported.)
void draw_cloud_slash(ImDrawList* dl, const animations::Rect& r, ImU32 color) {
    const float cy = r.y + r.h * 0.58f;
    const float base_r = r.h * 0.18f;
    dl->AddCircleFilled(ImVec2{r.x + r.w * 0.38f, cy}, base_r, color, 12);
    dl->AddCircleFilled(ImVec2{r.x + r.w * 0.6f, cy}, base_r * 1.15f, color, 12);
    dl->AddCircleFilled(ImVec2{r.x + r.w * 0.5f, cy - base_r * 0.7f}, base_r, color, 12);
    dl->AddRectFilled(ImVec2{r.x + r.w * 0.34f, cy}, ImVec2{r.x + r.w * 0.66f, cy + base_r}, color);
    // Diagonal slash.
    dl->AddLine(ImVec2{r.x + r.w * 0.25f, r.y + r.h * 0.8f},
                ImVec2{r.x + r.w * 0.75f, r.y + r.h * 0.3f}, color, 2.0f);
}

}  // namespace

void render_offline_indicator(ImDrawList* dl, const animations::Rect& leftmost_icon_rect) {
    if (!offline_indicator_visible(persistence::read_sync_state().status)) {
        return;
    }

    // Same size as a cluster icon, positioned one icon-width (plus a small gap) to
    // the left of the leftmost cluster icon, glyph-only (no button fill).
    const float gap = leftmost_icon_rect.w * 0.35f;
    const animations::Rect r{leftmost_icon_rect.x - leftmost_icon_rect.w - gap, leftmost_icon_rect.y,
                             leftmost_icon_rect.w, leftmost_icon_rect.h};
    // Image-path/swap model: the authored offline glyph drops in over IconOffline
    // with zero code change; the procedural cloud-with-slash is the is_unavailable
    // fallback (asset still loading, a 404, or the native build with no GL context).
    if (!bridge::draw_asset_image(dl, ImVec2{r.x, r.y}, ImVec2{r.x + r.w, r.y + r.h},
                                  assets::AssetId::IconOffline)) {
        draw_cloud_slash(dl, r, token_u32(theme::ColorToken::TextSecondary));
    }

    // Hover tooltip (manual hit-test; the cluster draws on the background draw list
    // with no active ImGui window, so IsItemHovered is unavailable here).
    const ImVec2 m = ImGui::GetIO().MousePos;
    if (m.x >= r.x && m.x <= r.x + r.w && m.y >= r.y && m.y <= r.y + r.h) {
        ImGui::SetTooltip("%s", kOfflineTooltip);
    }
}

}  // namespace poker_trainer::modal
