#include "screens/game_screen.hpp"

#include "render/cards.hpp"
#include "render/chips.hpp"
#include "render/dealer.hpp"
#include "render/hud.hpp"
#include "render/layout.hpp"
#include "render/opponents.hpp"
#include "render/render_constants.hpp"
#include "render/table.hpp"

#include "animations/chip_slide.hpp"
#include "easter_egg/frog_toggle.hpp"

#include "backbone/animation_clock.hpp"
#include "backbone/modal_state.hpp"
#include "backbone/scenario_events.hpp"
#include "backbone/screen_state.hpp"

#include "engine/scenario.hpp"
#include "settings/settings.hpp"
#include "theme/theme_tokens.hpp"

#include "audio/audio.hpp"
#include "audio/audio_paths.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <imgui.h>

#include "bridge/game_launch.hpp"
#include "bridge/screen_dispatch.hpp"

#include "math/interrogator.hpp"

#ifdef __EMSCRIPTEN__
// load_frog_bundle() (Tier-4 on-demand fetch) lives in the wasm-only
// bridge_platform layer; the native `game` library / test never links it, so the
// call is compiled out there. The Frog state machine itself stays native-testable.
#include "bridge/tier_orchestrator.hpp"
#endif

namespace poker_trainer::screens {

namespace {

namespace rnd = poker_trainer::render;

// Top-center community-card readout (Part B): a flat, screen-space copy of the
// board so the user reads the cards without parsing the perspective felt. Anchored
// near the top edge at a legible size (visual-pass values; the on-felt board is
// untouched).
constexpr float kBoardReadoutTopFrac = 0.015f;  // top anchor, fraction of height
constexpr float kBoardReadoutScale = 1.0f;      // x the base card size

// File-static pointer to the installed runtime, for the easter-egg-active free
// query Z13 reads later. Set once at install; mirrors the file-static integration
// state the bridge uses (e.g. game_launch.cpp's active scenario). Null before
// install / in tests that never install.
const GameScreenRuntime* g_installed_runtime = nullptr;

[[nodiscard]] ImU32 token_u32(theme::ColorToken token) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(token));
}

[[nodiscard]] float lerp(float a, float b, float t) noexcept { return a + (b - a) * t; }

// Build the per-frame UI snapshot from the live settings (or documented defaults).
[[nodiscard]] GameUiState ui_from_settings(const GameScreenRuntime& runtime) {
    const settings::Settings s = runtime.settings_source ? runtime.settings_source()
                                                         : settings::Settings{};
    return GameUiState{
        .show_hud = s.gameplay.show_hud,
        .cash_mode = s.units.cash_mode,
        .denomination_mode = s.gameplay.chip_denomination_mode,
    };
}

// Draw the top-right persistent cluster (Shop / Help / Settings / X). The icons
// are now Tab focus stops (their ids are the Game focus-list tail Z08 composes —
// see install_game_screen); the focused one shows the same 2px border_focus ring
// the math boxes use. Their click/Enter activation (opening the modals / Leave-
// Drill popup) is still the Z11 no-op seam. The Z10 Visual Countdown sits below
// this region and is left empty.
//
// The ring is drawn into the SAME background draw list as the icons (not via
// bridge::draw_focus_ring_rect, whose GetWindowDrawList target is unsafe here:
// this renderer runs between NewFrame and Render with no active ImGui window).
// This mirrors Z07's background-cluster focus ring (render_util::focus_outline),
// gated on keyboard mode + focus exactly as the bridge helper would be.
void draw_cluster_stub(ImDrawList* dl, const rnd::GameLayout& layout) {
    constexpr std::array<const char*, 4> labels = {"Sh", "?", "Se", "X"};
    const float box = layout.w * 0.024f;
    const float gap = box * 0.35f;
    float x = layout.cluster_anchor.x;
    const float y = layout.cluster_anchor.y;
    const ImU32 border = token_u32(theme::ColorToken::BorderDefault);
    const ImU32 text = token_u32(theme::ColorToken::TextSecondary);
    const ImU32 focus_ring = token_u32(theme::ColorToken::BorderFocus);
    const bool keyboard = backbone::is_keyboard_mode_active();
    const backbone::FocusableId focused = backbone::get_focused_element();
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const char* label = labels[i];
        dl->AddRect(ImVec2{x, y}, ImVec2{x + box, y + box}, border, 4.0f, 0, 1.0f);
        const ImVec2 ts = ImGui::CalcTextSize(label);
        dl->AddText(ImVec2{x + (box - ts.x) * 0.5f, y + (box - ts.y) * 0.5f}, text, label);
        if (keyboard && focused == kGameClusterFocusIds[i]) {
            dl->AddRect(ImVec2{x, y}, ImVec2{x + box, y + box}, focus_ring, 4.0f, 0, 2.0f);
        }
        x += box + gap;
    }
}

