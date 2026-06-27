#include "modal/shop_view.hpp"

#include "modal/modal_base.hpp"
#include "modal/modals.hpp"

#include "audio/audio_paths.hpp"

#include "assets/asset_paths.hpp"
#include "theme/theme_tokens.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

#include <imgui.h>

#include "bridge/asset_image.hpp"

// Zone 11 — Module 7 Shop content. The render is a marked seam (browser-verified); the
// pure shop_button_kind helper is unit-tested. Wallet reads come from a boot-computed
// ShopSnapshot; purchases / add / remove route through the wired ShopController, so this
// TU never includes Zone 04 (persistence) or Zone 03 (audio control) headers — only the
// Phase-0 audio track catalog (audio_paths.hpp) for names / prices / genre grouping.

namespace poker_trainer::modal {

namespace {

[[nodiscard]] ImU32 token_u32(theme::ColorToken token) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(token));
}

[[nodiscard]] const char* button_label(ShopButtonKind kind) noexcept {
    switch (kind) {
        case ShopButtonKind::Confirm: return "CONFIRM";
        case ShopButtonKind::Add: return "ADD";
        case ShopButtonKind::Remove: return "REMOVE";
        case ShopButtonKind::Buy:
        case ShopButtonKind::BuyDisabled: break;
    }
    return "BUY";
}

// Draw the header's right cluster (left of the X): the Leaderboard-swap icon, then the
// tomato icon + Spendable count. Returns true when the Leaderboard icon is clicked.
// Saves/restores the cursor so the pill header still starts at the top-left.
[[nodiscard]] bool draw_shop_header_right(std::uint64_t spendable) {
    const ImVec2 saved = ImGui::GetCursorPos();
    const float line = ImGui::GetTextLineHeight();
    const float btn = line * 1.6f;
    const float pad_y = ImGui::GetStyle().WindowPadding.y;
    const float region_w = ImGui::GetWindowContentRegionMax().x;
    const float gap = line * 0.6f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 win = ImGui::GetWindowPos();

    // Spendable count (tomato icon + number), right-aligned just left of the X button.
    const std::string count = std::to_string(spendable);
    const ImVec2 num_sz = ImGui::CalcTextSize(count.c_str());
    const float icon_sz = line;
    const float spend_w = icon_sz + gap * 0.4f + num_sz.x;
    const float spend_x = region_w - btn - gap - spend_w;
    const ImVec2 icon_min{win.x + spend_x, win.y + pad_y};
    const ImVec2 icon_max{icon_min.x + icon_sz, icon_min.y + icon_sz};
    if (!bridge::draw_asset_image(dl, icon_min, icon_max, assets::AssetId::IconTomato)) {
        dl->AddCircleFilled(ImVec2{(icon_min.x + icon_max.x) * 0.5f, (icon_min.y + icon_max.y) * 0.5f},
                            icon_sz * 0.4f, token_u32(theme::ColorToken::StateFail));
    }
    dl->AddText(ImVec2{icon_max.x + gap * 0.4f, win.y + pad_y + (icon_sz - num_sz.y) * 0.5f},
                token_u32(theme::ColorToken::TextPrimary), count.c_str());

    // Leaderboard-swap icon button, left of the spendable display.
    const float lb_x = spend_x - gap - btn;
    ImGui::SetCursorPos(ImVec2{lb_x, pad_y});
    const bool clicked = ImGui::InvisibleButton("##shop_to_leaderboard", ImVec2{btn, btn});
    const ImVec2 bmin = ImGui::GetItemRectMin();
    const ImVec2 bmax = ImGui::GetItemRectMax();
    if (ImGui::IsItemHovered()) {
        dl->AddRectFilled(bmin, bmax, token_u32(theme::ColorToken::ButtonBgHover), 4.0f);
    }
    if (!bridge::draw_asset_image(dl, bmin, bmax, assets::AssetId::IconTrophy)) {
        dl->AddRect(bmin, bmax, token_u32(theme::ColorToken::TextPrimary), 0.0f, 0, 1.0f);
    }

    ImGui::SetCursorPos(saved);
    return clicked;
}

