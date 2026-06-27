#include "modal/leaderboard_view.hpp"

#include "modal/modal_base.hpp"
#include "modal/modals.hpp"

#include "backbone/animation_clock.hpp"
#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"

#include "assets/asset_paths.hpp"
#include "theme/theme_tokens.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <imgui.h>
#ifdef __EMSCRIPTEN__
#include <imgui_internal.h>  // ImGui::ClearActiveID — drop the search field's capture on close
#endif

#include "bridge/asset_image.hpp"
#include "bridge/focus_registry.hpp"

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

std::optional<std::int64_t> rank_jump_digit(RankJumpBuffer& buf, int digit, std::uint64_t now_ms,
                                            std::int64_t min_rank, std::int64_t max_rank) noexcept {
    // `active && ...` short-circuits, so last_ms is read only once a sequence is running.
    const bool timed_out = buf.active && now_ms - buf.last_ms > kRankJumpWindowMs;
    const bool fresh = !buf.active || timed_out;
    if (fresh) {
        if (digit == 0) {
            buf = RankJumpBuffer{};  // a leading 0 never starts a sequence — no jump
            return std::nullopt;
        }
        buf.value = digit;
    } else {
        buf.value = buf.value * 10 + digit;
        // Once the raw number passes max_rank every larger value clamps to max anyway, so
        // cap it just above max_rank — observably identical, and it can't overflow on a
        // long key sequence.
        if (buf.value > max_rank + 1) {
            buf.value = max_rank + 1;
        }
    }
    buf.active = true;
    buf.last_ms = now_ms;
    return std::clamp<std::int64_t>(buf.value, min_rank, max_rank);
}

std::int64_t rank_jump_arrow(std::int64_t current, int delta, std::int64_t min_rank,
                             std::int64_t max_rank) noexcept {
    if (current < min_rank) {
        return min_rank;  // no / invalid current highlight -> snap to the first valid rank
    }
    return std::clamp<std::int64_t>(current + delta, min_rank, max_rank);
}

std::int64_t leaderboard_highest_match(const std::vector<LeaderboardRow>& rows,
                                       std::string_view query) {
    std::int64_t best = -1;
    for (const LeaderboardRow& row : rows) {
        if (leaderboard_username_matches(row.name, query)) {
            const std::int64_t r = static_cast<std::int64_t>(row.rank);
            if (best < 0 || r < best) {
                best = r;
            }
        }
    }
    return best;
}

