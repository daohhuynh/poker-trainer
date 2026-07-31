#include "modal/offline_indicator.hpp"

#include "modal/modal_base.hpp"

#include "backbone/screen_state.hpp"

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

// Which screens draw the "Training tool, no real money." disclaimer, and so give
// the glyph a row to hang under. The rest get the corner fallback.
//
// Keyed on the SCREEN rather than on "was a disclaimer published this frame",
// which is the tempting test but the wrong one: Root's morph frame draws no
// disclaimer either, so that rule would fling the glyph to the far corner and back
// for the duration of every Root -> Mode Selection transition. On a screen that
// does draw one, a frame without it means the whole corner is momentarily empty,
// and the right answer is to go with it.
[[nodiscard]] bool screen_draws_disclaimer(backbone::ScreenId screen) noexcept {
    switch (screen) {
        case backbone::ScreenId::Root:
        case backbone::ScreenId::ModeSelection:
        case backbone::ScreenId::Game:
        case backbone::ScreenId::PostRound:
            return true;
        case backbone::ScreenId::TutorialComplete:
        case backbone::ScreenId::Error:
            return false;
    }
    return false;
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

    // Sized off the same 0.8x font the disclaimer uses, so the glyph is identical
    // whichever branch places it.
    const float text_size = ImGui::GetFontSize() * 0.8f;
    const float icon = text_size * 2.2f;

    animations::Rect cloud{};
    if (screen_draws_disclaimer(backbone::read_screen_state().current)) {
        // Anchored under the "Training tool, no real money." disclaimer, whose rect
        // Z11 publishes each frame it draws one. Only trusted on the frame it was
        // stamped. render_modal_overlay runs after the screen body in the same ImGui
        // frame, so on these screens the stamp is current except during the Root ->
        // Mode morph, which draws no disclaimer at all — nothing to sit under, so
        // nothing is drawn.
        const ModalRuntime* rt = modal_runtime();
        if (rt == nullptr || rt->disclaimer_frame != ImGui::GetFrameCount()) {
            return;
        }
        const animations::Rect& row = rt->disclaimer_rect;
        cloud = animations::Rect{row.x + row.w - icon, row.y + row.h * 1.35f, icon, icon};
    } else {
        // Tutorial Complete and the Error screen draw no cluster and no disclaimer,
        // so there is nothing to hang off. They fall back to the bottom-left corner,
        // inset by the same margins home_icon_rect uses for its corner. Bottom-LEFT
        // specifically: the bottom-right is the Root frog's and the Post-Round Again
        // button's, so a status glyph there would teach two meanings for one corner.
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        cloud = animations::Rect{vp->Pos.x + vp->Size.x * 0.03f,
                                 vp->Pos.y + vp->Size.y * 0.97f - icon, icon, icon};
    }

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
