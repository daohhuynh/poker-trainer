#include "screens/post_round_screen.hpp"

#include "screens/clipboard_fallback_modal.hpp"
#include "screens/render_util.hpp"

#include "render/front_dealer.hpp"
#include "render/stat_modal.hpp"

#include "animations/button_morph.hpp"

#include "backbone/animation_clock.hpp"
#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"
#include "backbone/game_mode.hpp"
#include "backbone/modal_state.hpp"
#include "backbone/scenario_events.hpp"
#include "backbone/screen_state.hpp"

#include "engine/scenario.hpp"
#include "engine/scenario_id.hpp"

#include "settings/settings.hpp"
#include "theme/theme_tokens.hpp"

#include "math/interrogator.hpp"

#include "modal/modals.hpp"

#include "temporal/delta_timer.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>

#include <imgui.h>

#include "bridge/game_launch.hpp"
#include "bridge/screen_dispatch.hpp"

namespace poker_trainer::screens {

// Z08's easter-egg-active query (Butler vs Frog). Declared here rather than
// including the whole Game-screen render header — the symbol lives in the `game`
// library, which the Post-Round library links for exactly this query.
[[nodiscard]] bool easter_egg_frog_active() noexcept;

namespace {

namespace anim = poker_trainer::animations;

// Focus ids for the full Post-Round list. Z13 owns the WHOLE list (head + the
// persistent-cluster tail) — unlike the Game screen, where Z09 re-registers the
// math head per tier and the cluster tail stays deferred to Z11. The cluster icons
// are focus stops now (Tab reaches them, the ring shows, and the list wraps from
// Home back to the first item); their Enter-activation stays a Z11 seam (inert).
constexpr backbone::FocusableId kFocusTierStrip =
    backbone::make_focusable_id("post_round.tier_strip");
constexpr backbone::FocusableId kFocusAgain =
    backbone::make_focusable_id("post_round.again");
constexpr backbone::FocusableId kFocusExit =
    backbone::make_focusable_id("post_round.exit");
constexpr backbone::FocusableId kFocusCopy =
    backbone::make_focusable_id("post_round.copy");
constexpr backbone::FocusableId kFocusShare =
    backbone::make_focusable_id("post_round.share");

// The persistent-cluster tail (Shop / Help / Settings / Home), aligned 1:1 with the
// icons drawn in draw_cluster.
constexpr backbone::FocusableId kFocusShop =
    backbone::make_focusable_id("post_round.shop");
constexpr backbone::FocusableId kFocusHelp =
    backbone::make_focusable_id("post_round.help");
constexpr backbone::FocusableId kFocusSettings =
    backbone::make_focusable_id("post_round.settings");
constexpr backbone::FocusableId kFocusHome =
    backbone::make_focusable_id("post_round.home");
constexpr std::array<backbone::FocusableId, 4> kClusterFocusIds = {
    kFocusShop, kFocusHelp, kFocusSettings, kFocusHome};

// True when `id` is one of the persistent-cluster icons (Enter on these is inert —
// the Z11 activation seam — but it is consumed so it never falls through).
[[nodiscard]] bool is_cluster_focus(backbone::FocusableId id) noexcept {
    for (const backbone::FocusableId cid : kClusterFocusIds) {
        if (cid == id) {
            return true;
        }
    }
    return false;
}

// Dealer-arrival fade-in durations (ARCHITECTURE: dealer ~600ms, then modal
// ~600ms after the dealer). Phase 1 (the screen slide-in) is a Z14 seam.
constexpr float kDealerFadeMs = 600.0f;
constexpr float kModalFadeMs = 600.0f;
constexpr std::uint64_t kCopyFlashMs = 1000;  // "Copied" flash window

[[nodiscard]] ImU32 token_u32(theme::ColorToken token) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(token));
}

[[nodiscard]] float clamp01(float v) noexcept { return std::clamp(v, 0.0f, 1.0f); }

