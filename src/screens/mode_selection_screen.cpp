#include "screens/mode_selection_screen.hpp"

#include "screens/custom_popup.hpp"
#include "screens/render_util.hpp"

#include "animations/button_morph.hpp"
#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"
#include "backbone/screen_state.hpp"

#include <imgui.h>

#include "bridge/game_launch.hpp"
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

void emit_launch(const LaunchRequest& request) {
    // Zone 07 emits mode + config and stops. Zone 05's request_game_launch turns
    // the mode into a concrete seed via its reject loop; Zone 07 never touches the
    // engine.
    bridge::request_game_launch(request.mode, request.config);
}

void render_mode_selection_screen() {
    const animations::Canvas canvas = viewport_canvas();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // Atmospheric background wash (bg_primary) over background_mode.png; the
    // blurred image is drawn by the Zone 05 render layer (texture seam).
    dl->AddRectFilled(ImVec2{0.0f, 0.0f}, ImVec2{canvas.width, canvas.height},
                      ru::token_u32(theme::ColorToken::BgPrimary));

    // STANDARD button (top-left, the Play morph target).
    ru::button(dl, animations::standard_button_rect(canvas), "STANDARD", focus_on(kFocusStandard));

    // Centered middle row: Aggressor, Caller, Custom (left -> right).
    ru::button(dl, animations::mode_middle_button_rect(0, canvas), "Aggressor",
               focus_on(kFocusAggressorButton));
    ru::button(dl, animations::mode_middle_button_rect(1, canvas), "Caller",
               focus_on(kFocusCallerButton));
    ru::button(dl, animations::mode_middle_button_rect(2, canvas), "Custom",
               focus_on(kFocusCustomButton));

    // The top-right persistent cluster (Shop/Help/Settings/Home) is Zone 11's;
    // Zone 07 only reserves its four focus slots (see kModeSelectionFocusOrder).
}

void install_mode_selection_handlers() {
    // Escape on Mode Selection: return to Root (Notes — Escape Key Behavior).
    backbone::register_key_handler(
        [] { return backbone::read_screen_state().current == backbone::ScreenId::ModeSelection; },
        [](const backbone::KeyEvent& e) {
            if (e.type == backbone::KeyEventType::KeyDown && e.code == backbone::KeyCode::Escape) {
                on_mode_selection_escape();
                return true;
            }
            return false;
        },
        backbone::HandlerPriority::ScreenContext, "mode.escape");

    // Click routing for the four launch paths + Custom popup open.
    backbone::register_mouse_handler(
        [] { return backbone::read_screen_state().current == backbone::ScreenId::ModeSelection; },
        [](const backbone::MouseEvent& e) {
            if (e.type != backbone::MouseEventType::MouseDown || e.button != 0) {
                return false;
            }
            const animations::Canvas canvas = viewport_canvas();
            if (point_in_rect(e.x, e.y, animations::standard_button_rect(canvas))) {
                emit_launch(launch_request_for_standard());
                return true;
            }
            if (point_in_rect(e.x, e.y, animations::mode_middle_button_rect(0, canvas))) {
                emit_launch(launch_request_for_aggressor());
                return true;
            }
            if (point_in_rect(e.x, e.y, animations::mode_middle_button_rect(1, canvas))) {
                emit_launch(launch_request_for_caller());
                return true;
            }
            if (point_in_rect(e.x, e.y, animations::mode_middle_button_rect(2, canvas))) {
                // Custom does NOT launch; it opens the configuration popup.
                static_cast<void>(open_custom_popup());
                return true;
            }
            // Cluster (Shop/Help/Settings/Home) -> Zone 11 seam, not this wave.
            return false;
        },
        backbone::HandlerPriority::ScreenContext, "mode.click");
}

}  // namespace poker_trainer::screens
