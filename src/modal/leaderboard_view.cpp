#include "modal/leaderboard_view.hpp"

#include "modal/modal_base.hpp"
#include "modal/modals.hpp"

#include "assets/asset_paths.hpp"
#include "theme/theme_tokens.hpp"

#include <cstddef>

#include <imgui.h>

// Zone 11 — Leaderboard view. The PURE search filter (case-insensitive plain
// substring, 32-char cap) is unit-tested. The render is a CONSUMER-LIGHT thin shell:
// the data is server-side (absent) and the Shop track-purchase content is Module 7
// (unbuilt), so it shows the modal frame, header, search input, and the
// loading/error/retry scaffolding over empty data. It is not yet reachable from the
// UI (the Shop shell has no Leaderboard-swap control this wave); open_modal(
// kLeaderboardModalId) wires it when the Shop content lands.

namespace poker_trainer::modal {

namespace {

[[nodiscard]] char ascii_lower(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

}  // namespace

bool leaderboard_username_matches(std::string_view username, std::string_view query) noexcept {
    if (query.empty()) {
        return true;  // empty query matches everything (full top-100 list)
    }
    if (query.size() > username.size()) {
        return false;
    }
    for (std::size_t i = 0; i + query.size() <= username.size(); ++i) {
        bool matched = true;
        for (std::size_t j = 0; j < query.size(); ++j) {
            if (ascii_lower(username[i + j]) != ascii_lower(query[j])) {
                matched = false;
                break;
            }
        }
        if (matched) {
            return true;
        }
    }
    return false;
}

std::string leaderboard_clamp_search(std::string_view candidate) {
    // Byte cap at the 32-char limit. SEAM: Unicode-aware grapheme counting (the spec
    // accepts unicode input) is deferred; V1 caps bytes, which is exact for ASCII.
    if (candidate.size() <= kLeaderboardSearchMaxChars) {
        return std::string{candidate};
    }
    return std::string{candidate.substr(0, kLeaderboardSearchMaxChars)};
}

void render_leaderboard_view(ModalRuntime& runtime) {
    static_cast<void>(runtime);
    const bool visible = modal_begin_centered("##leaderboard_modal", kClusterModalWidthFrac,
                                              kClusterModalHeightFrac);
    bool dismiss = false;
    if (visible) {
        const bool x_clicked = modal_draw_x_close(kShopShellClose);
        // Icon-pill header (the authored trophy art drops in over IconTrophy via the
        // image-path/swap model; modal_draw_pill_header falls back to a box outline
        // when unavailable).
        modal_draw_pill_header(assets::AssetId::IconTrophy, "Leaderboard");
        ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::TextSecondary));
        ImGui::TextWrapped("Leaderboard data is served from the leaderboard backend, which is out "
                           "of scope for this build. The search filter, list scaffold, and "
                           "loading/error/retry states are present over empty data (Module 7 seam).");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::TextUnformatted("Unable to load leaderboard. Try again later.");
        ImGui::Button("Retry");  // inert seam: re-fetch lands with the leaderboard backend
        if (x_clicked || modal_click_outside_dismissed()) {
            dismiss = true;
        }
    }
    modal_end();
    if (dismiss) {
        close_modal();
    }
}

}  // namespace poker_trainer::modal