// Handle the mouse-only Frog easter egg: count clicks on the dealer hit region,
// and on a completed 22-click toggle load the Tier-4 bundle once and play the
// toggle SFX. Keyboard cannot reach this (the dealer is in no focus list); paused
// while a modal is open.
void handle_dealer_click(GameScreenRuntime& runtime, const rnd::GameLayout& layout) {
    if (backbone::is_any_modal_open() || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        return;
    }
    const ImVec2 m = ImGui::GetIO().MousePos;
    const bool in_dealer = m.x >= layout.dealer_tl.x &&
                           m.x <= layout.dealer_tl.x + layout.dealer_w &&
                           m.y >= layout.dealer_tl.y &&
                           m.y <= layout.dealer_tl.y + layout.dealer_h;
    if (!in_dealer) {
        return;
    }
    const easter_egg::FrogClickOutcome outcome =
        easter_egg::register_dealer_click(runtime.frog);
    if (outcome != easter_egg::FrogClickOutcome::Toggled) {
        return;
    }
    // First toggle in the session fetches the Frog bundle (Tier 4, on demand).
#ifdef __EMSCRIPTEN__
    if (!runtime.frog.tier4_requested) {
        bridge::load_frog_bundle();
    }
#endif
    runtime.frog.tier4_requested = true;
    // The Butler<->Frog toggle SFX (Z08 owns this cue; Z03 owns the spawn cues).
    audio::play_sfx(audio::SfxId::FrogToggle);
}

// Draw the top-center community-card readout (Part B). ADDITIVE: the on-felt
// community cards (render step 4) are unchanged. Each card routes through
// rnd::draw_card -> bridge::draw_asset_image, so the readout swaps to real art with
// the felt copy (zero code change). Always visible: the board is not a HUD number,
// so Show/Hide HUD does not gate it.
void draw_board_readout(ImDrawList* dl, const rnd::GameLayout& layout,
                        const engine::ScenarioState& scenario) {
    if (scenario.board_count == 0) {
        return;  // pre-flop: no community cards to read
    }
    rnd::draw_card_fan(dl, layout.w * 0.5f, layout.h * kBoardReadoutTopFrac, scenario.board.data(),
                       scenario.board_count, kBoardReadoutScale);
}

}  // namespace

