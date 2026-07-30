#include "screens/root_screen.hpp"

#include "screens/render_util.hpp"

#include "assets/asset_paths.hpp"

#include "animations/button_morph.hpp"
#include "backbone/animation_clock.hpp"
#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"
#include "backbone/modal_state.hpp"
#include "backbone/screen_state.hpp"

#include <algorithm>

#include <imgui.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include "bridge/sfx_trigger.hpp"
#include "theme/theme_tokens.hpp"

#include "modal/modals.hpp"

namespace poker_trainer::screens {

namespace {

namespace ru = render_util;

// Home activation on Root: reload the page (ARCHITECTURE Notes — Home Screen ...:
// "clicking the Home icon reloads the page"). Routes through the same
// window.location.reload the Error screen's Retry uses (bridge/error_screen.cpp).
// Guarded to __EMSCRIPTEN__ because the screens library also compiles natively for
// the unit tests, where there is no emscripten header and no page to reload — so
// it degrades to a no-op there (the pure root_activation_for_focus mapping is what
// the native tests exercise).
void reload_page() noexcept {
#ifdef __EMSCRIPTEN__
    emscripten_run_script("window.location.reload()");
#endif
}

[[nodiscard]] animations::Canvas viewport_canvas() {
    const ImVec2 size = ImGui::GetMainViewport()->Size;
    return animations::Canvas{size.x, size.y};
}

[[nodiscard]] bool focus_on(backbone::FocusableId id) {
    return backbone::is_keyboard_mode_active() && backbone::get_focused_element() == id;
}

[[nodiscard]] bool point_in_rect(float x, float y, const animations::Rect& r) {
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

// One of the three cluster-bound buttons (Settings / Shop / Help) mid-morph: "As they
// slide, their visual representations transform from full-size buttons into small icons."
// The destination icon is drawn first and the button body over it, fading out on the
// button's own eased progress — so the button dissolves to reveal the icon it becomes, and
// at progress 1 what is on screen is exactly the glyph the live Mode Selection cluster
// draws (visible == drawn, no pop at handoff). The label crossfades with the body for the
// same reason "PLAY" crossfades into "STANDARD".
void draw_morphing_cluster_button(ImDrawList* dl, const animations::Canvas& canvas,
                                  float global_t, animations::MorphButton button,
                                  assets::AssetId icon, const char* label) {
    const animations::Rect r = animations::morph_button_rect(button, global_t, canvas);
    const float body_alpha = 1.0f - animations::button_eased_progress(global_t, button);
    // The icon art is square, so it rides the largest square centered in the (still
    // button-shaped) morph rect rather than stretching to it. The target rect IS square,
    // so at progress 1 the square equals the rect and matches the live cluster exactly.
    const float side = std::min(r.w, r.h);
    const animations::Rect icon_rect{r.x + (r.w - side) * 0.5f, r.y + (r.h - side) * 0.5f, side,
                                     side};
    ru::draw_image_slot(dl, icon_rect, icon, ru::SlotFallback::Icon, /*focused=*/false);
    ru::fill_rect(dl, r, theme::ColorToken::ButtonBg, 6.0f, body_alpha);
    ru::centered_label(dl, r, label, theme::ColorToken::TextButton, body_alpha);
}

}  // namespace

// The Root screen's full-canvas background rect (the blurred-background image
// slot, drawn as a bg_primary wash until the texture seam is wired).
[[nodiscard]] animations::Rect canvas_rect(const animations::Canvas& canvas) {
    return animations::Rect{0.0f, 0.0f, canvas.width, canvas.height};
}

void render_root_screen() {
    const animations::Canvas canvas = viewport_canvas();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // Blurred room background image (background_room.png); bg_primary wash when
    // the asset is unavailable. Routes through the shared texture-bind seam.
    ru::draw_image_slot(dl, canvas_rect(canvas), assets::AssetId::BackgroundRoom,
                        ru::SlotFallback::Background, /*focused=*/false);

    // Tier-1 ambient particle drift, behind the UI (self-gates on Reduce Motion +
    // the Particle drift toggle).
    bridge::render_ambient_particles(dl, canvas.width, canvas.height);

    // Logo (top-left) image slot.
    ru::draw_image_slot(dl, animations::logo_rect(canvas), assets::AssetId::AppLogo,
                        ru::SlotFallback::Icon, /*focused=*/false);

    // Middle 2x2 grid: Play=TL, Settings=TR, Shop=BL, Help=BR. The draw rects breathe
    // (Tier-1 ambient) and tilt on hover / keyboard focus; click + focus hit-tests use
    // the un-breathed, un-tilted grid rects.
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const animations::Rect play_r =
        animations::root_grid_button_rect(animations::MorphButton::Play, canvas);
    const animations::Rect settings_r =
        animations::root_grid_button_rect(animations::MorphButton::Settings, canvas);
    const animations::Rect shop_r =
        animations::root_grid_button_rect(animations::MorphButton::Shop, canvas);
    const animations::Rect help_r =
        animations::root_grid_button_rect(animations::MorphButton::Help, canvas);
    ru::nav_button(dl, play_r, "PLAY", kFocusRootPlay, mouse, focus_on(kFocusRootPlay));
    ru::nav_button(dl, settings_r, "Settings", kFocusRootSettings, mouse, focus_on(kFocusRootSettings));
    ru::nav_button(dl, shop_r, "Shop", kFocusRootShop, mouse, focus_on(kFocusRootShop));
    ru::nav_button(dl, help_r, "Help", kFocusRootHelp, mouse, focus_on(kFocusRootHelp));

    // Home icon (top-right, stationary anchor of the cluster): a full persistent button —
    // breath + hover/focus tilt + cursor parallax, like the grid buttons. Hit-testing still
    // uses the original home_icon_rect (visual-only motion).
    ru::nav_image_slot(dl, animations::home_icon_rect(canvas), assets::AssetId::IconHome,
                       ru::SlotFallback::Icon, kFocusRootHome, mouse, focus_on(kFocusRootHome));

    // The persistent "Training tool, no real money." disclaimer, in the same spot it occupies
    // on every other screen. Root draws no cluster, so pass the exact rects the Mode Selection
    // cluster uses (Root morphs into Mode Selection, so this is where the buttons land) — the
    // disclaimer anchors to the same row and never jumps across the Root -> Mode transition.
    modal::render_training_disclaimer(
        dl, std::array<animations::Rect, 4>{
                animations::mode_button_target_rect(animations::MorphButton::Shop, canvas),
                animations::mode_button_target_rect(animations::MorphButton::Help, canvas),
                animations::mode_button_target_rect(animations::MorphButton::Settings, canvas),
                animations::home_icon_rect(canvas)});
}

void render_root_morph_frame(float global_t) {
    const animations::Canvas canvas = viewport_canvas();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // Background + the two stationary anchors stay put through the morph.
    //
    // No background crossfade runs here, and none is deferred: Root and Mode
    // Selection draw the same BackgroundRoom asset, so the backdrop is already
    // continuous across the morph and the handoff to the live Mode Selection
    // screen is pixel-identical. The focus-pull crossfade the architecture asks
    // for is the room-to-pool-of-light step on the way into the Game screen.
    ru::draw_image_slot(dl, canvas_rect(canvas), assets::AssetId::BackgroundRoom,
                        ru::SlotFallback::Background, /*focused=*/false);
    ru::draw_image_slot(dl, animations::logo_rect(canvas), assets::AssetId::AppLogo,
                        ru::SlotFallback::Icon, /*focused=*/false);
    ru::draw_image_slot(dl, animations::home_icon_rect(canvas), assets::AssetId::IconHome,
                        ru::SlotFallback::Icon, /*focused=*/false);

    // Play travels to its STANDARD target on its own eased + staggered timeline.
    // Its filled body morphs the whole way (filled, not just an outline) and its
    // label crossfades "PLAY" -> "STANDARD" across the button's own eased progress,
    // per "As it slides, its visual label transforms from PLAY into STANDARD." At
    // progress 1 the rect equals standard_button_rect and the label reads STANDARD,
    // so the live Mode Selection screen takes over with no pop (visible == drawn).
    const animations::Rect play_rect =
        animations::morph_button_rect(animations::MorphButton::Play, global_t, canvas);
    const float play_t =
        animations::button_eased_progress(global_t, animations::MorphButton::Play);
    ru::fill_rect(dl, play_rect, theme::ColorToken::ButtonBg, 6.0f);
    ru::centered_label(dl, play_rect, "PLAY", theme::ColorToken::TextButton, 1.0f - play_t);
    ru::centered_label(dl, play_rect, "STANDARD", theme::ColorToken::TextButton, play_t);

    // Settings / Shop / Help shrink into the top-right cluster icon slots, each on
    // its own staggered timeline, dissolving from button into icon as they travel.
    // Their morph targets are the exact rects the live Mode Selection cluster
    // renders/hit-tests (mode_button_target_rect), so the handoff is seamless. No
    // focus outline during the transition.
    draw_morphing_cluster_button(dl, canvas, global_t, animations::MorphButton::Settings,
                                 assets::AssetId::IconSettings, "Settings");
    draw_morphing_cluster_button(dl, canvas, global_t, animations::MorphButton::Shop,
                                 assets::AssetId::IconShop, "Shop");
    draw_morphing_cluster_button(dl, canvas, global_t, animations::MorphButton::Help,
                                 assets::AssetId::IconHelp, "Help");
}

void install_root_handlers(animations::MorphController& morph, RootFrogState& frog) {
    // Escape on Root: consume and do nothing (Notes — Escape Key Behavior).
    backbone::register_key_handler(
        [] {
            return backbone::read_screen_state().current == backbone::ScreenId::Root &&
                   !backbone::is_any_modal_open();
        },
        [](const backbone::KeyEvent& e) {
            if (e.type == backbone::KeyEventType::KeyDown && e.code == backbone::KeyCode::Escape) {
                return on_root_escape();
            }
            return false;
        },
        backbone::HandlerPriority::ScreenContext, "root.escape");

    // Keyboard activation: Space (and Enter, on this non-Game screen) activates the
    // focused element, the same as clicking it — Play starts the morph, Home reloads
    // the page. Settings/Shop/Help are Z11 seams (None -> key left unconsumed, a
    // no-op matching their click behavior today). The platform's WantCaptureKeyboard
    // gate already ensures Space/Enter only reach the router when no text field is
    // capturing — and Root has no text fields anyway.
    backbone::register_key_handler(
        [] {
            return backbone::read_screen_state().current == backbone::ScreenId::Root &&
                   !backbone::is_any_modal_open();
        },
        [&morph](const backbone::KeyEvent& e) {
            if (e.type != backbone::KeyEventType::KeyDown) {
                return false;
            }
            if (e.code != backbone::KeyCode::Space && e.code != backbone::KeyCode::Enter) {
                return false;
            }
            if (!backbone::is_keyboard_mode_active()) {
                return false;  // nothing focused yet
            }
            const backbone::FocusableId focused = backbone::get_focused_element();
            switch (root_activation_for_focus(focused)) {
                case RootActivation::StartMorph:
                    morph.start(backbone::total_ms_since_app_start());  // == a Play click
                    return true;
                case RootActivation::ReloadPage:
                    reload_page();  // == a Home click
                    return true;
                case RootActivation::None:
                    // Settings / Shop / Help open their Zone 11 modals (== a click).
                    if (focused == kFocusRootSettings) {
                        modal::open_settings_modal();
                        return true;
                    }
                    if (focused == kFocusRootShop) {
                        modal::open_shop_modal();
                        return true;
                    }
                    if (focused == kFocusRootHelp) {
                        modal::open_help_modal();
                        return true;
                    }
                    return false;  // nothing focused
            }
            return false;
        },
        backbone::HandlerPriority::ScreenContext, "root.activate");

    // Click routing. Play starts the morph (on the caller-owned controller); Home
    // reloads the page; Settings/Shop/Help open their modals (Zone 11 seam).
    backbone::register_mouse_handler(
        [] {
            return backbone::read_screen_state().current == backbone::ScreenId::Root &&
                   !backbone::is_any_modal_open();
        },
        [&morph, &frog](const backbone::MouseEvent& e) {
            if (e.type != backbone::MouseEventType::MouseDown || e.button != 0) {
                return false;
            }
            const animations::Canvas canvas = viewport_canvas();
            // The frog (bottom-right, on a quarter of visits): ribbit + open or swap
            // its speech bubble. Hit-tested at the exact rect render_root_frog draws
            // and only while it is actually showing, so an invisible frog is never
            // clickable. That rect deliberately overhangs the canvas corner so the
            // art sits flush in it; the overhanging slice is simply unreachable,
            // and the visible frog is a strict subset of what remains, so every
            // drawn pixel stays clickable. The ribbit routes through the bridge SFX
            // seam rather than a direct Zone 03 call — Zone 07 does not depend on
            // the audio zone.
            if (frog_showing(frog) && point_in_rect(e.x, e.y, frog_rect(canvas))) {
                bridge::play_sfx(audio::SfxId::FrogToggle);
                advance_frog_bubble(frog, backbone::total_ms_since_app_start());
                return true;
            }
            if (point_in_rect(e.x, e.y,
                              animations::root_grid_button_rect(animations::MorphButton::Play,
                                                                canvas))) {
                // Debounced inside MorphController: a second click mid-morph is ignored.
                morph.start(backbone::total_ms_since_app_start());
                return true;
            }
            // Home icon (top-right, stationary anchor) -> reload the page. Hit-tested
            // at the exact rect render_root_screen draws (home_icon_rect), so
            // visible == clickable.
            if (point_in_rect(e.x, e.y, animations::home_icon_rect(canvas))) {
                reload_page();
                return true;
            }
            // Settings / Shop / Help (the 2x2 grid's TR / BL / BR) open their Zone 11
            // modals — the same modals the persistent cluster opens on the other
            // screens. Hit-tested at the exact rects render_root_screen draws.
            if (point_in_rect(e.x, e.y,
                              animations::root_grid_button_rect(animations::MorphButton::Settings,
                                                                canvas))) {
                modal::open_settings_modal();
                return true;
            }
            if (point_in_rect(e.x, e.y, animations::root_grid_button_rect(
                                            animations::MorphButton::Shop, canvas))) {
                modal::open_shop_modal();
                return true;
            }
            if (point_in_rect(e.x, e.y, animations::root_grid_button_rect(
                                            animations::MorphButton::Help, canvas))) {
                modal::open_help_modal();
                return true;
            }
            return false;
        },
        backbone::HandlerPriority::ScreenContext, "root.click");
}

}  // namespace poker_trainer::screens
