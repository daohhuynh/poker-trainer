#include "tutorial/overlay.hpp"

#include "tutorial/callout.hpp"
#include "tutorial/forced_settings.hpp"
#include "tutorial/step_sequencer.hpp"
#include "tutorial/tutorial.hpp"

#include "animations/button_morph.hpp"

#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"
#include "backbone/modal_state.hpp"
#include "backbone/screen_state.hpp"

#include "engine/scenario.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>

#include "bridge/game_launch.hpp"
#include "math/input_boxes.hpp"
// Header-only geometry from Zone 08: layout.hpp is entirely inline and only the
// inline *_for overloads of hud.hpp are used, so no link edge on `game` is added.
#include "render/hud.hpp"
#include "render/layout.hpp"
#include "render/opponents.hpp"
#include "render/render_constants.hpp"
#include "math/interrogator.hpp"
#include "modal/confirm_modal.hpp"
#include "modal/modals.hpp"

#include <imgui.h>

// Forward declaration of the Tutorial Complete screen's install (screens/, Z14).
namespace poker_trainer::screens {
void install_tutorial_complete_screen();
}

namespace poker_trainer::tutorial {

namespace {

namespace be = backbone;

// The single installed runtime pointer (CLAUDE.md sec.10 -- non-owning, set by
// install_tutorial; mirrors modal::modal_runtime()).
TutorialRuntime* g_runtime = nullptr;

constexpr std::string_view kPromptMessage =
    "New here? Take a quick tour to learn how the trainer works.";

[[nodiscard]] Canvas viewport_canvas() {
    const ImVec2 size = ImGui::GetMainViewport()->Size;
    return Canvas{size.x, size.y};
}

[[nodiscard]] bool pt(const be::MouseEvent& e, const Rect& r) noexcept {
    return e.x >= r.x && e.x <= r.x + r.w && e.y >= r.y && e.y <= r.y + r.h;
}

[[nodiscard]] be::ScreenId screen_id_for(StepScreen s) noexcept {
    switch (s) {
        case StepScreen::Root: return be::ScreenId::Root;
        case StepScreen::ModeSelection: return be::ScreenId::ModeSelection;
        case StepScreen::Game: return be::ScreenId::Game;
        case StepScreen::PostRound: return be::ScreenId::PostRound;
    }
    return be::ScreenId::Root;
}

[[nodiscard]] be::TutorialPhase phase() noexcept {
    return be::read_screen_state().tutorial_state.phase;
}

[[nodiscard]] std::size_t script_len() noexcept { return tutorial_script_len(); }

[[nodiscard]] bool is_math_step(AdvanceKind a) noexcept {
    return a == AdvanceKind::MathStage1 || a == AdvanceKind::MathStage2 ||
           a == AdvanceKind::MathSubmit || a == AdvanceKind::MathTiers234;
}

[[nodiscard]] std::optional<int> digit_of(be::KeyCode code) noexcept {
    switch (code) {
        case be::KeyCode::Digit1: return 1;
        case be::KeyCode::Digit2: return 2;
        case be::KeyCode::Digit3: return 3;
        case be::KeyCode::Digit4: return 4;
        case be::KeyCode::Digit5: return 5;
        case be::KeyCode::Digit6: return 6;
        default: return std::nullopt;
    }
}

// ----- Spotlight geometry (Group C) -----

// The Game-screen spotlights below all resolve through render::compute_layout, the
// same function Zone 08 lays the table out with, so reshaping the table moves them
// automatically. They used to be hand-copied canvas fractions; when the felt was
// rebuilt every one of them ended up pointing at empty room.
//
// render/layout.hpp and render/hud.hpp are safe to include here even though
// `tutorial` does not link `game`: the layout helpers are header-only inline, and the
// only hud entry point used is the inline canvas_ui_scale_for overload.

[[nodiscard]] render::GameLayout game_layout(Canvas c) noexcept {
    return render::compute_layout(c.width, c.height);
}

// Pad a rect outward so the lit region comfortably contains its element rather than
// clipping its edge. Fractions of the canvas, so it holds at any size.
[[nodiscard]] Rect padded(Rect r, Canvas c, float fx, float fy) noexcept {
    const float px = c.width * fx;
    const float py = c.height * fy;
    return Rect{r.x - px, r.y - py, r.w + 2.0f * px, r.h + 2.0f * py};
}

// A rect centred on (cx, cy).
[[nodiscard]] Rect centered_rect(float cx, float cy, float w, float h) noexcept {
    return Rect{cx - w * 0.5f, cy - h * 0.5f, w, h};
}

// The top-left denomination legend row: chip disks with their dollar values beneath,
// anchored at info_anchor (game_screen.cpp draws it there first, before the info
// lines). Height covers a chip plus its label.
[[nodiscard]] Rect game_legend_rect(Canvas c) noexcept {
    const render::GameLayout L = game_layout(c);
    const float chip = render::kChipRadius * 2.0f * L.chip_scale;
    const float label = render::readout_line_height_for(render::kReadoutRelSecondary, c.width,
                                                        c.height);
    // Five denominations at most, each a chip plus the column pitch.
    const float w = 5.0f * render::kChipColumnPitch * L.chip_scale;
    return padded(Rect{L.info_anchor.x, L.info_anchor.y, w, chip + label}, c, 0.006f, 0.008f);
}

// Pot / Blinds / To Call / Opp Fold: the stacked lines directly under the legend.
[[nodiscard]] Rect game_info_lines_rect(Canvas c) noexcept {
    const Rect legend = game_legend_rect(c);
    const float line = render::readout_line_height_for(render::kReadoutRelPrimary, c.width,
                                                        c.height);
    return Rect{legend.x, legend.y + legend.h, legend.w, line * 4.0f};
}

// The on-felt "Opp fold: X%" readout. game_screen.cpp anchors it at
// table_center.y - table_ry * 0.62.
[[nodiscard]] Rect game_table_opp_fold_rect(Canvas c) noexcept {
    const render::GameLayout L = game_layout(c);
    const float line = render::readout_line_height_for(render::kReadoutRelPrimary, c.width,
                                                        c.height);
    return centered_rect(L.table_center.x, L.table_center.y - L.table_ry * 0.62f,
                         c.width * 0.22f, line * 2.0f);
}

// The top-centre board readout: a five-card fan centred on the canvas at
// kBoardReadoutTopFrac, drawn at kBoardReadoutScale x canvas_ui_scale.
[[nodiscard]] Rect game_board_readout_rect(Canvas c) noexcept {
    constexpr float kTopFrac = 0.015f;      // game_screen.cpp kBoardReadoutTopFrac
    constexpr float kReadoutScale = 2.1f;   // game_screen.cpp kBoardReadoutScale
    const float s = kReadoutScale * render::canvas_ui_scale_for(c.width, c.height);
    // Widest case is the full five-card board; a shorter board sits inside this.
    const float w = render::kCardFanStep * s * 4.0f + render::kCardWidth * s;
    const float h = render::kCardHeight * s;
    return padded(Rect{c.width * 0.5f - w * 0.5f, c.height * kTopFrac, w, h}, c, 0.006f, 0.008f);
}

// The hero's two hole cards, drawn as a fan above the hero seat (slot 0).
[[nodiscard]] Rect game_hole_cards_rect(Canvas c) noexcept {
    const render::GameLayout L = game_layout(c);
    const render::SeatSpot hero = render::seat_spot(L, 0);
    const float s = hero.scale;
    const float w = render::kCardFanStep * s + render::kCardWidth * s;
    const float h = render::kCardHeight * s;
    // game_screen draws the fan with its BOTTOM on the seat point, label below.
    return padded(Rect{hero.pos.x - w * 0.5f, hero.pos.y - h, w, h}, c, 0.008f, 0.010f);
}

// The five opponent seats: the bounding box of their seat points, grown to take in
// each seat's chip cluster above it and its position letter below.
[[nodiscard]] Rect game_opponent_ring_rect(Canvas c) noexcept {
    const render::GameLayout L = game_layout(c);
    float min_x = c.width;
    float max_x = 0.0f;
    float min_y = c.height;
    float max_y = 0.0f;
    for (int slot = 1; slot <= 5; ++slot) {
        const render::SeatSpot s = render::seat_spot(L, slot);
        min_x = std::min(min_x, s.pos.x);
        max_x = std::max(max_x, s.pos.x);
        min_y = std::min(min_y, s.pos.y);
        max_y = std::max(max_y, s.pos.y);
    }
    // Above a seat point sits its chip stack and the amount over it; below, the
    // position letter. Both scale with the felt, so measure them the same way.
    const float above = (render::kMaxChipsPerColumn * render::kChipStackStep * 0.4f +
                         render::kChipRadius) * L.chip_scale +
                        render::readout_line_height_for(render::kReadoutRelPrimary, c.width,
                                                        c.height);
    const float below = render::kChipRadius * L.chip_scale +
                        render::readout_line_height_for(render::kReadoutRelSecondary, c.width,
                                                        c.height);
    const float side = render::kChipColumnPitch * 2.0f * L.chip_scale;
    return Rect{min_x - side, min_y - above, (max_x - min_x) + 2.0f * side,
                (max_y - min_y) + above + below};
}

// The corridor between the active opponent and the pot: where a Caller's pushed
// chips rest, and where an Aggressor's conspicuously-empty chip area is. Both
// spotlights point here. game_screen lerps the rest position 0.55 of the way from
// the seat toward the pot.
[[nodiscard]] Rect game_pushed_chips_rect(Canvas c) noexcept {
    const render::GameLayout L = game_layout(c);
    const render::SeatSpot active = render::seat_spot(L, render::active_opponent_slot());
    const float fx = active.pos.x + (L.pot.x - active.pos.x) * 0.55f;
    const float fy = active.pos.y + (L.pot.y - active.pos.y) * 0.55f;
    const float span = render::kChipColumnPitch * 5.0f * L.chip_scale * active.scale;
    const float tall = (render::kMaxChipsPerColumn * render::kChipStackStep * 0.4f +
                        render::kChipRadius * 2.0f) * L.chip_scale * active.scale;
    return centered_rect(fx, fy, span, tall);
}

// Zone 09's math input column. input_boxes.cpp anchors the window's LEFT-MIDDLE at
// (0.02w, 0.5h) -- the same left margin as Z08's info column above it -- and lets
// ImGui auto-size it, so the panel grows up and down about the vertical centre. Its
// width is content-driven (the four Bet Size buttons are fixed-pixel), so this takes
// the wider of that pixel floor and a canvas fraction rather than assuming either.
[[nodiscard]] Rect game_math_column_rect(Canvas c) noexcept {
    const render::GameLayout L = game_layout(c);
    constexpr float kBetButtonRowPx = 300.0f;  // the fixed-pixel content floor
    const float w = std::max(kBetButtonRowPx, c.width * 0.18f);
    // Header line, three label+input pairs, and the bet-size row.
    const float line = render::readout_line_height_for(render::kReadoutRelPrimary, c.width,
                                                       c.height);
    const float h = std::min(line * 10.0f, c.height * 0.55f);
    return Rect{L.info_anchor.x, c.height * 0.5f - h * 0.5f, w, h};
}

// The persistent top-right cluster, mirroring game_cluster_rects: four square icons
// from cluster_anchor, each layout.w * 0.024 with a 35% gap.
[[nodiscard]] Rect game_cluster_rect(Canvas c) noexcept {
    const render::GameLayout L = game_layout(c);
    const float box = L.w * 0.024f;
    const float gap = box * 0.35f;
    const float w = 4.0f * box + 3.0f * gap;
    return padded(Rect{L.cluster_anchor.x, L.cluster_anchor.y, w, box}, c, 0.005f, 0.008f);
}

// The Post-Round Again button rect, replicated EXACTLY from post_round_screen.cpp's
// layout so the spotlight + click interception land on the real button.
[[nodiscard]] Rect again_button_rect(Canvas c) noexcept {
    const float w = c.width;
    const float h = c.height;
    const float aw = w * 0.16f;
    const float ah = h * 0.085f;
    return Rect{w - w * 0.03f - aw, h - h * 0.04f - ah, aw, ah};
}

// The Post-Round stat (results) modal box, mirroring post_round_screen::compute_layout.
// The recap spotlights the WHOLE box so the rows + tier tabs read at full brightness
// (Group C) and the callout sits to the right of it (3 o'clock).
[[nodiscard]] Rect post_stat_box_rect(Canvas c) noexcept {
    const float w = c.width;
    const float h = c.height;
    const float mw = std::max(w * 0.5f, 360.0f);
    const float modal_top = h * 0.06f + h * 0.45f * 0.80f;  // dealer_top + dealer_h*0.80
    const float mh = std::min(h * 0.5f, h * 0.95f - modal_top);
    return Rect{w * 0.5f - mw * 0.5f, modal_top, mw, mh};
}

// The Post-Round front-facing dealer box, mirroring compute_layout.
[[nodiscard]] Rect post_dealer_rect(Canvas c) noexcept {
    const float w = c.width;
    const float h = c.height;
    const float dealer_h = h * 0.45f;
    const float dealer_w = dealer_h * 0.72f;
    return Rect{w * 0.5f - dealer_w * 0.5f, h * 0.06f, dealer_w, dealer_h};
}

// Resolve a SpotTarget to an on-screen rect. Root / Mode Selection use Zone 07's exact
// layout helpers; the Game / Post-Round rects mirror render::compute_layout /
// post_round_screen::compute_layout (the same hand-replicated pattern as
// again_button_rect), so the spotlights land on the real elements (Group C).
[[nodiscard]] std::optional<Rect> resolve_spotlight_impl(SpotTarget target, Canvas c) {
    namespace an = animations;
    const float w = c.width;
    const float h = c.height;
    auto region = [&](float x, float y, float rw, float rh) {
        return Rect{x * w, y * h, rw * w, rh * h};
    };
    switch (target) {
        case SpotTarget::None: return std::nullopt;

        case SpotTarget::RootLogo: return an::logo_rect(c);
        case SpotTarget::RootButtonGrid: {
            const Rect tlb = an::root_grid_button_rect(an::MorphButton::Play, c);
            const Rect brb = an::root_grid_button_rect(an::MorphButton::Help, c);
            return Rect{tlb.x, tlb.y, (brb.x + brb.w) - tlb.x, (brb.y + brb.h) - tlb.y};
        }
        case SpotTarget::RootSettings: return an::root_grid_button_rect(an::MorphButton::Settings, c);
        case SpotTarget::RootShop: return an::root_grid_button_rect(an::MorphButton::Shop, c);
        case SpotTarget::RootHelp: return an::root_grid_button_rect(an::MorphButton::Help, c);
        case SpotTarget::RootHome: return an::home_icon_rect(c);
        case SpotTarget::RootPlay: return an::root_grid_button_rect(an::MorphButton::Play, c);

        case SpotTarget::ModeCluster: {
            const Rect l = an::mode_button_target_rect(an::MorphButton::Shop, c);
            const Rect r = an::home_icon_rect(c);
            return Rect{l.x, l.y, (r.x + r.w) - l.x, (r.y + r.h) - l.y};
        }
        case SpotTarget::ModeAggressor: return an::mode_middle_button_rect(0, c);
        case SpotTarget::ModeCaller: return an::mode_middle_button_rect(1, c);
        case SpotTarget::ModeCustom: return an::mode_middle_button_rect(2, c);
        case SpotTarget::ModeStandard: return an::standard_button_rect(c);

        // Game table tour. These DERIVE from render::compute_layout and its seat
        // helpers rather than restating them as canvas fractions. The fractions they
        // replaced were written against an older, differently-shaped table and silently
        // went stale the moment it was reshaped -- which is the whole bug. Anything
        // added here must come off the layout too, or it will drift the same way.
        case SpotTarget::GameChipLegend: return game_legend_rect(c);
        case SpotTarget::GamePotBlinds: return game_info_lines_rect(c);
        case SpotTarget::GameOppFold: return game_table_opp_fold_rect(c);
        // The board is read from the top-centre readout -- it is deliberately ~2x the
        // on-felt cards, which are largely cosmetic -- so that is what gets lit. The
        // two are at opposite ends of the screen; spanning both would light most of the
        // canvas and defeat the spotlight.
        case SpotTarget::GameCommunityCards: return game_board_readout_rect(c);
        case SpotTarget::GameHoleCards: return game_hole_cards_rect(c);
        case SpotTarget::GameOpponents: return game_opponent_ring_rect(c);
        case SpotTarget::GamePushedChips: return game_pushed_chips_rect(c);
        case SpotTarget::GameClusterX: return game_cluster_rect(c);
        case SpotTarget::GameOppEmptyChips: return game_pushed_chips_rect(c);

        // Game math inputs -- the LEFT-CENTER input column where render_math_inputs
        // actually draws (window anchored at 0.04w, vertically centered), NOT the old
        // bottom-center phantom. The focus ring on the taught box + the callout point
        // the eye to the specific input.
        case SpotTarget::GameMathPotOdds:
        case SpotTarget::GameMathOuts:
        case SpotTarget::GameMathEquity:
        case SpotTarget::GameMathEv:
        case SpotTarget::GameMathFold:
        case SpotTarget::GameMathTierEv:
        case SpotTarget::GameMathBetSize:
            return game_math_column_rect(c);

        // Post-Round -- mirror compute_layout. The recap rows / tier tabs / summary all
        // spotlight the whole stat box so it reads at full brightness (Group C).
        case SpotTarget::PostStatColumns:
        case SpotTarget::PostTimeGradeRow:
        case SpotTarget::PostOverallRow:
        case SpotTarget::PostTierTabs:
        case SpotTarget::PostSummaryTab:
            return post_stat_box_rect(c);
        case SpotTarget::PostDealer: return post_dealer_rect(c);
        case SpotTarget::PostScenarioId: return region(0.02f, 0.03f, 0.26f, 0.16f);
        case SpotTarget::PostAgain: return again_button_rect(c);
    }
    return std::nullopt;
}

// Internal alias: everything in this TU already calls resolve_spotlight.
[[nodiscard]] std::optional<Rect> resolve_spotlight(SpotTarget target, Canvas c) {
    return resolve_spotlight_impl(target, c);
}

[[nodiscard]] const char* bet_tier_name(engine::BetTier t) noexcept {
    switch (t) {
        case engine::BetTier::OneThirdPot: return "1/3 Pot";
        case engine::BetTier::HalfPot: return "1/2 Pot";
        case engine::BetTier::FullPot: return "Full Pot";
        case engine::BetTier::Overbet: return "Overbet";
    }
    return "?";
}

// Snap focus to the input box a math sub-step walks, so the keybind hints, digit jumps,
// and the focus ring all act on the spotlit box. current_tier is 0 throughout the
// tutorial's tier-1 walkthrough.
[[nodiscard]] std::optional<be::FocusableId> math_focus_for(const SubStep& s) {
    using interrogator::kFocusBetSizeGroup;
    using interrogator::kFocusEquity;
    using interrogator::kFocusEvCaller;
    using interrogator::kFocusEvTier;
    using interrogator::kFocusFoldTier;
    using interrogator::kFocusOuts;
    using interrogator::kFocusPotOdds;
    switch (s.math_input) {
        case engine::InputId::PotOdds: return kFocusPotOdds;
        case engine::InputId::Outs: return kFocusOuts;
        case engine::InputId::Equity: return kFocusEquity;
        case engine::InputId::Ev: return (s.step == 6) ? kFocusEvTier[0] : kFocusEvCaller;
        case engine::InputId::FoldProbability: return kFocusFoldTier[0];
        case engine::InputId::BetSize: return kFocusBetSizeGroup;
    }
    return std::nullopt;
}

// The positional digit key that focuses this math sub-step's box in the live scenario
// (1-based; 0 when it can't be resolved yet). The same value the callout names and the
// stage-1 advance validates -- read from Z09's ordering, not re-derived.
[[nodiscard]] int positional_digit_for(const SubStep& s) {
    const engine::ScenarioState* sc = bridge::active_scenario();
    const std::optional<be::FocusableId> f = math_focus_for(s);
    if (sc == nullptr || !f.has_value()) {
        return 0;
    }
    return interrogator::positional_index_of(*sc, /*current_tier=*/0, *f);
}

// The live "= <numbers>" line a MathStage2 callout appends, read from the single
// authoritative active scenario.
[[nodiscard]] std::string live_math_line(const SubStep& s) {
    const engine::ScenarioState* sc = bridge::active_scenario();
    if (sc == nullptr) {
        return {};
    }
    switch (s.math_input) {
        case engine::InputId::PotOdds:
            return std::format("Pot Odds = {} / ({} + {}) = {:.0f}%", sc->faced_bet, sc->pot,
                               sc->faced_bet, sc->caller_pot_odds_pct);
        case engine::InputId::Outs:
            // Name the cards that make the count (not just the number). The caller
            // walkthrough is locked to the teaching seed; the seed-lock test guards the
            // hand, so this specific naming stays accurate.
            if (sc->id == kTutorialScenarioCallerSeed) {
                return std::format(
                    "{} outs: any 4 completes your A-2-3-4-5 straight (the four 4s).",
                    sc->caller_outs);
            }
            return std::format("You have {} outs here.", sc->caller_outs);
        case engine::InputId::Equity: {
            const int mult = (sc->street == engine::Street::Flop) ? 4 : 2;
            return std::format("Equity = {} x {} = {:.0f}%", sc->caller_outs, mult,
                               sc->caller_equity_pct);
        }
        case engine::InputId::Ev:
            if (s.step == 6) {
                return std::format("EV at this size = {:.0f}", sc->tiers[0].ev);
            }
            return std::format("EV = {:.0f}", sc->caller_ev);
        case engine::InputId::FoldProbability: {
            // Breakeven Fold % = bet / (pot + bet) for the walked tier (tier 0). The box
            // grades this breakeven, NOT the opponent's fold %, which is shown on screen
            // (tiers[0].fold_probability) -- teach the comparison between them.
            const double bet = sc->tiers[0].bet_dollars;
            const double breakeven = (sc->pot + bet > 0.0) ? 100.0 * bet / (sc->pot + bet) : 0.0;
            const int shown = static_cast<int>(std::lround(sc->tiers[0].fold_probability * 100.0));
            return std::format(
                "Breakeven Fold % = {:.0f} / ({} + {:.0f}) = {:.0f}%. They fold {}% here, "
                "so this bet {}.",
                bet, sc->pot, bet, breakeven, shown,
                (sc->tiers[0].ev >= 0.0) ? "profits" : "does not profit");
        }
        case engine::InputId::BetSize:
            return std::format("Best size for this hand: {}.", bet_tier_name(sc->correct_bet_tier));
    }
    return {};
}

// Compose the callout: the static teaching text, plus the live action line the overlay
// owns -- "Press N to focus this input" on a stage-1 (N is the positional keybind),
// then the worked "= <numbers>" line + "Type your answer, then Tab" on a stage-2.
[[nodiscard]] std::string compose_callout(const SubStep& s) {
    std::string out{s.callout};
    switch (s.advance) {
        case AdvanceKind::MathStage1: {
            const int n = positional_digit_for(s);
            if (s.math_input == engine::InputId::BetSize) {
                out += (n > 0) ? std::format("\n\nPress {} to highlight the Bet Size buttons.", n)
                               : std::string{"\n\nPress the highlighted number to select the Bet "
                                             "Size buttons."};
            } else {
                out += (n > 0) ? std::format("\n\nPress {} to focus this input.", n)
                               : std::string{"\n\nPress the highlighted number to focus this input."};
            }
            break;
        }
        case AdvanceKind::MathStage2: {
            const std::string live = live_math_line(s);
            if (!live.empty()) {
                out += "\n\n";
                out += live;
            }
            out += (s.math_input == engine::InputId::BetSize)
                       ? "\n\nPress 1-4 to choose a size, then Tab to continue."
                       : "\n\nType your answer, then Tab.";
            break;
        }
        case AdvanceKind::MathSubmit: {
            // The caller's last input (EV) collapses its worked-number stage into the
            // submit step: append the live "EV = <n>" line; the static callout already
            // ends with the "press Enter to submit" instruction (no Tab prompt).
            const std::string live = live_math_line(s);
            if (!live.empty()) {
                out += "\n\n";
                out += live;
            }
            break;
        }
        default:
            break;
    }
    return out;
}

// ----- Trapped tutorial controls (Group A / D) -----

// The keyboard-reachable controls of the current sub-step, in Tab order. While the
// overlay is active these are the ONLY focusable controls -- the underlying screen's
// elements are never reached (the handlers below consume Tab/Enter/Space at the
// TutorialOverlay router priority, above every screen). Empty on a math step, where
// Tab/keys drive the math advance instead.
enum class Ctl : std::uint8_t {
    Previous,  // step one tour step back
    Primary,   // Next (tour) or the scripted action (modal open / launch / again)
    Skip,      // open the Skip-Tutorial confirmation
};

struct ControlSet {
    std::array<Ctl, 3> items{};
    std::size_t n{0};
};

// Whether the previous sub-step is a same-screen tour step we can step back onto.
// Previous never crosses a screen boundary or steps onto an action already performed.
[[nodiscard]] bool previous_available(const TutorialRuntime& t) {
    if (t.cursor == 0 || t.cursor >= script_len()) {
        return false;
    }
    const SubStep& cur = tutorial_script()[t.cursor];
    if (!advances_on_next(cur.advance)) {
        return false;  // Previous only on the Next-bearing tour steps
    }
    const SubStep& prev = tutorial_script()[t.cursor - 1];
    return prev.screen == cur.screen && advances_on_next(prev.advance);
}

[[nodiscard]] ControlSet controls_for(const TutorialRuntime& t) {
    ControlSet cs{};
    if (t.cursor >= script_len()) {
        return cs;
    }
    const SubStep& s = tutorial_script()[t.cursor];
    if (is_math_step(s.advance)) {
        return cs;  // empty: the math walkthrough owns Tab / keypresses
    }
    if (advances_on_next(s.advance) && previous_available(t)) {
        cs.items[cs.n++] = Ctl::Previous;
    }
    cs.items[cs.n++] = Ctl::Primary;
    cs.items[cs.n++] = Ctl::Skip;
    return cs;
}

[[nodiscard]] std::size_t primary_index(const TutorialRuntime& t) {
    const ControlSet cs = controls_for(t);
    for (std::size_t i = 0; i < cs.n; ++i) {
        if (cs.items[i] == Ctl::Primary) {
            return i;
        }
    }
    return 0;
}

[[nodiscard]] Ctl focused_control(const TutorialRuntime& t) {
    const ControlSet cs = controls_for(t);
    if (cs.n == 0) {
        return Ctl::Skip;  // unused (no controls on math steps)
    }
    return cs.items[std::min(t.control_index, cs.n - 1)];
}

void reset_controls(TutorialRuntime& t) { t.control_index = primary_index(t); }

void cycle_control(TutorialRuntime& t, bool reverse) {
    const ControlSet cs = controls_for(t);
    if (cs.n == 0) {
        return;
    }
    std::size_t cur = std::min(t.control_index, cs.n - 1);
    cur = reverse ? (cur + cs.n - 1) % cs.n : (cur + 1) % cs.n;
    t.control_index = cur;
}

void sync_backbone_step(const TutorialRuntime& t) {
    be::set_tutorial_state(
        be::TutorialState{be::TutorialPhase::Active, coarse_step_for_index(t.cursor)});
}

// Move to the next sub-step and, when it is a math stage, snap focus to its box.
void advance_cursor(TutorialRuntime& t) {
    if (t.cursor < script_len()) {
        ++t.cursor;
    }
    t.explore_seen_open = false;
    reset_controls(t);
    sync_backbone_step(t);
    if (t.cursor >= script_len()) {
        return;
    }
    const SubStep& s = tutorial_script()[t.cursor];
    if (is_math_step(s.advance) && s.advance != AdvanceKind::MathTiers234) {
        if (const std::optional<be::FocusableId> f = math_focus_for(s)) {
            be::activate_keyboard_mode();
            be::snap_focus_to(*f);
        }
    }
}

// Step one tour step back (the Previous control). Only invoked when
// previous_available, so it lands on a same-screen Next-bearing step.
void step_back(TutorialRuntime& t) {
    if (t.cursor == 0) {
        return;
    }
    --t.cursor;
    t.explore_seen_open = false;
    reset_controls(t);
    sync_backbone_step(t);
}

void dismiss_prompt(TutorialRuntime& t) {
    if (t.seams.mark_prompt_seen) {
        t.seams.mark_prompt_seen();  // Skip / dismiss -> has_seen_tutorial_prompt = true
    }
    be::set_tutorial_state(be::TutorialState{be::TutorialPhase::Inactive, 0});
}

void maybe_trigger_prompt(TutorialRuntime& t) {
    if (t.prompt_shown_this_session) {
        return;
    }
    if (be::read_screen_state().current != be::ScreenId::Root) {
        return;  // the flag is checked specifically on the Root screen
    }
    if (!t.seams.has_seen_prompt || t.seams.has_seen_prompt()) {
        return;  // already finished/skipped once -> no prompt
    }
    t.prompt_shown_this_session = true;
    t.control_index = 0;  // default focus on "Start Tutorial" (control 0; Skip is 1)
    be::set_tutorial_state(be::TutorialState{be::TutorialPhase::Prompt, 0});
}

void finish_to_complete_screen(TutorialRuntime& t) {
    if (t.seams.mark_completed) {
        t.seams.mark_completed();  // has_completed_tutorial = true on reaching the screen
    }
    t.cursor = script_len();
    // Phase Complete: the overlay stops, the Game/Post-Round tutorial behaviors turn
    // off, and modals are no longer locked (the Sign Up modal must be interactive).
    be::set_tutorial_state(be::TutorialState{be::TutorialPhase::Complete, kStepCount});
    be::set_screen(be::ScreenId::TutorialComplete, std::nullopt);
}

// Advance the action / screen-transition sub-steps. Runs once per frame.
void poll_advance(TutorialRuntime& t) {
    if (t.cursor >= script_len()) {
        return;
    }
    const SubStep& s = tutorial_script()[t.cursor];
    const be::ScreenId screen = be::read_screen_state().current;
    switch (s.advance) {
        case AdvanceKind::ModalExplore: {
            if (be::is_any_modal_open()) {
                t.explore_seen_open = true;
            } else if (t.explore_seen_open) {
                advance_cursor(t);
            }
            break;
        }
        case AdvanceKind::LeaderboardExplore: {
            // Reached the real way: mark "seen" only when the LEADERBOARD modal is open
            // (opened via the in-Shop swap icon), so merely reopening the Shop does not
            // advance. Advance once it has been opened and then fully closed.
            const std::optional<be::ModalId> mid = be::current_modal_id();
            if (mid.has_value() && *mid == modal::kLeaderboardModalId) {
                t.explore_seen_open = true;
            } else if (t.explore_seen_open && !be::is_any_modal_open()) {
                advance_cursor(t);
            }
            break;
        }
        case AdvanceKind::CustomExplore: {
            const bool open = t.seams.custom_popup_open && t.seams.custom_popup_open();
            if (open) {
                t.explore_seen_open = true;
            } else if (t.explore_seen_open) {
                advance_cursor(t);
            }
            break;
        }
        case AdvanceKind::PlayMorph:
            if (screen == be::ScreenId::ModeSelection) {
                advance_cursor(t);
            }
            break;
        case AdvanceKind::LaunchCaller:
        case AdvanceKind::LaunchAggressor:
            if (screen == be::ScreenId::Game) {
                advance_cursor(t);
            }
            break;
        case AdvanceKind::MathSubmit:
        case AdvanceKind::MathTiers234:
            if (screen == be::ScreenId::PostRound) {
                advance_cursor(t);
            }
            break;
        case AdvanceKind::Next:
        case AdvanceKind::MathStage1:
        case AdvanceKind::MathStage2:
        case AdvanceKind::FinishTutorial:
            break;  // advanced by a control / keypress / the click handler
    }
}

// Perform a scripted (in-world) step's advance via keyboard (Enter/Space on the
// Primary control), mirroring what clicking the spotlit element would do.
void synth_click(const Rect& r) {
    const float cx = r.x + r.w * 0.5f;
    const float cy = r.y + r.h * 0.5f;
    // Mouse + key dispatch use separate handler vectors, so this nested dispatch from
    // inside a key handler is re-entrancy-safe (event_router snapshots each vector).
    be::dispatch_mouse_event(be::MouseEvent{be::MouseEventType::MouseDown, cx, cy, 0, 0.0f});
    be::dispatch_mouse_event(be::MouseEvent{be::MouseEventType::MouseUp, cx, cy, 0, 0.0f});
}

void perform_scripted_advance(const SubStep& s) {
    TutorialRuntime* t = g_runtime;
    const Canvas c = viewport_canvas();
    switch (s.advance) {
        case AdvanceKind::ModalExplore:
            if (s.spot == SpotTarget::RootSettings) {
                modal::open_settings_modal();
            } else if (s.spot == SpotTarget::RootShop) {
                modal::open_shop_modal();
            } else if (s.spot == SpotTarget::RootHelp) {
                modal::open_help_modal();
            }
            break;
        case AdvanceKind::LeaderboardExplore:
            // Keyboard Enter/Space on the Root Shop spot REOPENS the Shop; the user then
            // reaches the Leaderboard via the in-Shop swap icon (real navigation).
            modal::open_shop_modal();
            break;
        case AdvanceKind::CustomExplore:
        case AdvanceKind::PlayMorph:
            // Re-use Z07's router click by synthesizing one at the spotlit element's
            // center (the tutorial mouse handler clicks through it, in_spot).
            if (const std::optional<Rect> r = resolve_spotlight(s.spot, c)) {
                synth_click(*r);
            }
            break;
        case AdvanceKind::LaunchCaller:
            if (t != nullptr && t->seams.launch_scenario) {
                t->seams.launch_scenario(kTutorialScenarioCallerSeed);
            }
            break;
        case AdvanceKind::LaunchAggressor:
        case AdvanceKind::FinishTutorial:
            (void)on_again_commit();  // the Post-Round Again redirect (a render-time button)
            break;
        default:
            break;
    }
}

void activate_control(TutorialRuntime& t, Ctl c) {
    switch (c) {
        case Ctl::Previous:
            step_back(t);
            break;
        case Ctl::Primary: {
            if (t.cursor >= script_len()) {
                return;
            }
            const SubStep& s = tutorial_script()[t.cursor];
            if (advances_on_next(s.advance)) {
                advance_cursor(t);  // Next
            } else {
                perform_scripted_advance(s);
            }
            break;
        }
        case Ctl::Skip:
            tutorial_skip_confirmation_open();
            break;
    }
}

// ----- Event handlers (HandlerPriority::TutorialOverlay) -----

// Math-walkthrough key contract (Group A): the tutorial is the sole authority over
// which box is live and what each key does while a math step is active.
[[nodiscard]] bool on_math_key(TutorialRuntime& t, const SubStep& s, const be::KeyEvent& e) {
    switch (s.advance) {
        case AdvanceKind::MathStage1: {
            // Stage 1: only the positional digit the callout names focuses the taught
            // box and advances; every other digit is inert (gates the other boxes).
            if (const std::optional<int> d = digit_of(e.code)) {
                const int want = positional_digit_for(s);
                if (want > 0 && *d == want) {
                    if (const std::optional<be::FocusableId> f = math_focus_for(s)) {
                        be::activate_keyboard_mode();
                        be::snap_focus_to(*f);
                    }
                    advance_cursor(t);  // -> stage 2 (the gate lifts; the box goes live)
                }
                return true;
            }
            return true;  // swallow Tab/Enter/Space/arrows: no box is live in stage 1
        }
        case AdvanceKind::MathStage2:
            // Stage 2: the taught box is live. Tab advances to the next box's stage 1
            // (or the submit step). Enter/Space are swallowed (no submit mid-walk).
            // Everything else falls through: a text box captures the digits itself, and
            // the bet-size group's routed 1-4 reach Z09's tier select.
            if (e.code == be::KeyCode::Tab) {
                advance_cursor(t);
                return true;
            }
            if (e.code == be::KeyCode::Enter || e.code == be::KeyCode::Space) {
                return true;
            }
            return false;
        case AdvanceKind::MathSubmit:
            // "Press Enter to submit": Z09 owns Enter (suppress-until-filled, then submit
            // -> Post-Round). Swallow everything else to keep the trap.
            if (e.code == be::KeyCode::Enter) {
                return false;
            }
            return true;
        case AdvanceKind::MathTiers234:
            return false;  // free play: Z09 owns Enter (advance tiers) / digits / Tab
        default:
            return false;
    }
}

bool on_tutorial_mouse(const be::MouseEvent& e) {
    TutorialRuntime* t = g_runtime;
    if (t == nullptr || e.type != be::MouseEventType::MouseDown || e.button != 0) {
        return false;
    }
    const be::TutorialPhase ph = phase();
    const Canvas c = viewport_canvas();

    if (ph == be::TutorialPhase::Prompt) {
        const Rect panel = prompt_panel_rect(c);
        if (pt(e, prompt_start_rect(panel))) {
            tutorial_start();
            return true;
        }
        if (pt(e, prompt_skip_rect(panel)) || !pt(e, panel)) {
            dismiss_prompt(*t);  // Skip or click-outside -> dismiss = Skip
            return true;
        }
        return true;  // swallow clicks inside the panel
    }

    if (ph != be::TutorialPhase::Active) {
        return false;
    }
    if (be::is_any_modal_open()) {
        return false;  // free-exploration / skip-confirm: the modal owns input
    }
    // The Skip button is always live, at full brightness, never spotlit.
    if (pt(e, skip_button_rect(c))) {
        tutorial_skip_confirmation_open();
        return true;
    }
    if (t->cursor >= script_len()) {
        return false;
    }
    const SubStep& s = tutorial_script()[t->cursor];

    // Math walkthrough: keyboard-only. Swallow ALL table / box / cluster clicks so no
    // other box can be focused -- except the tiers-2-4 free-play step.
    if (is_math_step(s.advance)) {
        return s.advance != AdvanceKind::MathTiers234;  // true = swallow; tiers 2-4 = Z09 owns
    }

    // Tour / action steps: the callout's own Next / Previous buttons take clicks first.
    if (advances_on_next(s.advance)) {
        const Rect panel = callout_panel_rect(c, resolve_spotlight(s.spot, c));
        if (pt(e, next_button_rect(panel))) {
            tutorial_advance();
            return true;
        }
        if (previous_available(*t) && pt(e, prev_button_rect(panel))) {
            step_back(*t);
            return true;
        }
    }

    const std::optional<Rect> spot = resolve_spotlight(s.spot, c);
    const bool in_spot = spot.has_value() && pt(e, *spot);
    switch (s.advance) {
        case AdvanceKind::Next:
            // Informational: swallow, EXCEPT in-spot on a Post-Round recap step, where the
            // spotlit stat box's tier tabs must stay clickable (Z13 owns the tab click).
            return !(s.screen == StepScreen::PostRound && in_spot);
        case AdvanceKind::ModalExplore:
        case AdvanceKind::CustomExplore:
        case AdvanceKind::PlayMorph:
            return in_spot ? false : true;  // click-through the spotlit element; else swallow
        case AdvanceKind::LeaderboardExplore:
            // Click-through the Root Shop spot to reopen the Shop; the user then clicks
            // the in-Shop Leaderboard swap icon (handled by the Shop's own click path,
            // reached because on_tutorial_mouse yields once a modal is open).
            return in_spot ? false : true;
        case AdvanceKind::LaunchCaller:
            // STANDARD is a router click (Z07): intercept it and launch the Caller
            // teaching seed instead of a random STANDARD draw.
            if (in_spot && t->seams.launch_scenario) {
                t->seams.launch_scenario(kTutorialScenarioCallerSeed);
            }
            return true;
        case AdvanceKind::LaunchAggressor:
        case AdvanceKind::FinishTutorial:
            // The Again button is a RENDER-TIME click in Z13 (ImGui IsMouseClicked), not
            // a router click, so it can't be intercepted here. Let Z13 arm + commit it;
            // the commit is redirected to the teaching seed / Tutorial Complete by
            // on_again_commit (wired into Z13's commit_again override).
            return in_spot ? false : true;
        case AdvanceKind::MathStage1:
        case AdvanceKind::MathStage2:
        case AdvanceKind::MathSubmit:
        case AdvanceKind::MathTiers234:
            return true;  // handled above; never reached
    }
    return false;
}

bool on_tutorial_key(const be::KeyEvent& e) {
    TutorialRuntime* t = g_runtime;
    if (t == nullptr || e.type != be::KeyEventType::KeyDown) {
        return false;
    }
    const be::TutorialPhase ph = phase();

    if (ph == be::TutorialPhase::Prompt) {
        if (e.code == be::KeyCode::Escape) {
            dismiss_prompt(*t);
            return true;
        }
        // The prompt is a two-button focus trap (Start Tutorial = 0, Skip = 1). Tab /
        // Shift-Tab toggle between them; Enter / Space activate the focused one. Without
        // this the prompt was mouse-only.
        if (e.code == be::KeyCode::Tab) {
            be::activate_keyboard_mode();
            t->control_index = t->control_index == 0 ? 1 : 0;
            return true;
        }
        if (e.code == be::KeyCode::Enter || e.code == be::KeyCode::Space) {
            if (t->control_index == 1) {
                dismiss_prompt(*t);  // Skip
            } else {
                tutorial_start();    // Start Tutorial
            }
            return true;
        }
        return true;  // swallow other keys while the prompt is up
    }

    if (ph != be::TutorialPhase::Active) {
        return false;
    }
    if (e.code == be::KeyCode::Escape) {
        if (!be::is_any_modal_open()) {
            tutorial_skip_confirmation_open();
            return true;
        }
        return false;  // topmost modal handles its own escape
    }
    if (be::is_any_modal_open()) {
        return false;
    }
    if (t->cursor >= script_len()) {
        return false;
    }
    const SubStep& s = tutorial_script()[t->cursor];

    // Math walkthrough owns its own key contract.
    if (is_math_step(s.advance)) {
        return on_math_key(*t, s, e);
    }

    // Tour / action steps: Tab cycles the trapped controls; Enter AND Space both
    // activate the focused control; every other key is swallowed so nothing leaks to
    // the underlying screen (the cross-step focus trap, Group A).
    if (e.code == be::KeyCode::Tab) {
        cycle_control(*t, be::has_mod(e.mods, be::ModMask::Shift));
        return true;
    }
    if (e.code == be::KeyCode::Enter || e.code == be::KeyCode::Space) {
        activate_control(*t, focused_control(*t));
        return true;
    }
    return true;  // trap: swallow digits / arrows / letters
}

}  // namespace

std::optional<animations::Rect> spotlight_rect(SpotTarget target, animations::Canvas c) {
    return resolve_spotlight_impl(target, c);
}

// ----- Public API -----

TutorialRuntime* tutorial_runtime() { return g_runtime; }

bool is_tutorial_active() noexcept { return phase() == be::TutorialPhase::Active; }

bool tutorial_suppresses_text_focus() noexcept {
    const TutorialRuntime* t = g_runtime;
    if (t == nullptr || phase() != be::TutorialPhase::Active || t->cursor >= script_len()) {
        return false;
    }
    return tutorial_script()[t->cursor].advance == AdvanceKind::MathStage1;
}

be::FocusableId tutorial_live_math_box() noexcept {
    const TutorialRuntime* t = g_runtime;
    if (t == nullptr || phase() != be::TutorialPhase::Active || t->cursor >= script_len()) {
        return be::kNoFocus;
    }
    const SubStep& s = tutorial_script()[t->cursor];
    if (s.advance == AdvanceKind::MathStage1 || s.advance == AdvanceKind::MathStage2) {
        if (const std::optional<be::FocusableId> f = math_focus_for(s)) {
            return *f;
        }
    }
    return be::kNoFocus;
}

bool on_again_commit() {
    // Called from Z13's commit_again (the Again button's second/committing press) while
    // the tutorial is active. Redirects the commit: Step 5 -> the Aggressor teaching
    // seed, Step 7 -> the Tutorial Complete screen. Returns true when it handled the
    // navigation (Z13 then skips its normal replay launch).
    TutorialRuntime* t = g_runtime;
    if (t == nullptr || phase() != be::TutorialPhase::Active || t->cursor >= script_len()) {
        return false;
    }
    const SubStep& s = tutorial_script()[t->cursor];
    if (s.advance == AdvanceKind::LaunchAggressor) {
        if (t->seams.launch_scenario) {
            t->seams.launch_scenario(kTutorialScenarioAggressorSeed);
        }
        return true;
    }
    if (s.advance == AdvanceKind::FinishTutorial) {
        finish_to_complete_screen(*t);
        return true;
    }
    return false;
}

void tutorial_advance() {
    if (g_runtime == nullptr || phase() != be::TutorialPhase::Active) {
        return;
    }
    advance_cursor(*g_runtime);
}

void tutorial_restore_forced_settings() {
    TutorialRuntime* t = g_runtime;
    if (t == nullptr || !t->settings_forced || t->seams.live_settings == nullptr) {
        return;
    }
    restore_forced(*t->seams.live_settings, t->saved_settings);
    t->settings_forced = false;
}

void tutorial_start() {
    TutorialRuntime* t = g_runtime;
    if (t == nullptr || phase() == be::TutorialPhase::Active) {
        return;
    }
    // Manual re-launch (Help) clears the flag so an abandoned run re-prompts next
    // session; on first launch the flag is already false (no-op).
    if (t->seams.clear_prompt_seen) {
        t->seams.clear_prompt_seen();
    }
    // Capture + force settings (in memory only; never persisted).
    if (t->seams.live_settings != nullptr) {
        t->saved_settings = capture_forced(*t->seams.live_settings);
        apply_forced(*t->seams.live_settings);
        t->settings_forced = true;
    }
    t->cursor = 0;
    t->control_index = 0;
    t->explore_seen_open = false;
    // Start clean on Root: close any modal (e.g. the Help modal it launched from).
    if (be::is_any_modal_open()) {
        modal::close_modal();
    }
    be::set_screen(be::ScreenId::Root, std::nullopt);
    be::set_tutorial_state(be::TutorialState{be::TutorialPhase::Active, coarse_step_for_index(0)});
}

void tutorial_skip() {
    TutorialRuntime* t = g_runtime;
    if (t == nullptr) {
        return;
    }
    tutorial_restore_forced_settings();
    // Skip-with-confirm counts as done: set both the prompt-seen gate (stops the
    // first-launch prompt next session) and the completion flag, so a skip is durable
    // across tabs the same way finishing the walkthrough is (see tutorial_finish_to).
    if (t->seams.mark_completed) {
        t->seams.mark_completed();  // skip -> has_completed_tutorial = true
    }
    if (t->seams.mark_prompt_seen) {
        t->seams.mark_prompt_seen();  // skip -> has_seen_tutorial_prompt = true
    }
    t->cursor = script_len();
    be::set_tutorial_state(be::TutorialState{be::TutorialPhase::Inactive, 0});
    be::set_screen(be::ScreenId::Root, std::nullopt);
}

void tutorial_finish_to(be::ScreenId dest) {
    TutorialRuntime* t = g_runtime;
    if (t == nullptr) {
        return;
    }
    tutorial_restore_forced_settings();
    if (t->seams.mark_completed) {
        t->seams.mark_completed();
    }
    if (t->seams.mark_prompt_seen) {
        t->seams.mark_prompt_seen();
    }
    t->cursor = script_len();
    be::set_tutorial_state(be::TutorialState{be::TutorialPhase::Inactive, 0});
    be::set_screen(dest, std::nullopt);
}

void tutorial_skip_confirmation_open() {
    modal::open_confirm_modal(modal::ConfirmSpec{
        .body = "Skip tutorial? You can re-open it from the Help menu later.",
        .on_yes = [] { tutorial_skip(); },
    });
}

void install_tutorial(TutorialRuntime& runtime) {
    g_runtime = &runtime;

    const auto active_or_prompt = [] {
        const be::TutorialPhase ph = phase();
        return ph == be::TutorialPhase::Active || ph == be::TutorialPhase::Prompt;
    };
    (void)be::register_mouse_handler(active_or_prompt, on_tutorial_mouse,
                                     be::HandlerPriority::TutorialOverlay, "tutorial.mouse");
    (void)be::register_key_handler(active_or_prompt, on_tutorial_key,
                                   be::HandlerPriority::TutorialOverlay, "tutorial.key");

    screens::install_tutorial_complete_screen();
}

void render_tutorial_overlay() {
    TutorialRuntime* t = g_runtime;
    if (t == nullptr) {
        return;
    }
    if (phase() == be::TutorialPhase::Inactive) {
        maybe_trigger_prompt(*t);
    }
    const be::TutorialPhase ph = phase();
    const Canvas c = viewport_canvas();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    if (ph == be::TutorialPhase::Prompt) {
        render_prompt(dl, c, kPromptMessage,
                      /*start_focused=*/t->control_index == 0,
                      /*skip_focused=*/t->control_index == 1);
        return;
    }
    if (ph != be::TutorialPhase::Active) {
        return;
    }

    poll_advance(*t);
    if (t->cursor >= script_len()) {
        return;
    }
    if (be::is_any_modal_open()) {
        // Exception (Group E #4): the in-Shop Leaderboard step spotlights the swap icon
        // INSIDE the open Shop modal, drawn on the FOREGROUND draw list (above the modal
        // window; the background list this overlay usually uses sits behind it). Every
        // other modal-open moment hides the overlay entirely (free exploration).
        const SubStep& modal_step = tutorial_script()[t->cursor];
        if (modal_step.advance == AdvanceKind::LeaderboardExplore) {
            const std::optional<be::ModalId> mid = be::current_modal_id();
            if (mid.has_value() && *mid == modal::kShopModalId) {
                if (const std::optional<Rect> icon = modal::shop_leaderboard_icon_rect()) {
                    ImDrawList* fg = ImGui::GetForegroundDrawList();
                    render_lens(fg, c, icon);
                    render_callout(fg, callout_panel_rect(c, icon),
                                   "Click the highlighted Leaderboard icon to open the "
                                   "Leaderboard. Close it to continue.",
                                   /*show_next=*/false, /*next_focused=*/false,
                                   /*show_prev=*/false, /*prev_focused=*/false);
                }
            }
        }
        return;  // free-exploration moment: hide the spotlight/callout/skip (per spec)
    }
    const SubStep& s = tutorial_script()[t->cursor];
    if (be::read_screen_state().current != screen_id_for(s.screen)) {
        return;  // transitional frame: don't draw on the wrong screen
    }

    const std::optional<Rect> spot = resolve_spotlight(s.spot, c);
    if (s.spot != SpotTarget::None) {
        render_lens(dl, c, spot);
    }

    const Ctl fc = focused_control(*t);
    const bool has_controls = controls_for(*t).n > 0;
    const bool show_next = advances_on_next(s.advance);
    const bool show_prev = show_next && previous_available(*t);
    const std::string text = compose_callout(s);
    render_callout(dl, callout_panel_rect(c, spot), text, show_next,
                   /*next_focused=*/show_next && fc == Ctl::Primary, show_prev,
                   /*prev_focused=*/fc == Ctl::Previous);

    // The Skip button: full brightness (drawn over the lens), never spotlit; its ring
    // shows only when it is the trapped control (not during the math walkthrough).
    render_skip_button(dl, c, /*focused=*/has_controls && fc == Ctl::Skip);
}

}  // namespace poker_trainer::tutorial