void render_game_screen(GameScreenRuntime& runtime, interrogator::InterrogatorRuntime& interrogator,
                        const engine::ScenarioState& scenario, const GameUiState& ui) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const rnd::GameLayout layout = rnd::compute_layout(vp->Size.x, vp->Size.y);
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // 1) Room + table (behind everything). First-person: the authored art recedes
    //    from the camera; everything below is positioned into that frame.
    rnd::draw_table(dl, layout);

    // 2) Denomination set: drives every chip cluster (pot, opponent stacks, bet).
    const std::span<const rnd::Denomination> set =
        rnd::denomination_set(scenario.big_blind, ui.denomination_mode);

    // 3) Opponent seats: position labels (always) + the effective stack as a greedy
    //    chip cluster on the felt + a floating HUD amount (HUD on), arrayed from the
    //    hero's lower-LEFT, up the left, across the far (top) rim, to the upper-
    //    RIGHT — each scaled down as it recedes (perspective). The right side is the
    //    seated dealer's; the hero is dead-center at the bottom.
    rnd::draw_opponent_seats(dl, layout, scenario, set, ui.cash_mode, ui.show_hud);

    // 4) Community cards: mid-table, above the pot (board_count revealed). The
    //    hero's hole cards are drawn later, dead-center at the bottom near the camera.
    rnd::draw_card_fan(dl, layout.table_center.x, layout.table_center.y - layout.table_ry * 0.20f,
                       scenario.board.data(), scenario.board_count);

    // 5) Chips. Legend set drives every cluster's denomination columns.
    const int active_slot = rnd::active_opponent_slot();

    if (scenario.side_pot) {
        // Side pot present: two slightly-offset pools + the all-in marker, no bet
        // push (the all-in opponent's chip area is empty). The engine carries only
        // a side_pot bool and one total `pot` (no per-pot contributions), so the
        // main/side split is a VISUAL convention: an even split of the displayed
        // pot. Grading uses the engine truth, not this visual (see report).
        const int side_amt = scenario.pot / 2;
        const int main_amt = scenario.pot - side_amt;
        const std::vector<rnd::ChipColumn> main_cols = rnd::decompose(main_amt, set);
        const std::vector<rnd::ChipColumn> side_cols = rnd::decompose(side_amt, set);
        rnd::draw_chip_cluster(dl, rnd::cluster_base_x(layout.pot.x, main_cols.size()), layout.pot.y,
                               main_cols);
        rnd::draw_chip_cluster(
            dl, rnd::cluster_base_x(layout.pot.x + rnd::kSidePotOffsetX, side_cols.size()),
            layout.pot.y + rnd::kSidePotOffsetY, side_cols);
        rnd::draw_all_in_marker(dl, layout, active_slot);
    } else {
        // Single main pot.
        const std::vector<rnd::ChipColumn> pot_cols = rnd::decompose(scenario.pot, set);
        rnd::draw_chip_cluster(dl, rnd::cluster_base_x(layout.pot.x, pot_cols.size()), layout.pot.y,
                               pot_cols);

        // Caller: the active opponent's bet is pushed forward + a floating bet
        // amount. Aggressor: nothing here (the empty chip area is the cue). The bet
        // is that far seat's chips, so it carries the seat's perspective scale.
        if (rnd::opponent_chip_state(scenario.type) == rnd::OpponentChipState::PushedForward) {
            const rnd::SeatSpot active = rnd::seat_spot(layout, active_slot);
            const rnd::Pt seat = active.pos;
            const float sc = active.scale;
            // Forward-pushed rest position: partway from the seat toward the pot.
            const float fwd_x = lerp(seat.x, layout.pot.x, 0.55f);
            const float fwd_y = lerp(seat.y, layout.pot.y, 0.55f);
            const float t = runtime.spawn_seen
                                ? animations::chip_push_progress(runtime.spawn_ms,
                                                                 backbone::total_ms_since_app_start())
                                : 1.0f;
            const float bx = lerp(seat.x, fwd_x, t);
            const float by = lerp(seat.y, fwd_y, t);
            const std::vector<rnd::ChipColumn> bet_cols = rnd::decompose(scenario.faced_bet, set);
            rnd::draw_chip_cluster(dl, rnd::cluster_base_x(bx, bet_cols.size(), sc), by, bet_cols, sc);
            rnd::draw_floating_bet(dl, bx, by - (rnd::kChipRadius * 2.0f + 14.0f) * sc,
                                   scenario.faced_bet, ui.cash_mode, scenario.big_blind, ui.show_hud);
        }
    }

    // 6) Hero hole cards: nearest the camera at the bottom-RIGHT (the hero seat),
    //    drawn larger than the board and on top of the pot (always two, face-up),
    //    above the hero's seat label.
    const rnd::Pt hero_seat = rnd::seat_center(layout, 0);
    rnd::draw_card_fan(dl, hero_seat.x,
                       hero_seat.y - rnd::kCardHeight * rnd::kHeroCardScale - 6.0f,
                       scenario.hole.data(), static_cast<std::uint8_t>(scenario.hole.size()),
                       rnd::kHeroCardScale);

    // 7) Top-left info stack: legend (always), then pot size + blinds (HUD on).
    float info_y = layout.info_anchor.y;
    rnd::draw_denomination_legend(dl, layout.info_anchor.x, info_y, set);
    info_y += rnd::kChipRadius * 2.0f + ImGui::GetTextLineHeight() + 10.0f;
    info_y += rnd::draw_pot_size(dl, layout.info_anchor.x, info_y, scenario, ui.cash_mode, ui.show_hud);
    rnd::draw_blinds(dl, layout.info_anchor.x, info_y, scenario, ui.cash_mode, ui.show_hud);

    // 8) Dealer (right side, profile Butler or front-facing Frog) + the cluster.
    rnd::draw_dealer(dl, layout, easter_egg::frog_active(runtime.frog));
    draw_cluster_stub(dl, layout);

    // 8b) Top-center community-card readout (HUD overlay, screen-space). Additive
    //    flat copy of the board for legibility; always visible (not HUD-gated).
    draw_board_readout(dl, layout, scenario);

    // 9) Mouse-only Frog easter egg on the dealer hit region.
    handle_dealer_click(runtime, layout);

    // 10) Math inputs LAST so they compose on top (Z09 owns them; do not reimplement
    //    — call render_math_inputs). They are an ImGui window, above the background
    //    draw list this screen draws into.
    interrogator::render_math_inputs(interrogator, scenario);
}