[[nodiscard]] bool point_in(const ImVec2& p, const anim::Rect& r) noexcept {
    return p.x >= r.x && p.x <= r.x + r.w && p.y >= r.y && p.y <= r.y + r.h;
}

[[nodiscard]] bool focused_is(backbone::FocusableId id) noexcept {
    return backbone::is_keyboard_mode_active() && backbone::get_focused_element() == id;
}

// The default landing tab from Recap Settings (Tier 1 or Summary), multi-tier only.
[[nodiscard]] render::RecapTab default_tab(const settings::Settings& s) noexcept {
    return s.recap.default_aggressor_recap_tab == settings::DefaultAggressorRecapTab::Summary
               ? render::RecapTab::Summary
               : render::RecapTab::Tier1;
}

[[nodiscard]] settings::Settings settings_or_default(const PostRoundRuntime& runtime) {
    return runtime.settings_source ? runtime.settings_source() : settings::Settings{};
}

// ----- Navigation (bus event + the existing navigation entry points) -----

void commit_again(PostRoundRuntime& runtime) {
    const engine::ScenarioId prev = runtime.snap.scenario.id;
    backbone::fire_again_pressed(backbone::AgainPressedEvent{prev});
    runtime.snap.valid = false;
    runtime.focus_registered = false;
    // Replay with the SAME launch config the user started the session with: a
    // STANDARD launch stays a STANDARD mix, an Aggressor stays Aggressor, a Custom
    // keeps its split weights. The launch mode is unrecoverable from the finished
    // scenario (STANDARD produces a Caller-or-Aggressor hand), so the bridge persists
    // it at launch and Again replays through it. The relaunch generates a fresh
    // scenario (ScenarioSpawned) and sets the screen back to Game. SEAM(Z14): the
    // Post-Round -> Game slide-out animation.
    const std::optional<bridge::LaunchConfig> cfg = bridge::last_launch_config();
    if (cfg.has_value()) {
        bridge::request_game_launch(cfg->mode, cfg->custom);
    } else {
        // Defensive: no launch recorded (cannot happen on a real Again, which is
        // only reachable via a prior launch). Fall back to STANDARD so the button is
        // never inert.
        bridge::request_game_launch(backbone::GameMode::Standard, std::nullopt);
    }
}

void do_exit(PostRoundRuntime& runtime) {
    backbone::fire_exit_to_mode_selection(
        backbone::ExitToModeSelectionEvent{runtime.snap.scenario.id});
    runtime.snap.valid = false;
    runtime.focus_registered = false;
    // SEAM(Z14): the ceremonial fade-to-black; Z13 performs only the state change.
    backbone::set_screen(backbone::ScreenId::ModeSelection, std::nullopt);
}

void do_copy(PostRoundRuntime& runtime) {
    const std::string id = engine::format_scenario_id(runtime.snap.scenario.id);
    if (platform_copy_text(id)) {
        runtime.copy_flash_ms = backbone::total_ms_since_app_start();
        runtime.copy_flash_active = true;
    } else {
        open_clipboard_fallback(runtime.clip, id);
    }
}

void do_share(PostRoundRuntime& runtime) {
    const std::string url = "/?scenario=" + engine::format_scenario_id(runtime.snap.scenario.id);
    if (platform_web_share(url)) {
        return;  // the native share sheet handles confirmation
    }
    // Unsupported -> copy the URL with the same feedback as Copy; both failing
    // opens the fallback mini-modal.
    if (platform_copy_text(url)) {
        runtime.copy_flash_ms = backbone::total_ms_since_app_start();
        runtime.copy_flash_active = true;
    } else {
        open_clipboard_fallback(runtime.clip, url);
    }
}

void apply_again_press(PostRoundRuntime& runtime) {
    if (press_again(runtime.again) == AgainPressOutcome::Committed) {
        commit_again(runtime);
    }
}

// ----- Capture + entry -----

