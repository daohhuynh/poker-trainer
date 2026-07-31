#include "modal/offline_indicator.hpp"

#include "modal/modal_base.hpp"

#include "persistence/sync_state.hpp"

#include "theme/theme_tokens.hpp"

#include <algorithm>

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

    // Diagonal slash, drawn in two passes. A single line in the glyph's own colour --
    // which is what this did -- lies entirely inside the cloud silhouette and is
    // therefore invisible at every size, leaving a featureless blob. The first pass
    // cuts a channel through the cloud in the screen background tint; the second draws
    // the slash inside that channel. Without the cut the icon reads as weather rather
    // than as a dropped connection, and the glyph is now the only carrier of the
    // message (the text moved to the hover tooltip).
    const ImVec2 from{r.x + r.w * 0.18f, r.y + r.h * 0.84f};
    const ImVec2 to{r.x + r.w * 0.82f, r.y + r.h * 0.24f};
    const float stroke = std::max(1.5f, r.h * 0.10f);
    dl->AddLine(from, to, ImGui::ColorConvertFloat4ToU32(theme::get_color(theme::ColorToken::BgPrimary)),
                stroke * 2.4f);
    dl->AddLine(from, to, color, stroke);
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

    // Anchored under the "Training tool, no real money." disclaimer, whose rect Z11
    // publishes each frame it draws one. Only trusted on the frame it was stamped:
    // screens that draw no disclaimer (Tutorial Complete, Error) get no indicator
    // rather than the previous screen's stale position. render_modal_overlay runs
    // after the screen body in the same ImGui frame, so on the four screens that do
    // draw it the stamp is always current.
    const ModalRuntime* rt = modal_runtime();
    if (rt == nullptr || rt->disclaimer_frame != ImGui::GetFrameCount()) {
        return;
    }
    const animations::Rect& row = rt->disclaimer_rect;

    // Glyph only, no text and no fill — ARCHITECTURE L563 makes this informational
    // rather than interactive, and the message lives in the hover tooltip. Sized and
    // coloured off the disclaimer so the two read as one passive stack.
    // 1.25x the text height was right when a label sat beside it; now the glyph
    // carries the whole message, and at ~13px the cloud's puffs came to about four
    // pixels and read as a smudge. ARCHITECTURE asks for cluster-icon size, which is
    // ~50px here and far too heavy for a passive tag under the disclaimer, so this
    // sits between: unmistakably a cloud, still clearly subordinate to the Home icon.
    const float icon = row.h * 2.2f;
    const animations::Rect cloud{row.x + row.w - icon, row.y + row.h + row.h * 0.35f, icon,
                                 icon};

    ImVec4 c = theme::get_color(theme::ColorToken::TextSecondary);
    c.w *= 0.7f;  // ~70% opacity: passive, matches the disclaimer
    draw_cloud_slash(dl, cloud, ImGui::ColorConvertFloat4ToU32(c));

    // Hover tooltip (manual hit-test: drawn on a top-level draw list with no active
    // ImGui window, so IsItemHovered is unavailable here).
    const ImVec2 m = ImGui::GetIO().MousePos;
    if (m.x >= cloud.x && m.x <= cloud.x + cloud.w && m.y >= cloud.y &&
        m.y <= cloud.y + cloud.h) {
        ImGui::SetTooltip("%s", kOfflineTooltip);
    }
}

}  // namespace poker_trainer::modal