std::int64_t leaderboard_handoff_rank(const std::vector<LeaderboardRow>& rows,
                                      std::string_view query) {
    const std::int64_t match = leaderboard_highest_match(rows, query);
    if (match >= 0) {
        return match;  // Case 2: a real substring match (also covers a non-empty query)
    }
    // Case 1 (empty query) and Case 3 (no match): the rank-1 default = the lowest present
    // rank. An empty query matches everyone, so highest_match("") is that minimum (or -1
    // when the board itself is empty).
    return leaderboard_highest_match(rows, "");
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

// Click on a focusable: activate keyboard mode + move the focus pointer to it (mirrors
// account_modal.cpp's focus_on_click), so the ring follows the click and the reconcile
// keeps the search caret when the field is clicked into.
void focus_on_click(backbone::FocusableId id) {
    backbone::activate_keyboard_mode();
    backbone::snap_focus_to(id);
}

// A text hyperlink (accent_primary, underline on hover or when `highlighted`). Returns
// true on click. `highlighted` draws a persistent underline for the arrow-key positional
// highlight on Category A grouped stops (leaderboard guest row).
[[nodiscard]] bool text_link(const char* id, const char* label, bool highlighted = false) {
    const ImVec2 size = ImGui::CalcTextSize(label);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton(id, size);
    const bool hov = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText(p, token_u32(theme::ColorToken::AccentPrimary), label);
    if (hov || highlighted) {
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
    // Focus ring on the swap icon (the first focus stop); it navigates away on click, so no
    // click-to-snap (keyboard reaches it via Tab).
    bridge::draw_focus_ring(kLeaderboardShopIcon, token_u32(theme::ColorToken::BorderFocus));
    ImGui::SetCursorPos(saved);
    return clicked;
}

// Commit the active search and hand off to the list stop (#3). Shared by BOTH Enter (in
// render_search_bar) and forward-Tab (in leaderboard_dispatch_key) — the two are
// interchangeable. Restores the full list, sets the shared highlight cursor via
// leaderboard_handoff_rank (highest match, else the rank-1 default for an empty / no-match
// search), scrolls it into view, resets the digit buffer, and moves focus to the list. The
// highlight is -1 (nothing highlighted) only when the board is empty (loading / error),
// where focus still lands on the list.
void commit_search_to_list(ModalRuntime& rt, const LeaderboardData& data) {
    const std::int64_t landing = leaderboard_handoff_rank(data.rows, rt.leaderboard_search);
    rt.leaderboard_search.clear();  // restore the full list
    rt.leaderboard_highlight_rank = landing;
    rt.leaderboard_rank_buffer = RankJumpBuffer{};  // a fresh digit sequence continues from here
    rt.leaderboard_scroll_to_highlight = landing >= 0;
    backbone::snap_focus_to(kLeaderboardList);
}

// The 32-char-capped search input with the live filter + Enter/Tab handoff. Typing live-
// filters the list (case-insensitive substring, progressively narrowing). On Enter it
// commits via commit_search_to_list — identical to forward-Tab (handled in the dispatch).
void render_search_bar(ModalRuntime& runtime, const LeaderboardData& data,
                       const bridge::FocusReconcile& rec, ImU32 ring) {
    char buf[kLeaderboardSearchMaxChars + 1] = {};
    const std::size_t n = std::min(runtime.leaderboard_search.size(), kLeaderboardSearchMaxChars);
    std::copy_n(runtime.leaderboard_search.data(), n, buf);
    buf[n] = '\0';

    bridge::grab_keyboard_if_target(rec, kLeaderboardSearch);  // steer the caret here on Tab-in
    ImGui::SetNextItemWidth(-1.0f);  // full width minus padding
    const bool entered = ImGui::InputTextWithHint(
        "##leaderboard_search", "Search leaderboard", buf, sizeof(buf),
        ImGuiInputTextFlags_EnterReturnsTrue);
    if (ImGui::IsItemClicked()) {
        focus_on_click(kLeaderboardSearch);  // a click into the field must own its focus stop
    }
    bridge::draw_focus_ring(kLeaderboardSearch, ring);
    // The buffer cap already enforces the 32-char keystroke limit; clamp defensively.
    runtime.leaderboard_search = leaderboard_clamp_search(buf);

    if (entered) {
        commit_search_to_list(runtime, data);  // Enter == forward-Tab (Cases 1-3 of the handoff)
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

    // Deferred so the click handoff (which clears the live filter) runs AFTER the row loop,
    // never mid-loop where it would invalidate `query` / the filtered iteration.
    std::int64_t clicked_rank = -1;
    bool scrolled = false;

    for (const LeaderboardRow& row : data.rows) {
        if (!query.empty() && !leaderboard_username_matches(row.name, query)) {
            continue;
        }
        ImGui::PushID(static_cast<int>(row.rank));
        const ImVec2 p = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("##row", ImVec2{region_w, row_h})) {
            clicked_rank = static_cast<std::int64_t>(row.rank);
        }
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

        if (is_highlight && runtime.leaderboard_scroll_to_highlight) {
            ImGui::SetScrollHereY();  // bring a freshly-jumped row into view (once)
            scrolled = true;
        }
        ImGui::PopID();
    }

    if (clicked_rank >= 0) {
        // Click a row (incl. a filtered search result): restore the full list, highlight
        // this player, and hand focus to the list stop with the shared cursor here. The
        // scroll is deferred to the next frame, when the full (unfiltered) list renders.
        runtime.leaderboard_highlight_rank = clicked_rank;
        runtime.leaderboard_search.clear();
        runtime.leaderboard_rank_buffer = RankJumpBuffer{};
        runtime.leaderboard_scroll_to_highlight = true;
        focus_on_click(kLeaderboardList);
    } else if (scrolled) {
        runtime.leaderboard_scroll_to_highlight = false;  // consumed: the jump is now in view
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
        // Underline the highlighted option when this stop is focused (Category A positional
        // toggle). No highlight = both links show only on hover.
        const bool row_focused = backbone::is_keyboard_mode_active() &&
                                 backbone::get_focused_element() == kLeaderboardYourRow;
        const bool sign_in_hl =
            row_focused && runtime.guest_row_highlight == GuestRowHighlight::SignIn;
        const bool sign_up_hl =
            row_focused && runtime.guest_row_highlight == GuestRowHighlight::SignUp;
        if (text_link("##lb_signin", "Sign in", sign_in_hl) && ctrl.open_sign_in) {
            ctrl.open_sign_in();
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("/");
        ImGui::SameLine();
        if (text_link("##lb_signup", "Sign up", sign_up_hl) && ctrl.open_sign_up) {
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
        // Directly enable the opt-in (same action as the Settings -> Tomatoes checkbox).
        // The toggle persists + pushes opted_in synchronously, so dropping the loaded flag
        // re-fetches the board next frame: the row recomputes to the opted-in layout AND the
        // now-eligible user appears in the list, all in place without leaving the modal.
        if (text_link("##lb_optin", "opt in to appear on leaderboard") &&
            ctrl.enable_opt_in) {
            ctrl.enable_opt_in();
            runtime.leaderboard_loaded = false;  // re-fetch in place so the user's row appears
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::TextSecondary));
        ImGui::TextUnformatted(")");
        ImGui::PopStyleColor();
    }
}

// List-stop navigation (digit jump-by-rank + arrow ±1). Tab and other keys fall through.
bool list_stop_key(ModalRuntime& rt, const backbone::KeyEvent& e) {
    const LeaderboardData& data = rt.leaderboard_data;
    if (data.rows.empty()) {
        return false;  // nothing to jump to (loading / error / empty board)
    }
    std::int64_t min_rank = -1;
    std::int64_t max_rank = -1;
    for (const LeaderboardRow& row : data.rows) {
        const std::int64_t r = static_cast<std::int64_t>(row.rank);
        if (min_rank < 0 || r < min_rank) {
            min_rank = r;
        }
        if (r > max_rank) {
            max_rank = r;
        }
    }
    const int code = static_cast<int>(e.code);
    if (code >= static_cast<int>(backbone::KeyCode::Digit0) &&
        code <= static_cast<int>(backbone::KeyCode::Digit9)) {
        const int digit = code - static_cast<int>(backbone::KeyCode::Digit0);
        const std::optional<std::int64_t> jumped =
            rank_jump_digit(rt.leaderboard_rank_buffer, digit, backbone::total_ms_since_app_start(),
                            min_rank, max_rank);
        if (jumped.has_value()) {
            rt.leaderboard_highlight_rank = *jumped;
            rt.leaderboard_scroll_to_highlight = true;
        }
        return true;  // a digit on the list stop is consumed (even a no-op leading 0)
    }
    if (e.code == backbone::KeyCode::ArrowDown || e.code == backbone::KeyCode::ArrowUp) {
        const int delta = e.code == backbone::KeyCode::ArrowDown ? 1 : -1;  // down = next rank
        rt.leaderboard_rank_buffer = RankJumpBuffer{};  // an arrow resets the digit buffer
        rt.leaderboard_highlight_rank =
            rank_jump_arrow(rt.leaderboard_highlight_rank, delta, min_rank, max_rank);
        rt.leaderboard_scroll_to_highlight = true;
        return true;
    }
    return false;  // Tab / Shift-Tab exit the stop via the backbone focus-nav handler
}

// Your-rank stop, state-dependent. Guest = one stop, two links (Category A positional
// toggle): Left/Up highlights Sign in, Right/Down highlights Sign up; Enter/Space activates
// the highlighted option, defaulting to Sign in when no highlight is set. Digit 1/2 jump
// immediately (and set the highlight for subsequent Enter). Opted-out = the single opt-in
// link (Space/Enter). Opted-in = a visual-only stop.
bool your_row_key(ModalRuntime& rt, const backbone::KeyEvent& e) {
    const LeaderboardController& ctrl = rt.leaderboard;
    const LeaderboardSelf self = ctrl.self ? ctrl.self() : LeaderboardSelf{};
    const bool activate = e.code == backbone::KeyCode::Space || e.code == backbone::KeyCode::Enter;
    if (self.state == LeaderboardSelfState::Guest) {
        // Absolute positional arrows: Left/Up = Sign in (left option), Right/Down = Sign up.
        if (e.code == backbone::KeyCode::ArrowLeft || e.code == backbone::KeyCode::ArrowUp) {
            rt.guest_row_highlight = GuestRowHighlight::SignIn;
            return true;
        }
        if (e.code == backbone::KeyCode::ArrowRight || e.code == backbone::KeyCode::ArrowDown) {
            rt.guest_row_highlight = GuestRowHighlight::SignUp;
            return true;
        }
        // Digit jump: immediate activate and set the highlight (so Enter follows suit).
        if (e.code == backbone::KeyCode::Digit1) {
            rt.guest_row_highlight = GuestRowHighlight::SignIn;
            if (ctrl.open_sign_in) {
                ctrl.open_sign_in();
            }
            return true;
        }
        if (e.code == backbone::KeyCode::Digit2) {
            rt.guest_row_highlight = GuestRowHighlight::SignUp;
            if (ctrl.open_sign_up) {
                ctrl.open_sign_up();
            }
            return true;
        }
        // Enter/Space: activate the highlighted option; Sign in is the default when no
        // highlight has been set (plain Enter with no prior arrow or digit input).
        if (activate) {
            if (rt.guest_row_highlight == GuestRowHighlight::SignUp) {
                if (ctrl.open_sign_up) {
                    ctrl.open_sign_up();
                }
            } else {
                if (ctrl.open_sign_in) {
                    ctrl.open_sign_in();  // default for None or SignIn highlight
                }
            }
            return true;
        }
        return false;
    }
    if (self.state == LeaderboardSelfState::OptedOut) {
        if (activate) {
            if (ctrl.enable_opt_in) {
                ctrl.enable_opt_in();
                rt.leaderboard_loaded = false;  // re-fetch in place so the user's row appears
            }
            return true;
        }
        return false;
    }
    // Opted in: the user's row is highlighted but has no actionable element. Consume
    // Space/Enter as a no-op so it stays a visual stop; let Tab fall through.
    return activate;
}

}  // namespace

std::span<const backbone::FocusableId> leaderboard_focus_list() noexcept {
    static constexpr std::array<backbone::FocusableId, 5> kList{
        kLeaderboardShopIcon, kLeaderboardSearch, kLeaderboardList, kLeaderboardYourRow,
        kLeaderboardClose};
    return kList;
}

void leaderboard_on_open(ModalRuntime& runtime) {
    runtime.leaderboard_rank_buffer = RankJumpBuffer{};
    runtime.leaderboard_last_synced = backbone::kNoFocus;
    runtime.leaderboard_scroll_to_highlight = false;
    runtime.guest_row_highlight = GuestRowHighlight::None;
    if (runtime.focus_registry == nullptr) {
        return;
    }
    bridge::FocusRegistry& reg = *runtime.focus_registry;
    reg.clear();
    // Only the search is a text field; the other four stops yield ImGui's keyboard focus so
    // their digit / arrow / activation keys reach the dispatch rather than an input box.
    reg.register_element(kLeaderboardShopIcon, bridge::FocusableEntry{});
    reg.register_element(kLeaderboardSearch, bridge::FocusableEntry{.is_text_field = true});
    reg.register_element(kLeaderboardList, bridge::FocusableEntry{});
    reg.register_element(kLeaderboardYourRow, bridge::FocusableEntry{});
    reg.register_element(kLeaderboardClose, bridge::FocusableEntry{});
}

bool leaderboard_dispatch_key(ModalRuntime& runtime, const backbone::KeyEvent& e) {
    if (e.type != backbone::KeyEventType::KeyDown) {
        return false;
    }
    const backbone::FocusableId focused = backbone::get_focused_element();
    const bool activate = e.code == backbone::KeyCode::Space || e.code == backbone::KeyCode::Enter;
    if (focused == kLeaderboardSearch) {
        // Forward Tab commits the search and hands off to the list — identical to Enter (the
        // two are interchangeable; Cases 1-3 of the handoff). Shift-Tab (back to the Shop
        // icon) and typing / Enter belong to the field, so they fall through.
        if (e.code == backbone::KeyCode::Tab &&
            !backbone::has_mod(e.mods, backbone::ModMask::Shift)) {
            commit_search_to_list(runtime, runtime.leaderboard_data);
            return true;
        }
        return false;  // typing + Enter belong to the ImGui search field (render_search_bar)
    }
    if (focused == kLeaderboardShopIcon) {
        if (activate) {
            swap_to_shop();
            return true;
        }
        return false;
    }
    if (focused == kLeaderboardList) {
        return list_stop_key(runtime, e);
    }
    if (focused == kLeaderboardYourRow) {
        return your_row_key(runtime, e);
    }
    if (focused == kLeaderboardClose) {
        if (activate) {
            close_modal();
            return true;
        }
        return false;
    }
    return false;  // unfocused / unknown stop: fall through (Tab navigates)
}

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
    // Reconcile ImGui's keyboard focus to the focus-managed stop (only the search is a text
    // field; every other stop yields the caret). Mirrors the auth modal's render_auth_body.
    const ImU32 ring = token_u32(theme::ColorToken::BorderFocus);
    const bridge::FocusReconcile rec =
        runtime.focus_registry != nullptr
            ? bridge::begin_focus_reconcile(*runtime.focus_registry, runtime.leaderboard_last_synced)
            : bridge::FocusReconcile{};

    const bool x_clicked = modal_draw_x_close(kLeaderboardClose);
    const bool to_shop = draw_leaderboard_header_right(self.lifetime);

    const PillHeaderAnchor anchor = modal_draw_pill_header(assets::AssetId::IconTrophy, "Leaderboard");
    // "(refreshes daily at 00:00 PST)" renders INLINE, immediately to the right of the
    // title pill, in smaller text_secondary — drawn via the draw list at the pill's text
    // baseline so it never stacks over the title.
    ImGui::GetWindowDrawList()->AddText(ImVec2{anchor.trailing_x, anchor.text_y},
                                        token_u32(theme::ColorToken::TextSecondary),
                                        "(refreshes daily at 00:00 PST)");

    render_search_bar(runtime, data, rec, ring);

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
    // The scrollable list is ONE focus stop; ring the whole child when it holds focus.
    bridge::draw_focus_ring(kLeaderboardList, ring);

    const ImVec2 yr_top = ImGui::GetCursorScreenPos();
    render_your_rank_row(runtime, data, self);
    {
        // The persistent your-rank row is a focus stop; ring the band it occupies.
        const ImVec2 yr_bot = ImGui::GetCursorScreenPos();
        const ImVec2 wp = ImGui::GetWindowPos();
        bridge::draw_focus_ring_rect(kLeaderboardYourRow,
                                     wp.x + ImGui::GetWindowContentRegionMin().x, yr_top.y,
                                     wp.x + ImGui::GetWindowContentRegionMax().x, yr_bot.y, ring);
    }

    // Record the element ImGui was reconciled to this frame (for next frame's decision).
    runtime.leaderboard_last_synced = bridge::active_focus_or_none();

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