void capture_and_enter(PostRoundRuntime& runtime, interrogator::InterrogatorRuntime& interrogator,
                       const backbone::GradingCompleteEvent& ev) {
    const engine::ScenarioState* scenario = bridge::active_scenario();
    if (scenario == nullptr || !interrogator.state.last_result.has_value()) {
        return;  // nothing to recap (should not happen on a real submit)
    }
    PostRoundSnapshot& snap = runtime.snap;
    snap.scenario = *scenario;
    snap.result = *interrogator.state.last_result;
    snap.pass = ev.passed;
    snap.elapsed_ms = ev.elapsed_ms;
    snap.frog_active = easter_egg_frog_active();
    snap.arrival_start_ms = backbone::total_ms_since_app_start();
    snap.valid = true;

    reset_again(runtime.again);
    runtime.copy_flash_active = false;
    runtime.focus_registered = false;

    const settings::Settings s = settings_or_default(runtime);
    runtime.active_tab =
        render::has_tier_tabs(snap.scenario) ? default_tab(s) : render::RecapTab::Tier1;

    // SEAM(Z14): the Game -> Post-Round 350ms slide-in. Zone 09 left the screen
    // transition unwired (its do_submit only fires the bus events), so Z13 drives
    // the state change off GradingComplete; the slide itself is Z14's.
    backbone::set_screen(backbone::ScreenId::PostRound, snap.scenario.id);
}

// Register the head focus list once per entry, while PostRound is the current
// screen (mirrors Z07's register-on-entry pattern).
void ensure_focus_registered(PostRoundRuntime& runtime) {
    if (runtime.focus_registered) {
        return;
    }
    runtime.focus_head.clear();
    if (render::has_tier_tabs(runtime.snap.scenario)) {
        runtime.focus_head.push_back(kFocusTierStrip);
    }
    runtime.focus_head.push_back(kFocusAgain);
    runtime.focus_head.push_back(kFocusExit);
    runtime.focus_head.push_back(kFocusCopy);
    runtime.focus_head.push_back(kFocusShare);
    // Cluster tail: Shop -> Help -> Settings -> Home, then Tab wraps from Home back
    // to the first item (the tier strip when multi-tier, else Again).
    for (const backbone::FocusableId cid : kClusterFocusIds) {
        runtime.focus_head.push_back(cid);
    }
    backbone::register_focus_list(backbone::ScreenId::PostRound,
                                  std::span<const backbone::FocusableId>{runtime.focus_head});
    runtime.focus_registered = true;
}

// ----- Layout -----

struct PostRoundLayout {
    anim::Rect dealer;
    ImVec2 modal_tl;
    ImVec2 modal_br;
    anim::Rect again;
    anim::Rect exit;
    anim::Rect copy;
    anim::Rect share;
    ImVec2 id_text_pos;
    std::array<anim::Rect, 4> cluster;  // Shop / Help / Settings / Home
};

[[nodiscard]] PostRoundLayout compute_layout(float w, float h) {
    PostRoundLayout l{};
    const float dealer_h = h * 0.45f;
    const float dealer_w = dealer_h * 0.72f;
    const float cx = w * 0.5f;
    const float dealer_top = h * 0.06f;
    l.dealer = anim::Rect{cx - dealer_w * 0.5f, dealer_top, dealer_w, dealer_h};

    const float mw = std::max(w * 0.5f, 360.0f);
    const float modal_top = dealer_top + dealer_h * 0.80f;  // dealer face peeks above
    const float mh = std::min(h * 0.5f, h * 0.95f - modal_top);
    l.modal_tl = ImVec2{cx - mw * 0.5f, modal_top};
    l.modal_br = ImVec2{cx + mw * 0.5f, modal_top + mh};

    const float again_w = w * 0.16f;
    const float again_h = h * 0.085f;
    l.again = anim::Rect{w - w * 0.03f - again_w, h - h * 0.04f - again_h, again_w, again_h};

    const float door = h * 0.07f;
    l.exit = anim::Rect{w * 0.03f, h - h * 0.04f - door, door, door};

    l.id_text_pos = ImVec2{w * 0.03f, h * 0.04f};
    const float util = h * 0.05f;
    const float util_x = w * 0.03f;
    l.copy = anim::Rect{util_x, h * 0.04f + ImGui::GetTextLineHeight() * 1.4f, util, util};
    l.share = anim::Rect{util_x, l.copy.y + util + 6.0f, util, util};

    const float box = w * 0.024f;
    const float gap = box * 0.35f;
    const float cluster_y = h * 0.04f;
    const float cluster_x0 = w - w * 0.03f - 4.0f * box - 3.0f * gap;
    for (std::size_t i = 0; i < 4; ++i) {
        l.cluster[i] =
            anim::Rect{cluster_x0 + static_cast<float>(i) * (box + gap), cluster_y, box, box};
    }
    return l;
}

