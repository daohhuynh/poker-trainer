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

inline void centered_label(ImDrawList* dl, const animations::Rect& r, const char* text,
                           theme::ColorToken color) {
    const ImVec2 size = ImGui::CalcTextSize(text);
    const ImVec2 pos{r.x + (r.w - size.x) * 0.5f, r.y + (r.h - size.y) * 0.5f};
    dl->AddText(pos, token_u32(color), text);
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

// A small placeholder glyph box for an icon whose texture binding is the Zone 05
// GPU-upload seam (the asset registry is injected by Zone 05; CPU TextureHandles
// from get_texture are not yet bound to ImTextureIDs). Drawn so the layout reads
// correctly and never crashes when art is absent.
inline void icon_placeholder(ImDrawList* dl, const animations::Rect& r, bool focused) {
    dl->AddRect(top_left(r), bottom_right(r), token_u32(theme::ColorToken::TextPrimary), 0.0f, 0,
                1.0f);
    if (focused) {
        focus_outline(dl, r);
    }
}

}  // namespace poker_trainer::screens::render_util
