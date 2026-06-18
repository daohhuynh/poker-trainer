#pragma once

// Zone 07 internal render helpers (shared by the three screen render TUs to
// avoid copy-paste). Header-only, ImGui-dependent — included only by the render
// .cpp files, never by the pure-logic units or their tests. Added per CLAUDE.md
// section 6 (a zone may grow files within its owned directory): it serves Zone
// 07's render scope and introduces no new cross-zone dependency (theme + ImGui
// are already Zone 07 dependencies).
//
// All colors come from theme tokens via get_color; no literal colors here, per
// the theme-system invariant.

#include "animations/button_morph.hpp"

#include "assets/asset_paths.hpp"

#include <cstdint>

#include <imgui.h>

#include "bridge/asset_image.hpp"
#include "theme/theme_tokens.hpp"

namespace poker_trainer::screens::render_util {

[[nodiscard]] inline ImVec2 top_left(const animations::Rect& r) noexcept {
    return ImVec2{r.x, r.y};
}
[[nodiscard]] inline ImVec2 bottom_right(const animations::Rect& r) noexcept {
    return ImVec2{r.x + r.w, r.y + r.h};
}
[[nodiscard]] inline ImU32 token_u32(theme::ColorToken token) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(token));
}

inline void fill_rect(ImDrawList* dl, const animations::Rect& r, theme::ColorToken fill,
                      float rounding = 0.0f) {
    dl->AddRectFilled(top_left(r), bottom_right(r), token_u32(fill), rounding);
}

// alpha multiplies the token color's alpha channel — used for the
// "PLAY" -> "STANDARD" label crossfade during the Root -> Mode Selection morph.
// Defaults to 1.0 (fully opaque), so existing call sites are unchanged; alpha <= 0
// draws nothing.
inline void centered_label(ImDrawList* dl, const animations::Rect& r, const char* text,
                           theme::ColorToken color, float alpha = 1.0f) {
    if (alpha <= 0.0f) {
        return;
    }
    const ImVec2 size = ImGui::CalcTextSize(text);
    const ImVec2 pos{r.x + (r.w - size.x) * 0.5f, r.y + (r.h - size.y) * 0.5f};
    ImVec4 color_rgba = theme::get_color(color);
    color_rgba.w *= alpha;
    dl->AddText(pos, ImGui::ColorConvertFloat4ToU32(color_rgba), text);
}

// 2px focus outline in border_focus. The caller is responsible for only drawing
// it when keyboard mode is active and this element holds focus.
inline void focus_outline(ImDrawList* dl, const animations::Rect& r) {
    dl->AddRect(top_left(r), bottom_right(r), token_u32(theme::ColorToken::BorderFocus),
                0.0f, 0, 2.0f);
}

// A themed button: default-state fill + centered label, with an optional focus
// outline. Tokens per ARCHITECTURE Token Application Rules
// (bg_button_default / text_button / border_focus).
inline void button(ImDrawList* dl, const animations::Rect& r, const char* label, bool focused) {
    fill_rect(dl, r, theme::ColorToken::ButtonBg, 6.0f);
    centered_label(dl, r, label, theme::ColorToken::TextButton);
    if (focused) {
        focus_outline(dl, r);
    }
}

// How an image slot degrades when its art is unavailable (asset still loading, a
// production 404, or the native test build with no GL context): a background
// fills with bg_primary; a logo/icon draws a thin outlined box. With the
// placeholder PNGs on disk the slot renders the real image and the fallback is
// never seen in the app.
enum class SlotFallback : std::uint8_t {
    Background,
    Icon,
};

// ===== The Zone 07 image-slot wrapper over the shared texture-bind seam ========
// Every Zone 07 image slot (blurred background, logo, Home icon) routes through
// the one AddImage point (bridge::draw_asset_image), so dropping real art in
// later is a zero-code-change file swap: the artist overwrites the placeholder
// PNG at the asset id and this exact code draws the new bytes. If the asset is
// unavailable, the procedural fallback keeps the layout readable.
inline void draw_image_slot(ImDrawList* dl, const animations::Rect& r, assets::AssetId id,
                            SlotFallback fallback, bool focused) {
    if (!bridge::draw_asset_image(dl, top_left(r), bottom_right(r), id)) {
        if (fallback == SlotFallback::Background) {
            fill_rect(dl, r, theme::ColorToken::BgPrimary);
        } else {
            dl->AddRect(top_left(r), bottom_right(r), token_u32(theme::ColorToken::TextPrimary),
                        0.0f, 0, 1.0f);
        }
    }
    if (focused) {
        focus_outline(dl, r);
    }
}

}  // namespace poker_trainer::screens::render_util