// ----- Render pieces -----

void draw_again(ImDrawList* dl, const anim::Rect& r, AgainState state, bool focused) {
    const theme::ColorToken fill =
        state == AgainState::Armed ? theme::ColorToken::AgainButtonArmed : theme::ColorToken::ButtonBg;
    dl->AddRectFilled(ImVec2{r.x, r.y}, ImVec2{r.x + r.w, r.y + r.h}, token_u32(fill), 6.0f);
    const char* label = again_label(state);
    const ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2{r.x + (r.w - ts.x) * 0.5f, r.y + (r.h - ts.y) * 0.5f},
                token_u32(theme::ColorToken::TextButton), label);
    if (focused) {
        render_util::focus_outline(dl, r);
    }
}

void draw_id_block(ImDrawList* dl, const PostRoundRuntime& runtime, const PostRoundLayout& l) {
    const std::string id_text =
        "Scenario ID: " + engine::format_scenario_id(runtime.snap.scenario.id);
    dl->AddText(l.id_text_pos, token_u32(theme::ColorToken::TextSecondary), id_text.c_str());
    render_util::draw_image_slot(dl, l.copy, assets::AssetId::IconCopy,
                                 render_util::SlotFallback::Icon, focused_is(kFocusCopy));
    render_util::draw_image_slot(dl, l.share, assets::AssetId::IconShare,
                                 render_util::SlotFallback::Icon, focused_is(kFocusShare));
    if (runtime.copy_flash_active) {
        dl->AddText(ImVec2{l.copy.x + l.copy.w + 8.0f, l.copy.y},
                    token_u32(theme::ColorToken::StatePass), "Copied");
    }
}

// Zone 11 owns the persistent cluster (render + activation). Post-Round hands it the
// icon geometry (l.cluster) + focus ids; the fourth icon is Home. render_persistent_
// cluster draws the icon glyphs + focus rings + the offline indicator and caches the
// geometry so the mouse hit-test (handle_mouse) and the Z11 cluster keyboard handler
// can resolve a hit / focus to an action.
void draw_cluster(ImDrawList* dl, const PostRoundLayout& l) {
    modal::render_persistent_cluster(
        dl, modal::ClusterContext{.screen = modal::ClusterScreen::PostRound,
                                  .style = modal::ClusterStyle::IconGlyph,
                                  .rects = l.cluster,
                                  .ids = kClusterFocusIds});
}

// ----- Mouse (inline, like Z08's dealer-click handling) -----

