#include "modal/modals.hpp"

#include "modal/modal_base.hpp"
#include "modal/offline_indicator.hpp"

#include "backbone/focus_manager.hpp"
#include "backbone/screen_state.hpp"

#include "assets/asset_paths.hpp"
#include "theme/theme_tokens.hpp"

#include <cstddef>
#include <optional>

#include <imgui.h>

#include "bridge/asset_image.hpp"

// Zone 11 — the persistent top-right cluster: rendering (icon glyph on Game /
// Post-Round, Zone 07's morph-handoff button look on Mode Selection), the offline
// indicator, hit-testing, focus->icon resolution, and the icon actions. The screens
// call render_persistent_cluster from their renderers (so positions conform to each
// screen's layout) and route their mouse/keyboard cluster input through
// cluster_hit_test / the Z11 cluster key handler. This module takes no dependency on
// the screen zones (the geometry + ids are passed in via ClusterContext).

namespace poker_trainer::modal {

namespace {

[[nodiscard]] ImU32 token_u32(theme::ColorToken token) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(token));
}

[[nodiscard]] ImVec2 tl(const animations::Rect& r) { return ImVec2{r.x, r.y}; }
[[nodiscard]] ImVec2 br(const animations::Rect& r) { return ImVec2{r.x + r.w, r.y + r.h}; }

[[nodiscard]] bool point_in(const animations::Rect& r, float x, float y) {
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

[[nodiscard]] ClusterIcon icon_for_index(ClusterScreen screen, std::size_t i) {
    switch (i) {
        case 0: return ClusterIcon::Shop;
        case 1: return ClusterIcon::Help;
        case 2: return ClusterIcon::Settings;
        default: return screen == ClusterScreen::Game ? ClusterIcon::Close : ClusterIcon::Home;
    }
}

[[nodiscard]] assets::AssetId asset_for_icon(ClusterIcon icon) {
    switch (icon) {
        case ClusterIcon::Shop: return assets::AssetId::IconShop;
        case ClusterIcon::Help: return assets::AssetId::IconHelp;
        case ClusterIcon::Settings: return assets::AssetId::IconSettings;
        case ClusterIcon::Home: return assets::AssetId::IconHome;
        case ClusterIcon::Close: return assets::AssetId::IconClose;
    }
    return assets::AssetId::IconShop;
}

[[nodiscard]] const char* button_label(ClusterIcon icon) {
    switch (icon) {
        case ClusterIcon::Shop: return "Shop";
        case ClusterIcon::Help: return "Help";
        case ClusterIcon::Settings: return "Settings";
        default: return "";  // Home / Close render as glyphs even in MorphButton style
    }
}

[[nodiscard]] bool focused_on(backbone::FocusableId id) {
    return backbone::is_keyboard_mode_active() && backbone::get_focused_element() == id;
}

void draw_focus_outline(ImDrawList* dl, const animations::Rect& r) {
    dl->AddRect(tl(r), br(r), token_u32(theme::ColorToken::BorderFocus), 4.0f, 0, 2.0f);
}

// Icon-glyph slot: no default fill, bg_button_hover on hover, text_primary glyph
// (via the image-slot seam; thin outlined box fallback when art is unavailable).
void draw_glyph_icon(ImDrawList* dl, const animations::Rect& r, assets::AssetId asset, bool focused,
                     bool hovered) {
    if (hovered) {
        dl->AddRectFilled(tl(r), br(r), token_u32(theme::ColorToken::ButtonBgHover), 4.0f);
    }
    if (!bridge::draw_asset_image(dl, tl(r), br(r), asset)) {
        dl->AddRect(tl(r), br(r), token_u32(theme::ColorToken::TextPrimary), 0.0f, 0, 1.0f);
    }
    if (focused) {
        draw_focus_outline(dl, r);
    }
}

// Mode Selection morph-handoff button: bg_button_default fill (bg_button_hover on
// hover) + centered text_button label. Mirrors Zone 07's render so the Root->Mode
// morph hands off without a pop.
void draw_button_icon(ImDrawList* dl, const animations::Rect& r, const char* label, bool focused,
                      bool hovered) {
    const theme::ColorToken fill =
        hovered ? theme::ColorToken::ButtonBgHover : theme::ColorToken::ButtonBg;
    dl->AddRectFilled(tl(r), br(r), token_u32(fill), 6.0f);
    const ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2{r.x + (r.w - ts.x) * 0.5f, r.y + (r.h - ts.y) * 0.5f},
                token_u32(theme::ColorToken::TextButton), label);
    if (focused) {
        draw_focus_outline(dl, r);
    }
}

}  // namespace

void render_persistent_cluster(ImDrawList* dl, const ClusterContext& ctx) {
    ModalRuntime* rt = modal_runtime();
    if (rt != nullptr) {
        rt->cluster = ctx;
        rt->has_cluster = true;
    }

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    for (std::size_t i = 0; i < ctx.rects.size(); ++i) {
        const ClusterIcon icon = icon_for_index(ctx.screen, i);
        const animations::Rect& r = ctx.rects[i];
        const bool hovered = point_in(r, mouse.x, mouse.y);
        const bool focused = focused_on(ctx.ids[i]);
        // Mode Selection draws Shop/Help/Settings as morph buttons; Home/Close (and
        // every Game / Post-Round icon) render as glyphs.
        const bool as_button =
            ctx.style == ClusterStyle::MorphButton && icon != ClusterIcon::Home &&
            icon != ClusterIcon::Close;
        if (as_button) {
            draw_button_icon(dl, r, button_label(icon), focused, hovered);
        } else {
            draw_glyph_icon(dl, r, asset_for_icon(icon), focused, hovered);
        }
    }

    // Offline sync indicator, left of the leftmost (Shop) icon. Self-gates on
    // sync_state; never reached on Root (the cluster is not drawn there).
    render_offline_indicator(dl, ctx.rects[0]);
}

std::optional<ClusterIcon> cluster_hit_test(float x, float y) {
    const ModalRuntime* rt = modal_runtime();
    if (rt == nullptr || !rt->has_cluster) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < rt->cluster.rects.size(); ++i) {
        if (point_in(rt->cluster.rects[i], x, y)) {
            return icon_for_index(rt->cluster.screen, i);
        }
    }
    return std::nullopt;
}

std::optional<ClusterIcon> cluster_action_for_focus(backbone::FocusableId focused) {
    const ModalRuntime* rt = modal_runtime();
    if (rt == nullptr || !rt->has_cluster) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < rt->cluster.ids.size(); ++i) {
        if (rt->cluster.ids[i] == focused) {
            return icon_for_index(rt->cluster.screen, i);
        }
    }
    return std::nullopt;
}

void activate_cluster_icon(ClusterIcon icon) {
    switch (icon) {
        case ClusterIcon::Shop: open_shop_modal(); return;
        case ClusterIcon::Help: open_help_modal(); return;
        case ClusterIcon::Settings: open_settings_modal(); return;
        case ClusterIcon::Home:
            // Return to Root. SEAM(Z14): the ceremonial transition; instant cut here.
            backbone::set_screen(backbone::ScreenId::Root, std::nullopt);
            return;
        case ClusterIcon::Close: open_leave_drill_confirm(); return;
    }
}

}  // namespace poker_trainer::modal
