#include "modal/outage_banner.hpp"

#include "backbone/animation_clock.hpp"

#include "assets/asset_paths.hpp"
#include "theme/theme_tokens.hpp"

#include <algorithm>
#include <cstdint>

#include <imgui.h>

#include "bridge/asset_image.hpp"

// Pure state machine (top of file) + the ImGui foreground-draw-list render
// (bottom). The pure functions take `now` explicitly and touch no ImGui, so they
// are unit-tested; the render reads the animation clock and is browser-verified.

namespace poker_trainer::modal {

namespace {

[[nodiscard]] float clamp01(float v) noexcept { return std::clamp(v, 0.0f, 1.0f); }

// Cubic ease-out (slide-in) / ease-in (slide-out).
[[nodiscard]] float ease_out(float t) noexcept {
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}
[[nodiscard]] float ease_in(float t) noexcept { return t * t * t; }

[[nodiscard]] float phase_t(const BannerState& s, std::uint64_t now, float span_ms) noexcept {
    const float elapsed = static_cast<float>(now - s.phase_start_ms);
    return clamp01(elapsed / span_ms);
}

}  // namespace

void banner_advance(BannerState& s, std::uint64_t now) noexcept {
    switch (s.phase) {
        case BannerPhase::Hidden:
            return;
        case BannerPhase::SlidingIn:
            if (now - s.phase_start_ms >= static_cast<std::uint64_t>(kBannerSlideMs)) {
                s.phase = BannerPhase::Holding;
                s.phase_start_ms = now;
            }
            return;
        case BannerPhase::Holding:
            if (now - s.phase_start_ms >= kBannerHoldMs) {
                s.phase = BannerPhase::SlidingOut;
                s.phase_start_ms = now;
                s.bar_frozen = 0.0f;  // natural expiry: the bar has drained
            }
            return;
        case BannerPhase::SlidingOut:
            if (now - s.phase_start_ms >= static_cast<std::uint64_t>(kBannerSlideMs)) {
                s.phase = BannerPhase::Hidden;
            }
            return;
    }
}

void banner_trigger(BannerState& s, std::string_view message, std::uint64_t now) {
    s.message.assign(message);
    if (s.phase == BannerPhase::Hidden) {
        s.phase = BannerPhase::SlidingIn;  // full slide-in from above
    } else {
        // Already visible (any of SlidingIn / Holding / SlidingOut): replace in
        // place, restart the 5s hold at a full bar, do NOT replay the slide-in.
        s.phase = BannerPhase::Holding;
    }
    s.phase_start_ms = now;
}

void banner_dismiss(BannerState& s, std::uint64_t now) noexcept {
    if (s.phase == BannerPhase::Hidden || s.phase == BannerPhase::SlidingOut) {
        return;
    }
    s.bar_frozen = banner_geometry(s, now).bar;  // halt the bar at its current width
    s.phase = BannerPhase::SlidingOut;
    s.phase_start_ms = now;
}

bool banner_visible(const BannerState& s) noexcept {
    return s.phase != BannerPhase::Hidden;
}

BannerGeometry banner_geometry(const BannerState& s, std::uint64_t now) noexcept {
    BannerGeometry g{};
    switch (s.phase) {
        case BannerPhase::Hidden:
            return g;
        case BannerPhase::SlidingIn:
            g.visible = true;
            g.reveal = ease_out(phase_t(s, now, kBannerSlideMs));
            g.bar = 1.0f;
            return g;
        case BannerPhase::Holding:
            g.visible = true;
            g.reveal = 1.0f;
            g.bar = 1.0f - phase_t(s, now, static_cast<float>(kBannerHoldMs));
            return g;
        case BannerPhase::SlidingOut:
            g.visible = true;
            g.reveal = 1.0f - ease_in(phase_t(s, now, kBannerSlideMs));
            g.bar = s.bar_frozen;
            return g;
    }
    return g;
}

// ===== ImGui render (foreground draw list; browser-verified, not unit-tested) =====

namespace {

[[nodiscard]] ImU32 token_u32(theme::ColorToken token) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(token));
}

