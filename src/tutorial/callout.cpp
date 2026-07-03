#include "tutorial/callout.hpp"

#include "animations/button_morph.hpp"

#include "theme/theme_tokens.hpp"

#include <algorithm>
#include <optional>
#include <string_view>

#include <imgui.h>

namespace poker_trainer::tutorial {

namespace {

// Lens opacity per ARCHITECTURE §Overlay Visual Pattern ("approximately 60%").
constexpr float kLensAlpha = 0.6f;
// Padding around a spotlit element so its cutout breathes a little.
constexpr float kSpotPad = 8.0f;
constexpr float kPanelPad = 14.0f;

[[nodiscard]] ImVec2 tl(const Rect& r) noexcept { return ImVec2{r.x, r.y}; }
[[nodiscard]] ImVec2 br(const Rect& r) noexcept { return ImVec2{r.x + r.w, r.y + r.h}; }

[[nodiscard]] ImU32 token_col(theme::ColorToken token) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(token));
}

// A token color with its alpha replaced (for the translucent lens).
[[nodiscard]] ImU32 token_col_alpha(theme::ColorToken token, float alpha) {
    ImVec4 c = theme::get_color(token);
    c.w = alpha;
    return ImGui::ColorConvertFloat4ToU32(c);
}

void fill(ImDrawList* dl, const Rect& r, ImU32 col, float rounding = 0.0f) {
    if (r.w <= 0.0f || r.h <= 0.0f) {
        return;
    }
    dl->AddRectFilled(tl(r), br(r), col, rounding);
}

void outline(ImDrawList* dl, const Rect& r, ImU32 col, float thickness, float rounding = 0.0f) {
    dl->AddRect(tl(r), br(r), col, rounding, 0, thickness);
}

void centered_text(ImDrawList* dl, const Rect& r, std::string_view text, theme::ColorToken color) {
    const ImVec2 size = ImGui::CalcTextSize(text.data(), text.data() + text.size());
    const ImVec2 pos{r.x + (r.w - size.x) * 0.5f, r.y + (r.h - size.y) * 0.5f};
    dl->AddText(pos, token_col(color), text.data(), text.data() + text.size());
}

void render_button(ImDrawList* dl, const Rect& r, std::string_view label, bool focused) {
    fill(dl, r, token_col(theme::ColorToken::ButtonBg), 6.0f);
    centered_text(dl, r, label, theme::ColorToken::TextButton);
    if (focused) {
        outline(dl, r, token_col(theme::ColorToken::BorderFocus), 2.0f, 6.0f);
    }
}

}  // namespace

Rect skip_button_rect(Canvas canvas) noexcept {
    const float w = canvas.width * 0.16f;
    const float h = canvas.height * 0.05f;
    return Rect{canvas.width * 0.5f - w * 0.5f, canvas.height - canvas.height * 0.02f - h, w, h};
}

Rect callout_panel_rect(Canvas canvas, const std::optional<Rect>& spotlight) noexcept {
    const float pw = canvas.width * 0.30f;
    const float margin = canvas.width * 0.02f;
    const float cx = canvas.width * 0.5f - pw * 0.5f;
    if (!spotlight.has_value()) {
        // No spotlight: bottom-center, clear of the skip button.
        return Rect{cx, canvas.height * 0.62f, pw, canvas.height * 0.24f};
    }
    const Rect s = *spotlight;
    const float s_cy = s.y + s.h * 0.5f;
    // A panel boxed to one side narrows to fit and grows taller so the wrapped copy
    // still fits; full width when it falls to the top / bottom.
    const float min_side_w = canvas.width * 0.20f;
    const float right_avail = (canvas.width - margin) - (s.x + s.w + margin);
    const float left_avail = (s.x - margin) - margin;
    auto side_panel = [&](float x, float w) {
        const float h = (w < canvas.width * 0.26f) ? canvas.height * 0.34f : canvas.height * 0.24f;
        const float y = std::clamp(s_cy - h * 0.5f, canvas.height * 0.04f,
                                   canvas.height * 0.96f - h);
        return Rect{x, y, w, h};
    };
    // Prefer 3 o'clock (right of the spotlight) -- the Post-Round recap and the
    // left-column math inputs both clear their element this way (Group C).
    if (right_avail >= min_side_w) {
        return side_panel(s.x + s.w + margin, std::min(pw, right_avail));
    }
    if (left_avail >= min_side_w) {
        return side_panel(margin, std::min(pw, left_avail));
    }
    // The spotlight spans the width: go above or below it (full width), clear of the
    // bottom-center skip button.
    const float py = (s_cy < canvas.height * 0.5f) ? canvas.height * 0.66f : canvas.height * 0.08f;
    return Rect{cx, py, pw, canvas.height * 0.24f};
}

