#include "modal/leaderboard_view.hpp"

#include "modal/modal_base.hpp"
#include "modal/modals.hpp"

#include "assets/asset_paths.hpp"
#include "theme/theme_tokens.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

#include <imgui.h>
#ifdef __EMSCRIPTEN__
#include <imgui_internal.h>  // ImGui::ClearActiveID — drop the search field's capture on close
#endif

#include "bridge/asset_image.hpp"

// Zone 11 — Leaderboard view. The PURE search filter (case-insensitive plain substring,
// 32-char cap) is unit-tested. The render is a marked seam (browser-verified): the ranked
// top-100 list, the live search with Enter-jump + highlight, the persistent your-rank
// bottom row, and the loading / error / Retry states, all over a boot-wired
// LeaderboardController (fetch + your-rank + the guest / opt-out links).

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

namespace {

[[nodiscard]] ImU32 token_u32(theme::ColorToken token) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(token));
}

[[nodiscard]] ImU32 token_u32_alpha(theme::ColorToken token, float alpha) {
    ImVec4 c = theme::get_color(token);
    c.w *= alpha;
    return ImGui::ColorConvertFloat4ToU32(c);
}

// A text hyperlink (accent_primary, underline on hover). Returns true on click. Advances
// the cursor by the text width so links lay out inline via SameLine.
[[nodiscard]] bool text_link(const char* id, const char* label) {
    const ImVec2 size = ImGui::CalcTextSize(label);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton(id, size);
    const bool hov = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText(p, token_u32(theme::ColorToken::AccentPrimary), label);
    if (hov) {
        dl->AddLine(ImVec2{p.x, p.y + size.y}, ImVec2{p.x + size.x, p.y + size.y},
                    token_u32(theme::ColorToken::AccentPrimary), 1.0f);
    }
    return clicked;
}

// Draw the lifetime tomato count (icon + number) at the given screen position, returning
// the total width drawn. Used both in the header and per row (right-aligned).
void draw_tomato_count(ImDrawList* dl, ImVec2 at, std::uint64_t value, theme::ColorToken text) {
    const float line = ImGui::GetTextLineHeight();
    const ImVec2 imin{at.x, at.y};
    const ImVec2 imax{at.x + line, at.y + line};
    if (!bridge::draw_asset_image(dl, imin, imax, assets::AssetId::IconTomato)) {
        dl->AddCircleFilled(ImVec2{(imin.x + imax.x) * 0.5f, (imin.y + imax.y) * 0.5f},
                            line * 0.4f, token_u32(theme::ColorToken::StateFail));
    }
    const std::string num = std::to_string(value);
    dl->AddText(ImVec2{imax.x + line * 0.25f, at.y}, token_u32(text), num.c_str());
}

[[nodiscard]] float tomato_count_width(std::uint64_t value) {
    const float line = ImGui::GetTextLineHeight();
    return line + line * 0.25f + ImGui::CalcTextSize(std::to_string(value).c_str()).x;
}

// Header right cluster (left of the X): the Shop-swap icon, then the Lifetime Tomatoes
// count. Returns true when the Shop icon is clicked. Saves/restores the cursor.
[[nodiscard]] bool draw_leaderboard_header_right(std::uint64_t lifetime) {
    const ImVec2 saved = ImGui::GetCursorPos();
    const float line = ImGui::GetTextLineHeight();
    const float btn = line * 1.6f;
    const float pad_y = ImGui::GetStyle().WindowPadding.y;
    const float region_w = ImGui::GetWindowContentRegionMax().x;
    const float gap = line * 0.6f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 win = ImGui::GetWindowPos();

    const float count_w = tomato_count_width(lifetime);
    const float count_x = region_w - btn - gap - count_w;
    draw_tomato_count(dl, ImVec2{win.x + count_x, win.y + pad_y}, lifetime,
                      theme::ColorToken::TextPrimary);

    const float shop_x = count_x - gap - btn;
    ImGui::SetCursorPos(ImVec2{shop_x, pad_y});
    const bool clicked = ImGui::InvisibleButton("##leaderboard_to_shop", ImVec2{btn, btn});
    const ImVec2 bmin = ImGui::GetItemRectMin();
    const ImVec2 bmax = ImGui::GetItemRectMax();
    if (ImGui::IsItemHovered()) {
        dl->AddRectFilled(bmin, bmax, token_u32(theme::ColorToken::ButtonBgHover), 4.0f);
    }
    if (!bridge::draw_asset_image(dl, bmin, bmax, assets::AssetId::IconShop)) {
        dl->AddRect(bmin, bmax, token_u32(theme::ColorToken::TextPrimary), 0.0f, 0, 1.0f);
    }
    ImGui::SetCursorPos(saved);
    return clicked;
}