void install_game_screen(GameScreenRuntime& runtime,
                         interrogator::InterrogatorRuntime& interrogator) {
    g_installed_runtime = &runtime;

    // Compose the persistent-cluster focus tail for Z09: when Z09 (re)registers the
    // Game focus list per scenario / per tier, it appends these ids after its math
    // segment so Tab reaches Shop/Help/Settings/X and wraps from X to the first
    // input (SEAM(Z08/Z11) — the icons' activation is still a Z11 no-op).
    interrogator.cluster_focus_tail.assign(kGameClusterFocusIds.begin(),
                                           kGameClusterFocusIds.end());

    // Capture the spawn timestamp (chip-push start) and reset the Frog click
    // counter on every new scenario. The focus-list (re)registration is Z09's,
    // driven by its own scenario_spawned subscription — Z08 does not touch it.
    (void)backbone::subscribe_scenario_spawned(
        [&runtime](const backbone::ScenarioSpawnedEvent&) {
            runtime.spawn_ms = backbone::total_ms_since_app_start();
            runtime.spawn_seen = true;
            easter_egg::reset_click_count(runtime.frog);
        },
        "game_screen.scenario_spawned");

    // Take over the Game renderer (single-slot, last-writer-wins; replaces Z09's
    // W2 placeholder). The renderer reads the single authoritative scenario from
    // the bridge and composes Z09 by calling render_math_inputs itself.
    bridge::register_screen_renderer(backbone::ScreenId::Game, [&runtime, &interrogator] {
        const engine::ScenarioState* scenario = bridge::active_scenario();
        if (scenario == nullptr) {
            // Game without an active scenario should not happen (set_screen(Game,
            // id) requires one); clear to the room background and skip the rest.
            const ImGuiViewport* vp = ImGui::GetMainViewport();
            ImGui::GetBackgroundDrawList()->AddRectFilled(
                ImVec2{0.0f, 0.0f}, ImVec2{vp->Size.x, vp->Size.y},
                ImGui::ColorConvertFloat4ToU32(theme::get_color(theme::ColorToken::BgPrimary)));
            return;
        }
        render_game_screen(runtime, interrogator, *scenario, ui_from_settings(runtime));
    });
}

bool easter_egg_frog_active() noexcept {
    return g_installed_runtime != nullptr && g_installed_runtime->frog.frog_active;
}

}  // namespace poker_trainer::screens
