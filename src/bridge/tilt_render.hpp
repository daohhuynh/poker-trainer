#pragma once

// Zone 05 — the ImGui side of the hover/focus tilt (paired with bridge::hover_tilt_angle
// in ambient.*). The angle math is pure and lives in the ImGui-free bridge library; the
// actual rotation manipulates the draw list's vertex buffer, so it lives here as a
// header-only helper included ONLY by ImGui-having render translation units (screens /
// modal), never by an ImGui-free bridge .cpp. Same split as bridge/asset_image.hpp.

#include <cmath>

#include <imgui.h>

namespace poker_trainer::bridge {

// RAII tilt: on construction records the draw list's current vertex count; on
// destruction rotates every vertex appended in between about `center` by `angle`
// radians. This rotates the whole drawn button — filled rect, label glyphs, focus ring,
// icon image — together, the standard ImGui "ImRotate" idiom.
//
// VISUAL-ONLY: it transforms already-emitted geometry, so the caller's hit-test / focus
// rect is completely unaffected — the clickable/focusable area never moves, only the
// pixels tilt. A no-op when `angle` is 0 (Reduce Motion / toggle off, or at rest) or
// when `dl` is null. Intended for buttons drawn on an unclipped draw list (e.g. the
// background draw list); do not use inside a tightly clipped ImGui window, where the
// rotated corners would be clipped.
class TiltScope {
public:
    TiltScope(ImDrawList* dl, ImVec2 center, float angle) noexcept
        : dl_(dl),
          center_(center),
          angle_(angle),
          vtx_start_(dl != nullptr ? dl->VtxBuffer.Size : 0) {}

    ~TiltScope() {
        if (dl_ == nullptr || angle_ == 0.0f) {
            return;
        }
        const float s = std::sin(angle_);
        const float c = std::cos(angle_);
        for (int i = vtx_start_; i < dl_->VtxBuffer.Size; ++i) {
            ImVec2& p = dl_->VtxBuffer[i].pos;
            const float dx = p.x - center_.x;
            const float dy = p.y - center_.y;
            p.x = center_.x + dx * c - dy * s;
            p.y = center_.y + dx * s + dy * c;
        }
    }

    TiltScope(const TiltScope&) = delete;
    TiltScope& operator=(const TiltScope&) = delete;
    TiltScope(TiltScope&&) = delete;
    TiltScope& operator=(TiltScope&&) = delete;

private:
    ImDrawList* dl_;
    ImVec2 center_;
    float angle_;
    int vtx_start_;
};

}  // namespace poker_trainer::bridge