// The 32-char-capped search input with the live filter + Enter-jump behavior. Mutates
// runtime.leaderboard_search and, on Enter, sets the highlight rank + clears the filter.
void render_search_bar(ModalRuntime& runtime, const LeaderboardData& data) {
    char buf[kLeaderboardSearchMaxChars + 1] = {};
    const std::size_t n = std::min(runtime.leaderboard_search.size(), kLeaderboardSearchMaxChars);
    std::copy_n(runtime.leaderboard_search.data(), n, buf);
    buf[n] = '\0';

    ImGui::SetNextItemWidth(-1.0f);  // full width minus padding
    const bool entered = ImGui::InputTextWithHint(
        "##leaderboard_search", "Search leaderboard", buf, sizeof(buf),
        ImGuiInputTextFlags_EnterReturnsTrue);
    // The buffer cap already enforces the 32-char keystroke limit; clamp defensively.
    runtime.leaderboard_search = leaderboard_clamp_search(buf);

    if (entered) {
        // Enter: clear the filter and jump to the highest-ranked match (lowest rank).
        std::int64_t best = -1;
        for (const LeaderboardRow& row : data.rows) {
            if (leaderboard_username_matches(row.name, runtime.leaderboard_search)) {
                if (best < 0 || static_cast<std::int64_t>(row.rank) < best) {
                    best = static_cast<std::int64_t>(row.rank);
                }
            }
        }
        runtime.leaderboard_highlight_rank = best;
        runtime.leaderboard_search.clear();
    }
}

// Emit the ranked rows into the CURRENT window (the caller's contained scroll child).
// Applies the live filter, the hover tint, the self-row tint, and the Enter-jump
// highlight + scroll (which scrolls the enclosing child).
void render_list_rows(ModalRuntime& runtime, const LeaderboardData& data,
                      const LeaderboardSelf& self) {
    const float line = ImGui::GetTextLineHeight();
    const float row_h = line + ImGui::GetStyle().FramePadding.y * 2.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float region_w = ImGui::GetWindowContentRegionMax().x;
    const std::string_view query = runtime.leaderboard_search;
    const bool self_named = self.state != LeaderboardSelfState::Guest && !self.name.empty();

    for (const LeaderboardRow& row : data.rows) {
        if (!query.empty() && !leaderboard_username_matches(row.name, query)) {
            continue;
        }
        ImGui::PushID(static_cast<int>(row.rank));
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const bool clicked_hit = ImGui::InvisibleButton("##row", ImVec2{region_w, row_h});
        static_cast<void>(clicked_hit);
        const bool hov = ImGui::IsItemHovered();
        const ImVec2 rmin = ImGui::GetItemRectMin();
        const ImVec2 rmax = ImGui::GetItemRectMax();

        const bool is_self = self_named && row.name == self.name;
        const bool is_highlight =
            runtime.leaderboard_highlight_rank == static_cast<std::int64_t>(row.rank);
        if (is_highlight) {
            dl->AddRectFilled(rmin, rmax, token_u32(theme::ColorToken::AccentSecondary));
        } else if (is_self) {
            dl->AddRectFilled(rmin, rmax, token_u32_alpha(theme::ColorToken::AccentSecondary, 0.3f));
        } else if (hov) {
            dl->AddRectFilled(rmin, rmax, token_u32(theme::ColorToken::ButtonBgHover));
        }

        const float ty = p.y + ImGui::GetStyle().FramePadding.y;
        const ImU32 text = token_u32(theme::ColorToken::TextPrimary);
        const std::string rank_s = std::to_string(row.rank);
        dl->AddText(ImVec2{p.x + line * 0.3f, ty}, text, rank_s.c_str());
        dl->AddText(ImVec2{p.x + line * 3.5f, ty}, text, row.name.c_str());
        const float cw = tomato_count_width(row.lifetime);
        draw_tomato_count(dl, ImVec2{p.x + region_w - cw - line * 0.3f, ty}, row.lifetime,
                          theme::ColorToken::TextPrimary);

        if (is_highlight) {
            ImGui::SetScrollHereY();  // bring the Enter-jump match into view
        }
        ImGui::PopID();
    }
}

// The error + Retry state (fetch failed). Retry re-arms a fetch on the next frame.
void render_error_retry(ModalRuntime& runtime) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::TextSecondary));
    ImGui::TextUnformatted("Unable to load leaderboard. Try again later.");
    ImGui::PopStyleColor();
    if (ImGui::Button("Retry")) {
        runtime.leaderboard_loaded = false;  // re-fetch on the next frame
    }
}

// Vertical space the pinned your-rank bottom row needs, so the list child can reserve it
// and the outer modal window never overflows (an overflow would scroll the pinned header
// out of view). The Separator owns one ItemSpacing above + below; the row itself is two
// lines for the guest prompt (text + Sign in / Sign up links) and the opted-out variant
// (which can wrap), one line when opted in.
[[nodiscard]] float your_rank_reserve(const LeaderboardSelf& self) {
    const float spacing = ImGui::GetStyle().ItemSpacing.y * 2.0f;  // separator gaps
    const float lines = self.state == LeaderboardSelfState::OptedIn ? 1.0f : 2.0f;
    return spacing + lines * ImGui::GetFrameHeightWithSpacing();
}

