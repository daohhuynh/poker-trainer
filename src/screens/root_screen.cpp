#include "screens/root_screen.hpp"

#include "screens/render_util.hpp"

#include "animations/button_morph.hpp"
#include "backbone/animation_clock.hpp"
#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"
#include "backbone/screen_state.hpp"

#include <imgui.h>

#include "theme/theme_tokens.hpp"

namespace poker_trainer::screens {

namespace {

namespace ru = render_util;

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

void render_root_screen() {
    const animations::Canvas canvas = viewport_canvas();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // Atmospheric background wash (bg_primary) over the blurred Root background.
    // The blurred background_root.png itself is drawn by the Zone 05 render layer
    // once the asset registry it owns is GPU-uploaded; see the texture seam note
    // in render_util.hpp.
    dl->AddRectFilled(ImVec2{0.0f, 0.0f}, ImVec2{canvas.width, canvas.height},
                      ru::token_u32(theme::ColorToken::BgPrimary));

    // Logo (top-left) — placeholder until the texture seam is wired.
    ru::icon_placeholder(dl, animations::logo_rect(canvas), /*focused=*/false);

    // Middle 2x2 grid: Play=TL, Settings=TR, Shop=BL, Help=BR.
    ru::button(dl, animations::root_grid_button_rect(animations::MorphButton::Play, canvas), "PLAY",
               focus_on(kFocusRootPlay));
    ru::button(dl, animations::root_grid_button_rect(animations::MorphButton::Settings, canvas),
               "Settings", focus_on(kFocusRootSettings));
    ru::button(dl, animations::root_grid_button_rect(animations::MorphButton::Shop, canvas), "Shop",
               focus_on(kFocusRootShop));
    ru::button(dl, animations::root_grid_button_rect(animations::MorphButton::Help, canvas), "Help",
               focus_on(kFocusRootHelp));

    // Home icon (top-right, stationary anchor of the cluster).
    ru::icon_placeholder(dl, animations::home_icon_rect(canvas), focus_on(kFocusRootHome));
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

    // Click routing. Play starts the morph (on the caller-owned controller);
    // Settings/Shop/Help open their modals (Zone 11 seam); Home reloads the page
    // (Zone 05 / window.location.reload seam).
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
            // Settings / Shop / Help -> Zone 11 cluster modals (seam, not this wave).
            // Home -> window.location.reload (Zone 05 bridge seam, not this wave).
            return false;
        },
        backbone::HandlerPriority::ScreenContext, "root.click");
}

}  // namespace poker_trainer::screens
