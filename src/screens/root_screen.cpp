#include "screens/root_screen.hpp"

#include "screens/render_util.hpp"

#include "assets/asset_paths.hpp"

#include "animations/button_morph.hpp"
#include "backbone/animation_clock.hpp"
#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"
#include "backbone/screen_state.hpp"

#include <imgui.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include "theme/theme_tokens.hpp"

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

}  // namespace

// The Root screen's full-canvas background rect (the blurred-background image
// slot, drawn as a bg_primary wash until the texture seam is wired).
[[nodiscard]] animations::Rect canvas_rect(const animations::Canvas& canvas) {
    return animations::Rect{0.0f, 0.0f, canvas.width, canvas.height};
}

void render_root_screen() {
    const animations::Canvas canvas = viewport_canvas();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // Blurred Root background image (background_root.png); bg_primary wash when
    // the asset is unavailable. Routes through the shared texture-bind seam.
    ru::draw_image_slot(dl, canvas_rect(canvas), assets::AssetId::BackgroundRoot,
                        ru::SlotFallback::Background, /*focused=*/false);

    // Logo (top-left) image slot.
    ru::draw_image_slot(dl, animations::logo_rect(canvas), assets::AssetId::AppLogo,
                        ru::SlotFallback::Icon, /*focused=*/false);

    // Middle 2x2 grid: Play=TL, Settings=TR, Shop=BL, Help=BR.
    ru::button(dl, animations::root_grid_button_rect(animations::MorphButton::Play, canvas), "PLAY",
               focus_on(kFocusRootPlay));
    ru::button(dl, animations::root_grid_button_rect(animations::MorphButton::Settings, canvas),
               "Settings", focus_on(kFocusRootSettings));
    ru::button(dl, animations::root_grid_button_rect(animations::MorphButton::Shop, canvas), "Shop",
               focus_on(kFocusRootShop));
    ru::button(dl, animations::root_grid_button_rect(animations::MorphButton::Help, canvas), "Help",
               focus_on(kFocusRootHelp));

    // Home icon (top-right, stationary anchor of the cluster) image slot.
    ru::draw_image_slot(dl, animations::home_icon_rect(canvas), assets::AssetId::IconHome,
                        ru::SlotFallback::Icon, focus_on(kFocusRootHome));
}

void render_root_morph_frame(float global_t) {
    const animations::Canvas canvas = viewport_canvas();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // Background + the two stationary anchors stay put through the morph.
    // SEAM(visual-pass): the synchronized background-blur crossfade goes here.
    // The Root background now renders its image through the shared seam; the
    // ~300 ms crossfade into the Mode Selection background (blending background_root
    // into background_mode using animations::MorphController::crossfade) is still a
    // deferred visual-pass step, so the morph draws the single Root background slot
    // and the live Mode Selection screen takes over background_mode at handoff.
    ru::draw_image_slot(dl, canvas_rect(canvas), assets::AssetId::BackgroundRoot,
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
    // its own staggered timeline, filled throughout. Their morph targets are the
    // exact rects the live Mode Selection cluster renders/hit-tests
    // (mode_button_target_rect), so the handoff is seamless. No focus outline
    // during the transition.
    ru::button(dl,
               animations::morph_button_rect(animations::MorphButton::Settings, global_t, canvas),
               "Settings", /*focused=*/false);
    ru::button(dl, animations::morph_button_rect(animations::MorphButton::Shop, global_t, canvas),
               "Shop", /*focused=*/false);
    ru::button(dl, animations::morph_button_rect(animations::MorphButton::Help, global_t, canvas),
               "Help", /*focused=*/false);
}

void install_root_handlers(animations::MorphController& morph) {
    // Escape on Root: consume and do nothing (Notes — Escape Key Behavior).
    backbone::register_key_handler(
        [] { return backbone::read_screen_state().current == backbone::ScreenId::Root; },
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
        [] { return backbone::read_screen_state().current == backbone::ScreenId::Root; },
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
            switch (root_activation_for_focus(backbone::get_focused_element())) {
                case RootActivation::StartMorph:
                    morph.start(backbone::total_ms_since_app_start());  // == a Play click
                    return true;
                case RootActivation::ReloadPage:
                    reload_page();  // == a Home click
                    return true;
                case RootActivation::None:
                    return false;  // focused Z11 seam element: leave the key unconsumed
            }
            return false;
        },
        backbone::HandlerPriority::ScreenContext, "root.activate");

    // Click routing. Play starts the morph (on the caller-owned controller); Home
    // reloads the page; Settings/Shop/Help open their modals (Zone 11 seam).
    backbone::register_mouse_handler(
        [] { return backbone::read_screen_state().current == backbone::ScreenId::Root; },
        [&morph](const backbone::MouseEvent& e) {
            if (e.type != backbone::MouseEventType::MouseDown || e.button != 0) {
                return false;
            }
            const animations::Canvas canvas = viewport_canvas();
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
            // Settings / Shop / Help -> Zone 11 cluster modals (seam, not this wave).
            return false;
        },
        backbone::HandlerPriority::ScreenContext, "root.click");
}

}  // namespace poker_trainer::screens