void handle_mouse(PostRoundRuntime& runtime, const PostRoundLayout& l, bool tabbed) {
    if (backbone::is_any_modal_open() || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        return;
    }
    const ImVec2 m = ImGui::GetIO().MousePos;

    if (tabbed) {
        const render::StripGeom g = render::tab_strip_geom(
            l.modal_tl.x, l.modal_tl.y, l.modal_br.x - l.modal_tl.x, ImGui::GetTextLineHeight());
        const int tab = render::tab_index_at(g, m.x, m.y);
        if (tab >= 0) {
            runtime.active_tab = static_cast<render::RecapTab>(tab);
            backbone::snap_focus_to(kFocusTierStrip);
            return;
        }
    }
    // Persistent cluster (Shop / Help / Settings / Home): Zone 11 resolves the hit
    // (from the geometry it cached in render_persistent_cluster) and performs the
    // action (open modal, or Home -> Root).
    if (const std::optional<modal::ClusterIcon> icon = modal::cluster_hit_test(m.x, m.y)) {
        modal::activate_cluster_icon(*icon);
        return;
    }
    if (point_in(m, l.again)) {
        backbone::snap_focus_to(kFocusAgain);
        apply_again_press(runtime);
    } else if (point_in(m, l.exit)) {
        backbone::snap_focus_to(kFocusExit);
        do_exit(runtime);
    }
    // Copy/Share are intentionally NOT handled here: this rAF path runs outside the
    // DOM user gesture, so the browser blocks navigator.clipboard / navigator.share.
    // They are handled in-gesture by on_copy_share_mouse_up (MouseUp handler).
}

// In-gesture Copy/Share: a backbone MouseUp handler runs synchronously inside the
// DOM gesture callstack (platform.cpp on_mouse_up -> dispatch_mouse_event), which is
// what the browser requires for navigator.clipboard.writeText / navigator.share.
// MouseUp (the canonical activation-granting half of a click) is used rather than
// MouseDown so the gesture is valid across browsers. Hit-tests the rects the render
// pass cached. Enter on these buttons already runs in-gesture via the key handler
// dispatched from the DOM keydown. Returns true only when it acts on a Copy/Share
// hit, so other clicks pass through untouched.
bool on_copy_share_mouse_up(PostRoundRuntime& runtime, const backbone::MouseEvent& e) {
    if (e.type != backbone::MouseEventType::MouseUp || e.button != 0) {
        return false;
    }
    const ImVec2 m{e.x, e.y};
    if (point_in(m, runtime.copy_rect)) {
        backbone::snap_focus_to(kFocusCopy);
        do_copy(runtime);
        return true;
    }
    if (point_in(m, runtime.share_rect)) {
        backbone::snap_focus_to(kFocusShare);
        do_share(runtime);
        return true;
    }
    return false;
}

}  // namespace

