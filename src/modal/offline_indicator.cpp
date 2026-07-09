#include "modal/offline_indicator.hpp"

#include "persistence/sync_state.hpp"

#include "theme/theme_tokens.hpp"

#include <cfloat>

#include <imgui.h>

#include "bridge/screen_dispatch.hpp"

namespace poker_trainer::modal {

namespace {

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

void render_offline_indicator(ImDrawList* dl) {
    // Visible when the last sync ATTEMPT failed (SyncFailing) OR the browser is offline for a
    // signed-in user (bridge::offline_hint — navigator.onLine gated on an authenticated
    // account). The OR means simply dropping connectivity surfaces it, not only a failed
    // fail-push. Both inputs are pure per-frame reads.
    if (dl == nullptr || (!offline_indicator_visible(persistence::read_sync_state().status) &&
                          !bridge::offline_hint())) {
        return;
    }

    // Match the "Training tool, no real money." disclaimer EXACTLY: the small (0.8x) font in
    // text_secondary at ~70% alpha. An informational line, not a control — no fill, no hover
    // state. Laid out (text)(cloud), right-aligned to the bottom-right corner with the cloud
    // essentially flush in the corner. The cloud is drawn procedurally in the same muted
    // token (an image asset can't be alpha-matched to the disclaimer, so no image path here).
    ImFont* font = ImGui::GetFont();
    const float size = ImGui::GetFontSize() * 0.8f;
    ImVec4 c = theme::get_color(theme::ColorToken::TextSecondary);
    c.w *= 0.7f;  // ~70% opacity: passive, matches the disclaimer
    const ImU32 color = ImGui::ColorConvertFloat4ToU32(c);

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float pad = size * 0.5f;   // a hair off the very corner so nothing clips
    const float icon = size * 1.25f;  // cloud square, ~ the text cap height
    const float gap = size * 0.4f;    // between the text and the cloud

    const char* text = "Offline - changes saved locally";  // ASCII only (the font has no em dash)
    const ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);

    // Cloud flush in the bottom-right corner; text to its left, vertically centered.
    const animations::Rect cloud{vp->Pos.x + vp->Size.x - pad - icon,
                                 vp->Pos.y + vp->Size.y - pad - icon, icon, icon};
    const float text_x = cloud.x - gap - ts.x;
    const float text_y = cloud.y + (icon - ts.y) * 0.5f;

    dl->AddText(font, size, ImVec2{text_x, text_y}, color, text);
    draw_cloud_slash(dl, cloud, color);

    // Hover tooltip (manual hit-test over the whole group; drawn on a top-level draw list
    // with no active ImGui window, so IsItemHovered is unavailable here).
    const ImVec2 m = ImGui::GetIO().MousePos;
    if (m.x >= text_x && m.x <= cloud.x + icon && m.y >= cloud.y && m.y <= cloud.y + icon) {
        ImGui::SetTooltip("%s", kOfflineTooltip);
    }
}

}  // namespace poker_trainer::modal