// Draw a tomato icon + price number at the current cursor (the locked-row price element,
// outside the button). `red` tints the number state_fail (the insufficient-funds state).
void draw_price(std::uint32_t price, bool red) {
    const float line = ImGui::GetTextLineHeight();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float y = p.y + ImGui::GetStyle().FramePadding.y;
    const ImVec2 imin{p.x, y};
    const ImVec2 imax{p.x + line, y + line};
    if (!bridge::draw_asset_image(dl, imin, imax, assets::AssetId::IconTomato)) {
        dl->AddCircleFilled(ImVec2{(imin.x + imax.x) * 0.5f, (imin.y + imax.y) * 0.5f},
                            line * 0.4f, token_u32(theme::ColorToken::StateFail));
    }
    const std::string num = std::to_string(price);
    dl->AddText(ImVec2{imax.x + line * 0.25f, y},
                token_u32(red ? theme::ColorToken::StateFail : theme::ColorToken::TextSecondary),
                num.c_str());
    const ImVec2 num_sz = ImGui::CalcTextSize(num.c_str());
    ImGui::Dummy(ImVec2{line + line * 0.25f + num_sz.x, line});
}

// Render one track row: the in-rotation dot, the track name, the price (locked rows), and
// the three-state action button. Applies the row's action to the controller on click.
void render_shop_row(ModalRuntime& rt, const ShopRowView& row) {
    ImGui::PushID(static_cast<int>(static_cast<std::uint8_t>(row.track)));
    const float line = ImGui::GetTextLineHeight();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::AlignTextToFramePadding();
    const ImVec2 row_start = ImGui::GetCursorScreenPos();
    // In-rotation dot (accent_primary), present only when the track is in its pool.
    if (row.in_pool) {
        dl->AddCircleFilled(ImVec2{row_start.x + line * 0.35f, row_start.y + line * 0.5f},
                            line * 0.22f, token_u32(theme::ColorToken::AccentPrimary));
    }
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + line);  // reserve the dot column

    const std::string_view name = audio::music_track_info(row.track).display_name;
    ImGui::TextUnformatted(name.data(), name.data() + name.size());

    const ShopButtonKind kind = shop_button_kind(row, rt.shop_armed_buy == row.track);
    const bool locked = (kind == ShopButtonKind::Buy || kind == ShopButtonKind::Confirm ||
                         kind == ShopButtonKind::BuyDisabled);

    // Uniform button width so the column lines up across genres.
    const float btn_w = ImGui::CalcTextSize("REMOVE").x + ImGui::GetStyle().FramePadding.x * 4.0f;
    const float btn_h = ImGui::GetFrameHeight();
    const float region_w = ImGui::GetWindowContentRegionMax().x;

    if (locked) {
        const std::string price_num = std::to_string(row.price);
        const float price_w =
            line + line * 0.25f + ImGui::CalcTextSize(price_num.c_str()).x + line * 0.6f;
        ImGui::SameLine();
        ImGui::SetCursorPosX(region_w - btn_w - price_w);
        draw_price(row.price, kind == ShopButtonKind::BuyDisabled);
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX(region_w - btn_w);

    if (kind == ShopButtonKind::BuyDisabled) {
        // Reduced opacity, non-interactive (clicks suppressed); price already red.
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.4f);
        ImGui::BeginDisabled();
        ImGui::Button("BUY", ImVec2{btn_w, btn_h});
        ImGui::EndDisabled();
        ImGui::PopStyleVar();
        ImGui::PopID();
        return;
    }

    const bool armed = (kind == ShopButtonKind::Confirm);
    if (armed) {
        ImGui::PushStyleColor(ImGuiCol_Button, theme::get_color(theme::ColorToken::AgainButtonArmed));
    }
    const bool clicked = ImGui::Button(button_label(kind), ImVec2{btn_w, btn_h});
    if (armed) {
        ImGui::PopStyleColor();
    }

    if (clicked) {
        switch (kind) {
            case ShopButtonKind::Buy:
                rt.shop_armed_buy = row.track;  // arm; a second click commits
                break;
            case ShopButtonKind::Confirm:
                if (rt.shop.on_buy) {
                    rt.shop.on_buy(row.track);
                }
                rt.shop_armed_buy.reset();
                break;
            case ShopButtonKind::Add:
                if (rt.shop.on_add) {
                    rt.shop.on_add(row.track);
                }
                rt.shop_armed_buy.reset();
                break;
            case ShopButtonKind::Remove:
                if (rt.shop.on_remove) {
                    rt.shop.on_remove(row.track);
                }
                rt.shop_armed_buy.reset();
                break;
            case ShopButtonKind::BuyDisabled:
                break;
        }
    }
    ImGui::PopID();
}