void render_post_round_screen(PostRoundRuntime& runtime) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float w = vp->Size.x;
    const float h = vp->Size.y;

    if (!runtime.snap.valid) {
        dl->AddRectFilled(ImVec2{0.0f, 0.0f}, ImVec2{w, h}, token_u32(theme::ColorToken::BgPrimary));
        return;
    }

    ensure_focus_registered(runtime);

    const settings::Settings settings = settings_or_default(runtime);
    const PostRoundLayout l = compute_layout(w, h);

    // Cache the Copy/Share hit rects so the in-gesture MouseDown handler (A4) can
    // hit-test them; the layout is a pure function of the viewport, so the cached
    // rects match what this frame draws.
    runtime.copy_rect = l.copy;
    runtime.share_rect = l.share;

    // Fade-in: dealer ~600ms, then modal ~600ms after. Honor the Recap toggle
    // (off -> everything appears at once).
    float dealer_alpha = 1.0f;
    float modal_alpha = 1.0f;
    if (settings.recap.dealer_arrival_animation) {
        const std::uint64_t now = backbone::total_ms_since_app_start();
        const float elapsed = static_cast<float>(now - runtime.snap.arrival_start_ms);
        dealer_alpha = clamp01(elapsed / kDealerFadeMs);
        modal_alpha = clamp01((elapsed - kDealerFadeMs) / kModalFadeMs);
    }

    // Background (background_mode, full opacity — Phase 1 slide is a Z14 seam).
    render_util::draw_image_slot(dl, anim::Rect{0.0f, 0.0f, w, h}, assets::AssetId::BackgroundMode,
                                 render_util::SlotFallback::Background, false);

    // Front-facing dealer.
    ImVec2 dealer_tl{l.dealer.x, l.dealer.y};
    ImVec2 dealer_br{l.dealer.x + l.dealer.w, l.dealer.y + l.dealer.h};
    render::FrontDealerRender dealer{};
    dealer.top_left = &dealer_tl;
    dealer.bottom_right = &dealer_br;
    dealer.pass = runtime.snap.pass;
    dealer.frog_active = runtime.snap.frog_active;
    dealer.dealer_alpha = dealer_alpha;
    dealer.overlay_alpha = 1.0f;  // placeholders load immediately; ~200ms late-fade is approximate
    render::draw_front_dealer(dl, dealer);

    // Stat modal.
    const bool tabbed = render::has_tier_tabs(runtime.snap.scenario);
    ImVec2 modal_tl = l.modal_tl;
    ImVec2 modal_br = l.modal_br;
    render::StatModalRender modal{};
    modal.top_left = &modal_tl;
    modal.bottom_right = &modal_br;
    modal.alpha = modal_alpha;
    modal.strip_focused = tabbed && focused_is(kFocusTierStrip);
    // Z10 (Temporal): real Target from the live (frozen-at-submit) timer; Actual from
    // the GradingComplete snapshot (elapsed at submit). Both floored ms->s for display
    // to match the existing Actual read; the pass/overtime verdict stays ms-exact in
    // Z10's time_within_target(), so the row's coloring may read equal at a sub-second
    // boundary while the dealer still grades it correctly.
    const render::TimeGrade tg{static_cast<int>(temporal::target_time_ms() / 1000u),
                               static_cast<int>(runtime.snap.elapsed_ms / 1000u)};
    render::render_stat_modal(dl, runtime.snap.scenario, runtime.snap.result, runtime.active_tab,
                              tg, modal);

    // Foreground UI chrome.
    draw_id_block(dl, runtime, l);
    render_util::draw_image_slot(dl, l.exit, assets::AssetId::IconExit,
                                 render_util::SlotFallback::Icon, focused_is(kFocusExit));
    draw_again(dl, l.again, runtime.again.state, focused_is(kFocusAgain));
    draw_cluster(dl, l);

    // Expire the "Copied" flash.
    if (runtime.copy_flash_active &&
        backbone::total_ms_since_app_start() - runtime.copy_flash_ms >= kCopyFlashMs) {
        runtime.copy_flash_active = false;
    }

    handle_mouse(runtime, l, tabbed);

    // The Z13-internal clipboard fallback mini-modal (on top, when open).
    render_clipboard_fallback(runtime.clip);
}