Rect next_button_rect(const Rect& panel) noexcept {
    const float w = panel.w * 0.30f;
    const float h = panel.h * 0.22f;
    return Rect{panel.x + panel.w - w - kPanelPad, panel.y + panel.h - h - kPanelPad, w, h};
}

Rect prev_button_rect(const Rect& panel) noexcept {
    const float w = panel.w * 0.30f;
    const float h = panel.h * 0.22f;
    return Rect{panel.x + kPanelPad, panel.y + panel.h - h - kPanelPad, w, h};
}

Rect prompt_panel_rect(Canvas canvas) noexcept {
    const float pw = canvas.width * 0.40f;
    const float ph = canvas.height * 0.26f;
    return Rect{canvas.width * 0.5f - pw * 0.5f, canvas.height * 0.5f - ph * 0.5f, pw, ph};
}

Rect prompt_start_rect(const Rect& panel) noexcept {
    const float w = panel.w * 0.40f;
    const float h = panel.h * 0.24f;
    return Rect{panel.x + panel.w * 0.06f, panel.y + panel.h - h - kPanelPad, w, h};
}

Rect prompt_skip_rect(const Rect& panel) noexcept {
    const float w = panel.w * 0.40f;
    const float h = panel.h * 0.24f;
    return Rect{panel.x + panel.w - w - panel.w * 0.06f, panel.y + panel.h - h - kPanelPad, w, h};
}

void render_lens(ImDrawList* dl, Canvas canvas, const std::optional<Rect>& spotlight) {
    const ImU32 scrim = token_col_alpha(theme::ColorToken::BgModalScrim, kLensAlpha);
    if (!spotlight.has_value()) {
        fill(dl, Rect{0.0f, 0.0f, canvas.width, canvas.height}, scrim);
        return;
    }
    // Padded cutout, clamped to the canvas.
    const float x0 = std::max(0.0f, spotlight->x - kSpotPad);
    const float y0 = std::max(0.0f, spotlight->y - kSpotPad);
    const float x1 = std::min(canvas.width, spotlight->x + spotlight->w + kSpotPad);
    const float y1 = std::min(canvas.height, spotlight->y + spotlight->h + kSpotPad);
    // Four scrim bands around the cutout (the element shows through the hole).
    fill(dl, Rect{0.0f, 0.0f, canvas.width, y0}, scrim);                       // top
    fill(dl, Rect{0.0f, y1, canvas.width, canvas.height - y1}, scrim);         // bottom
    fill(dl, Rect{0.0f, y0, x0, y1 - y0}, scrim);                             // left
    fill(dl, Rect{x1, y0, canvas.width - x1, y1 - y0}, scrim);               // right
    // Accent ring around the spotlit element.
    dl->AddRect(ImVec2{x0, y0}, ImVec2{x1, y1}, token_col(theme::ColorToken::AccentPrimary), 6.0f,
                0, 2.0f);
}

void render_callout(ImDrawList* dl, const Rect& panel, std::string_view text, bool show_next,
                    bool next_focused, bool show_prev, bool prev_focused) {
    fill(dl, panel, token_col(theme::ColorToken::BgModalSurface), 8.0f);
    outline(dl, panel, token_col(theme::ColorToken::BorderDefault), 1.5f, 8.0f);
    const float wrap = panel.w - kPanelPad * 2.0f;
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2{panel.x + kPanelPad, panel.y + kPanelPad},
                token_col(theme::ColorToken::TextPrimary), text.data(), text.data() + text.size(),
                wrap);
    if (show_prev) {
        render_button(dl, prev_button_rect(panel), "Previous", prev_focused);
    }
    if (show_next) {
        render_button(dl, next_button_rect(panel), "Next", next_focused);
    }
}

void render_skip_button(ImDrawList* dl, Canvas canvas, bool focused) {
    render_button(dl, skip_button_rect(canvas), "Skip Tutorial", focused);
}

void render_prompt(ImDrawList* dl, Canvas canvas, std::string_view message, bool start_focused,
                   bool skip_focused) {
    // Dim the whole screen behind the prompt, then the centered panel.
    fill(dl, Rect{0.0f, 0.0f, canvas.width, canvas.height},
         token_col_alpha(theme::ColorToken::BgModalScrim, kLensAlpha));
    const Rect panel = prompt_panel_rect(canvas);
    fill(dl, panel, token_col(theme::ColorToken::BgModalSurface), 8.0f);
    outline(dl, panel, token_col(theme::ColorToken::BorderDefault), 1.5f, 8.0f);
    const float wrap = panel.w - kPanelPad * 2.0f;
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2{panel.x + kPanelPad, panel.y + kPanelPad},
                token_col(theme::ColorToken::TextPrimary), message.data(),
                message.data() + message.size(), wrap);
    render_button(dl, prompt_start_rect(panel), "Start Tutorial", start_focused);
    render_button(dl, prompt_skip_rect(panel), "Skip", skip_focused);
}

}  // namespace poker_trainer::tutorial