void render_genre_section(ModalRuntime& rt, const ShopSnapshot& snap, std::size_t genre) {
    const std::string_view title = audio::kMusicGenreNames[genre];
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::TextSecondary));
    ImGui::TextUnformatted(title.data(), title.data() + title.size());
    ImGui::PopStyleColor();
    // Each genre owns three consecutive track ids (audio_paths.hpp layout).
    for (std::size_t k = 0; k < 3; ++k) {
        const std::size_t id = genre * 3 + k;
        render_shop_row(rt, snap.rows[id]);
    }
}

}  // namespace

ShopButtonKind shop_button_kind(const ShopRowView& row, bool armed) noexcept {
    if (row.owned) {
        return row.in_pool ? ShopButtonKind::Remove : ShopButtonKind::Add;
    }
    if (!row.affordable) {
        return ShopButtonKind::BuyDisabled;
    }
    return armed ? ShopButtonKind::Confirm : ShopButtonKind::Buy;
}

void render_shop_view(ModalRuntime& runtime) {
    if (!modal_begin_centered("##shop_modal", kClusterModalWidthFrac, kClusterModalHeightFrac)) {
        modal_end();
        return;
    }
    const ShopSnapshot snap = runtime.shop.snapshot ? runtime.shop.snapshot() : ShopSnapshot{};

    // Pinned header: the X, the Leaderboard-swap icon, the Spendable count, the pill,
    // and the lock banner stay fixed; only the track list scrolls, in a contained child
    // below. This keeps the header-right cluster from reflowing when the list overflows
    // (no outer scrollbar), so the swap control holds its position across Shop<->
    // Leaderboard swaps. Mirrors the Settings/Leaderboard frame.
    const bool x_clicked = modal_draw_x_close(kShopShellClose);
    const bool to_leaderboard = draw_shop_header_right(snap.spendable);
    modal_draw_pill_header(assets::AssetId::IconShop, "Shop");
    modal_draw_lock_banner();

    // Zero the child's window padding so the rows keep the same left/right extents they
    // had before the list was wrapped (rows right-align to the content width).
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
    ImGui::BeginChild("##shop_body", ImVec2{0.0f, 0.0f}, false);
    // The track rows are interactive controls — dimmed + click-suppressed under the
    // tutorial lock; the X close and the Leaderboard icon above stay live.
    modal_begin_locked_controls();
    for (std::size_t g = 0; g < audio::kMusicGenreCount; ++g) {
        render_genre_section(runtime, snap, g);
    }
    modal_end_locked_controls();
    ImGui::EndChild();
    ImGui::PopStyleVar();

    const bool dismiss = x_clicked || modal_click_outside_dismissed();
    modal_end();

    if (to_leaderboard) {
        swap_to_leaderboard();
    } else if (dismiss) {
        close_modal();
    }
}

}  // namespace poker_trainer::modal