// Procedural warning glyph (a triangle with an exclamation mark). ARCHITECTURE
// leaves the specific icon to the visual pass and the sealed asset_paths.hpp has no
// warning-glyph AssetId, so this is drawn, not an image slot. (Reported.)
void draw_warning_glyph(ImDrawList* dl, float cx, float cy, float size, ImU32 color) {
    const float h = size * 0.5f;
    const ImVec2 a{cx, cy - h};
    const ImVec2 b{cx - h, cy + h};
    const ImVec2 c{cx + h, cy + h};
    dl->AddTriangle(a, b, c, color, 2.0f);
    dl->AddLine(ImVec2{cx, cy - h * 0.35f}, ImVec2{cx, cy + h * 0.3f}, color, 2.0f);
    dl->AddCircleFilled(ImVec2{cx, cy + h * 0.6f}, 1.5f, color);
}

}  // namespace

void render_outage_banner(BannerState& s) {
    const std::uint64_t now = backbone::total_ms_since_app_start();
    banner_advance(s, now);
    const BannerGeometry g = banner_geometry(s, now);
    if (!g.visible) {
        return;
    }

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float canvas_w = vp->Size.x;
    const float canvas_h = vp->Size.y;

    const float bw = std::max(canvas_w / 6.0f, 180.0f);
    const float bh = std::max(canvas_h * 0.075f, 50.0f);  // ~60px, scales with canvas
    const float top_gap = canvas_h * 0.01f;
    const float x = vp->Pos.x + (canvas_w - bw) * 0.5f;
    const float y = vp->Pos.y + (g.reveal * (bh + top_gap) - bh);  // slides down from above

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddRectFilled(ImVec2{x, y}, ImVec2{x + bw, y + bh}, token_u32(theme::ColorToken::BgModalSurface),
                      4.0f);

    // Image-path/swap model: the authored warning glyph drops in over IconWarning;
    // the procedural triangle is the is_unavailable fallback.
    const float glyph_cy = y + bh * 0.45f;
    const float glyph_sz = bh * 0.4f;
    const float glyph_cx = x + bh * 0.5f;
    if (!bridge::draw_asset_image(dl, ImVec2{glyph_cx - glyph_sz, glyph_cy - glyph_sz},
                                  ImVec2{glyph_cx + glyph_sz, glyph_cy + glyph_sz},
                                  assets::AssetId::IconWarning)) {
        draw_warning_glyph(dl, glyph_cx, glyph_cy, glyph_sz, token_u32(theme::ColorToken::TextPrimary));
    }

    const ImVec2 ts = ImGui::CalcTextSize(s.message.c_str());
    const float text_x = x + bh;
    dl->AddText(ImVec2{text_x, y + (bh - ts.y) * 0.5f}, token_u32(theme::ColorToken::TextPrimary),
                s.message.c_str());

    // Countdown bar along the bottom edge (white, ~2-3px), draining right -> left.
    const float bar_h = std::max(2.0f, bh * 0.05f);
    const float bar_w = bw * clamp01(g.bar);
    dl->AddRectFilled(ImVec2{x, y + bh - bar_h}, ImVec2{x + bar_w, y + bh},
                      ImGui::ColorConvertFloat4ToU32(ImVec4{1.0f, 1.0f, 1.0f, 1.0f}));

    // Click anywhere on the banner dismisses it (only while fully/visibly in view,
    // not mid-slide-out). This is the banner's own click target; it neither
    // navigates nor activates anything beneath it.
    if (s.phase != BannerPhase::SlidingOut && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const ImVec2 m = ImGui::GetIO().MousePos;
        if (m.x >= x && m.x <= x + bw && m.y >= y && m.y <= y + bh) {
            banner_dismiss(s, now);
        }
    }
}

}  // namespace poker_trainer::modal