// The persistent your-rank bottom row (always visible, separated by a grey gap). Returns
// a requested navigation action via the controller callbacks.
void render_your_rank_row(ModalRuntime& runtime, const LeaderboardData& data,
                          const LeaderboardSelf& self) {
    ImGui::Separator();
    const LeaderboardController& ctrl = runtime.leaderboard;

    if (self.state == LeaderboardSelfState::Guest) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::TextSecondary));
        ImGui::TextUnformatted("Sign in or sign up to see your rank.");
        ImGui::PopStyleColor();
        if (text_link("##lb_signin", "Sign in") && ctrl.open_sign_in) {
            ctrl.open_sign_in();
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("/");
        ImGui::SameLine();
        if (text_link("##lb_signup", "Sign up") && ctrl.open_sign_up) {
            ctrl.open_sign_up();
        }
        return;
    }

    // Logged in: resolve the user's rank from the fetched top-100 when present.
    std::int64_t rank = -1;
    for (const LeaderboardRow& row : data.rows) {
        if (row.name == self.name) {
            rank = static_cast<std::int64_t>(row.rank);
            break;
        }
    }
    const std::string rank_s = rank >= 0 ? std::to_string(rank) : "—";
    ImGui::TextUnformatted(rank_s.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(self.name.empty() ? "(you)" : self.name.c_str());
    ImGui::SameLine();
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        draw_tomato_count(dl, p, self.lifetime, theme::ColorToken::TextPrimary);
        ImGui::Dummy(ImVec2{tomato_count_width(self.lifetime), ImGui::GetTextLineHeight()});
    }
    if (self.state == LeaderboardSelfState::OptedOut) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::TextSecondary));
        ImGui::TextUnformatted("(opted out —");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (text_link("##lb_optin", "opt in to appear on leaderboard") &&
            ctrl.open_settings_tomatoes) {
            ctrl.open_settings_tomatoes();
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::TextSecondary));
        ImGui::TextUnformatted(")");
        ImGui::PopStyleColor();
    }
}

}  // namespace

void render_leaderboard_view(ModalRuntime& runtime) {
    if (!modal_begin_centered("##leaderboard_modal", kClusterModalWidthFrac,
                              kClusterModalHeightFrac)) {
        modal_end();
        return;
    }

    // Fetch once per open (blocking sync XHR); cached for the lifetime of the open view.
    if (!runtime.leaderboard_loaded) {
        runtime.leaderboard_data =
            runtime.leaderboard.fetch ? runtime.leaderboard.fetch() : LeaderboardData{};
        runtime.leaderboard_loaded = true;
    }
    const LeaderboardData& data = runtime.leaderboard_data;
    const LeaderboardSelf self =
        runtime.leaderboard.self ? runtime.leaderboard.self() : LeaderboardSelf{};

    // ===== Pinned header (drawn directly in the modal window, above the scroll child),
    // so the X, the Shop-swap icon, the Lifetime Tomatoes count, the title, and the
    // search bar stay fixed regardless of list scroll. Mirrors the Settings frame, where
    // only the body child scrolls. The header-right cluster is positioned from the outer
    // window's content width, which is stable because the list scrolls inside its own
    // child (no outer scrollbar appears to reflow the toolbar across Shop<->Leaderboard).
    const bool x_clicked = modal_draw_x_close(kShopShellClose);
    const bool to_shop = draw_leaderboard_header_right(self.lifetime);

    const PillHeaderAnchor anchor = modal_draw_pill_header(assets::AssetId::IconTrophy, "Leaderboard");
    // "(refreshes daily at 00:00 PST)" renders INLINE, immediately to the right of the
    // title pill, in smaller text_secondary — drawn via the draw list at the pill's text
    // baseline so it never stacks over the title.
    ImGui::GetWindowDrawList()->AddText(ImVec2{anchor.trailing_x, anchor.text_y},
                                        token_u32(theme::ColorToken::TextSecondary),
                                        "(refreshes daily at 00:00 PST)");

    render_search_bar(runtime, data);

    // Reserve the pinned your-rank row; only the ranked list scrolls, in a contained child
    // above it. The error/retry state renders in the same child so the header stays pinned.
    const float avail = ImGui::GetContentRegionAvail().y - your_rank_reserve(self);
    ImGui::BeginChild("##leaderboard_list", ImVec2{0.0f, avail > 0.0f ? avail : 0.0f}, false);
    if (!data.ok) {
        render_error_retry(runtime);
    } else {
        render_list_rows(runtime, data, self);
    }
    ImGui::EndChild();

    render_your_rank_row(runtime, data, self);

    const bool dismiss = x_clicked || modal_click_outside_dismissed();
    modal_end();

    if (to_shop || dismiss) {
#ifdef __EMSCRIPTEN__
        ImGui::ClearActiveID();  // release the search field's keyboard capture on leave
#endif
    }
    if (to_shop) {
        swap_to_shop();
    } else if (dismiss) {
        close_modal();
    }
}

}  // namespace poker_trainer::modal
