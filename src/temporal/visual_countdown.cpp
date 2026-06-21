#include "temporal/visual_countdown.hpp"

#include "temporal/delta_timer.hpp"

#include "theme/theme_tokens.hpp"

#include <cstdint>
#include <format>

#include <imgui.h>

namespace poker_trainer::temporal {

namespace {

// Canvas-relative placement: top-right, just below the persistent cluster, right-
// aligned near the cluster's right edge. Z11 exposes no shared cluster-bounds
// accessor and render/layout.hpp is Z08's, so these fractions mirror Z08's Game
// cluster layout (cluster_anchor x = 0.86w, four icons of 0.024w) rather than reach
// across the zone boundary. SEAM(Z10/Z08/Z11): flagged for the visual pass.
constexpr float kCountdownRightFrac = 0.98f;  // right edge, fraction of canvas width
constexpr float kCountdownTopFrac = 0.095f;   // top edge, fraction of canvas height

}  // namespace

CountdownDisplay format_countdown(std::uint64_t elapsed_ms, std::uint64_t target_ms) {
    if (elapsed_ms < target_ms) {
        const std::uint64_t remaining = target_ms - elapsed_ms;
        const std::uint64_t n = (remaining + 999ULL) / 1000ULL;  // ceil to whole seconds
        return CountdownDisplay{std::format("{}s", n), theme::ColorToken::TextSecondary};
    }
    const std::uint64_t over = elapsed_ms - target_ms;
    const std::uint64_t n = over / 1000ULL;  // floor; "0s" at the crossover, then up
    return CountdownDisplay{std::format("{}s", n), theme::ColorToken::StateFail};
}

void render_countdown() {
    if (!countdown_should_render()) {
        return;
    }
    const CountdownDisplay disp = format_countdown(actual_time_ms(), target_time_ms());

    // Canvas-relative, no absolute pixels (matches game_screen.cpp's viewport use).
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float right = vp->Size.x * kCountdownRightFrac;
    const float top = vp->Size.y * kCountdownTopFrac;
    const ImVec2 text_size = ImGui::CalcTextSize(disp.text.c_str());
    const ImVec2 pos{right - text_size.x, top};

    // Plain text — no icon, no box, no border; drawn into the same background draw
    // list the Game chrome uses so it sits below the cluster.
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddText(pos, ImGui::ColorConvertFloat4ToU32(theme::get_color(disp.color)),
                disp.text.c_str());
}

}  // namespace poker_trainer::temporal
