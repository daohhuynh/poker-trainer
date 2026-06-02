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

#include <imgui.h>

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

// The Zone 07 image slots that will hold real PNG art once it exists. Each maps
// to one authored asset (logo.png, the Home cluster icon, the blurred Root/Mode
// background); see asset_paths.hpp for the canonical paths.
enum class ImageSlot : std::uint8_t {
    Logo,
    HomeIcon,
    Background,
};

// ===== SEAM(visual-pass / Z05 GPU upload): the ONE texture-bind point ========
// Every Zone 07 image slot (logo, Home icon, blurred background) routes through
// this single function, so dropping real art in later is a one-touch change.
//
// Today no PNG is GPU-uploaded: Z02 decodes pixels into CPU TextureHandles, but
// binding those to ImTextureIDs is Z05's deferred GPU-upload seam. Until that
// lands, this draws a token-colored placeholder (an outlined box for icons, a
// bg_primary wash for the background) so every layout reads correctly and never
// crashes when art is absent. When textures are bound, replace the body below
// with a single `dl->AddImage(tex_for(slot), top_left(r), bottom_right(r))` and
// keep the focus outline — one edit here re-skins all three slots.
inline void draw_image_slot(ImDrawList* dl, const animations::Rect& r, ImageSlot slot,
                            bool focused) {
    // <-- bind real art here: if a texture exists for `slot`, AddImage and skip
    //     the placeholder branch below.
    if (slot == ImageSlot::Background) {
        fill_rect(dl, r, theme::ColorToken::BgPrimary);
    } else {
        dl->AddRect(top_left(r), bottom_right(r), token_u32(theme::ColorToken::TextPrimary), 0.0f,
                    0, 1.0f);
    }
    if (focused) {
        focus_outline(dl, r);
    }
}

}  // namespace poker_trainer::screens::render_util
