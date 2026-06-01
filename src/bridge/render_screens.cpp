#include "bridge/canvas_sizing.hpp"
#include "bridge/loading_screen.hpp"

#include "bridge/bridge_runtime.hpp"

#include "assets/tier_config.hpp"
#include "theme/theme_tokens.hpp"

#include "imgui.h"

#include <algorithm>

// ImGui render bodies for Z05's own screens (loading + small-window / mobile
// fallback). Binding-adjacent code held to -Wall -Wextra -Werror (bridge_platform
// lib), compiled only into the wasm app. The pure progress / sizing logic these
// call lives in the native-testable bridge library.
//
// Visual specifics deferred to the visual implementation pass: the dealer button
// is drawn as a token-colored disc (its PNG + dashed-ring artwork are Tier-1
// assets the visual pass composites), and the blurred-Root background image is
// likewise deferred — what is locked here is the progress-arc behavior (clockwise
// from the top, accent_primary, proportional to Tier-1 progress) and the
// fallback messaging / colors.

namespace poker_trainer::bridge {

namespace {

constexpr float kPi = 3.14159265358979323846f;

[[nodiscard]] ImU32 token_u32(theme::ColorToken token) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(token));
}

}  // namespace

void render_loading_screen() {
    const BridgeRuntime& rt = runtime();
    const ImGuiIO& io = ImGui::GetIO();
    const float w = io.DisplaySize.x;
    const float h = io.DisplaySize.y;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(w, h),
                      token_u32(theme::ColorToken::BgPrimary));

    // Dealer button centered, ~150-200px diameter scaled to the canvas.
    const ImVec2 center(w * 0.5f, h * 0.5f);
    const float diameter = std::clamp(std::min(w, h) * 0.22f, 150.0f, 200.0f);
    const float radius = diameter * 0.5f;
    dl->AddCircleFilled(center, radius,
                        token_u32(theme::ColorToken::DealerButtonBlue), 64);
    dl->AddCircleFilled(center, radius * 0.30f,
                        token_u32(theme::ColorToken::DealerButtonGreen), 32);

    // Progress arc along the inset dashed ring: clockwise from the top, filling
    // proportionally to Tier-1 download progress. accent_primary per the theme
    // token rules (Module 3's "white arc" prose is superseded by the no-hardcoded
    // -colors invariant).
    float fraction = 0.0f;
    if (rt.tier_loader != nullptr) {
        fraction = loading_arc_fraction(
            rt.tier_loader->resolved_count(assets::AssetTier::Tier1),
            rt.tier_loader->total_count(assets::AssetTier::Tier1));
    }
    if (fraction > 0.0f) {
        const float arc_radius = radius * 0.80f;
        const float a_start = -kPi * 0.5f;                  // top
        const float a_end = a_start + fraction * 2.0f * kPi;  // clockwise (y-down)
        dl->PathArcTo(center, arc_radius, a_start, a_end, 96);
        dl->PathStroke(token_u32(theme::ColorToken::AccentPrimary),
                       ImDrawFlags_None, std::max(2.0f, radius * 0.05f));
    }
}

void render_fallback(DisplayMode mode) {
    if (mode == DisplayMode::Normal) {
        return;
    }
    const ImGuiIO& io = ImGui::GetIO();
    const float w = io.DisplaySize.x;
    const float h = io.DisplaySize.y;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(w, h),
                      token_u32(theme::ColorToken::BgPrimary));

    const char* message =
        (mode == DisplayMode::Mobile)
            ? "This trainer is designed for desktop. Please visit on a laptop "
              "or desktop browser."
            : "Please use a larger window.";
    const ImVec2 text_size = ImGui::CalcTextSize(message);
    const ImVec2 pos((w - text_size.x) * 0.5f, (h - text_size.y) * 0.5f);
    dl->AddText(pos, token_u32(theme::ColorToken::TextPrimary), message);
}

}  // namespace poker_trainer::bridge