namespace {

// ----- Keyboard (ScreenContext priority, gated to PostRound with no modal) -----

[[nodiscard]] bool post_round_input_active() {
    return backbone::read_screen_state().current == backbone::ScreenId::PostRound &&
           !backbone::is_any_modal_open();
}

// Space OR Enter activates the focused element, interchangeably per press — the
// app-wide convention (Root / Mode Selection use the same Space||Enter key-down
// gate). Both keys are always live and alternable: Space arms Again -> "CONFIRM",
// then Enter on the focused CONFIRM commits, or vice versa. The Game screen is the
// one exception (Space-only there; Enter is its math submit / tier-advance) — that
// override lives in Z09, not here. Both keys fire in the keydown gesture, so
// Copy/Share's clipboard / Web-Share calls run under transient activation (not
// blocked) on either key, exactly as the mouse path runs under the MouseUp gesture.
bool on_activate_key(PostRoundRuntime& runtime, const backbone::KeyEvent& e) {
    if (e.type != backbone::KeyEventType::KeyDown ||
        (e.code != backbone::KeyCode::Space && e.code != backbone::KeyCode::Enter)) {
        return false;
    }
    const backbone::FocusableId focused = backbone::get_focused_element();
    if (focused == kFocusTierStrip) {
        return true;  // a no-op on the focused strip (arrows / 1-5 select the tab)
    }
    if (focused == kFocusAgain) {
        apply_again_press(runtime);
        return true;
    }
    if (focused == kFocusExit) {
        do_exit(runtime);
        return true;
    }
    if (focused == kFocusCopy) {
        do_copy(runtime);
        return true;
    }
    if (focused == kFocusShare) {
        do_share(runtime);
        return true;
    }
    if (is_cluster_focus(focused)) {
        // Zone 11 owns cluster activation: leave Space/Enter unconsumed so the Z11
        // cluster keyboard handler (ScreenContext, registered after this one) opens
        // the modal / returns Home.
        return false;
    }
    return false;  // unarmed / nothing focused: leave the key unconsumed
}

bool on_tab_nav_key(PostRoundRuntime& runtime, const backbone::KeyEvent& e) {
    if (e.type != backbone::KeyEventType::KeyDown ||
        backbone::get_focused_element() != kFocusTierStrip) {
        return false;
    }
    if (!render::has_tier_tabs(runtime.snap.scenario)) {
        return false;
    }
    int idx = static_cast<int>(runtime.active_tab);
    const int count = static_cast<int>(render::kRecapTabCount);
    if (e.code == backbone::KeyCode::ArrowLeft || e.code == backbone::KeyCode::ArrowUp) {
        idx = std::max(0, idx - 1);  // clamp: left/up of first stays on first (no wrap)
    } else if (e.code == backbone::KeyCode::ArrowRight || e.code == backbone::KeyCode::ArrowDown) {
        idx = std::min(count - 1, idx + 1);  // clamp: right/down of last stays on last (no wrap)
    } else {
        // Keys 1-5 select a tab directly (1 = Tier 1 ... 5 = Summary).
        const int digit = static_cast<int>(e.code) - static_cast<int>(backbone::KeyCode::Digit1);
        if (e.mods != backbone::ModMask::None || digit < 0 || digit >= count) {
            return false;
        }
        idx = digit;
    }
    runtime.active_tab = static_cast<render::RecapTab>(idx);
    return true;
}

// Post-Round Escape -> Root immediately, no confirmation (ARCHITECTURE Notes —
// Escape Key Behavior: more aggressive than the Game screen because no scenario is
// in progress). Gated by post_round_input_active to no-modal; when a modal is open
// the ModalLayer Escape handler (modal_base) consumes it first and closes the modal.
bool on_escape_key(const backbone::KeyEvent& e) {
    if (e.type != backbone::KeyEventType::KeyDown || e.code != backbone::KeyCode::Escape) {
        return false;
    }
    backbone::set_screen(backbone::ScreenId::Root, std::nullopt);
    return true;
}

}  // namespace

void install_post_round_screen(PostRoundRuntime& runtime,
                               interrogator::InterrogatorRuntime& interrogator) {
    bridge::register_screen_renderer(backbone::ScreenId::PostRound,
                                     [&runtime] { render_post_round_screen(runtime); });

    (void)backbone::subscribe_grading_complete(
        [&runtime, &interrogator](const backbone::GradingCompleteEvent& ev) {
            capture_and_enter(runtime, interrogator, ev);
        },
        "post_round.grading_complete");

    backbone::register_key_handler(
        post_round_input_active,
        [&runtime](const backbone::KeyEvent& e) { return on_activate_key(runtime, e); },
        backbone::HandlerPriority::ScreenContext, "post_round.activate");
    backbone::register_key_handler(
        post_round_input_active,
        [&runtime](const backbone::KeyEvent& e) { return on_tab_nav_key(runtime, e); },
        backbone::HandlerPriority::ScreenContext, "post_round.tab_nav");
    backbone::register_key_handler(post_round_input_active, on_escape_key,
                                   backbone::HandlerPriority::ScreenContext, "post_round.escape");

    // Copy/Share via the mouse run through this in-gesture MouseUp handler (A4) so
    // the browser permits the clipboard / share calls; the rAF render path's mouse
    // handling fires too late (outside the gesture) for them.
    backbone::register_mouse_handler(
        post_round_input_active,
        [&runtime](const backbone::MouseEvent& e) { return on_copy_share_mouse_up(runtime, e); },
        backbone::HandlerPriority::ScreenContext, "post_round.copy_share_mouse");
}

}  // namespace poker_trainer::screens
