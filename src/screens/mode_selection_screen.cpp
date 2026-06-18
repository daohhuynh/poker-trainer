#include "screens/mode_selection_screen.hpp"

#include "screens/custom_popup.hpp"
#include "screens/render_util.hpp"

#include "assets/asset_paths.hpp"

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

// The persistent top-right cluster (left -> right: Shop, Help, Settings, Home).
//
// SEAM(Z11): Zone 11 (Wave 3) owns the shared cross-screen cluster component
// (render_persistent_cluster) and the modal-open behavior of Shop/Help/Settings.
// Until it lands, Zone 07 renders the cluster locally on Mode Selection at the
// morph's exact target rects (animations::mode_button_target_rect for the three
// morphing icons, home_icon_rect for the stationary Home anchor). Rendering at
// those same rects is what makes the Root -> Mode morph hand off to a live,
// hit-testable element: the thing that animates in IS the thing that renders and
// is clicked at rest (visible == clickable). The three icons render as the morph
// produces them at progress 1 — a filled button with its label — so there is no
// pop at handoff; Home renders through the shared HomeIcon image slot.
void render_cluster(ImDrawList* dl, const animations::Canvas& canvas) {
    ru::button(dl, animations::mode_button_target_rect(animations::MorphButton::Shop, canvas),
               "Shop", focus_on(kFocusModeShop));
    ru::button(dl, animations::mode_button_target_rect(animations::MorphButton::Help, canvas),
               "Help", focus_on(kFocusModeHelp));
    ru::button(dl, animations::mode_button_target_rect(animations::MorphButton::Settings, canvas),
               "Settings", focus_on(kFocusModeSettings));
    ru::draw_image_slot(dl, animations::home_icon_rect(canvas), assets::AssetId::IconHome,
                        ru::SlotFallback::Icon, focus_on(kFocusModeHome));
}

// Open the Custom configuration popup, seeded with the last-saved split (or 50/50
// if Save has never run). Shared by the Custom button's click and its keyboard
// activation so both open it identically (no copy-paste of the open sequence).
void open_custom_popup_for(CustomPopupState& popup, const CustomWeightsStore& store) {
    popup.weights = reset_weights(store.load());
    popup.open = true;
    popup.just_opened = true;  // suppress same-frame click-outside dismiss (B1)
    static_cast<void>(open_custom_popup());  // push the modal focus trap
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

    // Blurred Mode Selection background image (background_mode.png); bg_primary
    // wash when the asset is unavailable. Routes through the shared texture-bind
    // seam, the same single AddImage point as every other image slot.
    ru::draw_image_slot(dl, animations::Rect{0.0f, 0.0f, canvas.width, canvas.height},
                        assets::AssetId::BackgroundMode, ru::SlotFallback::Background,
                        /*focused=*/false);

    // STANDARD button (top-left, the Play morph target).
    ru::button(dl, animations::standard_button_rect(canvas), "STANDARD", focus_on(kFocusStandard));

    // Centered middle row: Aggressor, Caller, Custom (left -> right).
    ru::button(dl, animations::mode_middle_button_rect(0, canvas), "Aggressor",
               focus_on(kFocusAggressorButton));
    ru::button(dl, animations::mode_middle_button_rect(1, canvas), "Caller",
               focus_on(kFocusCallerButton));
    ru::button(dl, animations::mode_middle_button_rect(2, canvas), "Custom",
               focus_on(kFocusCustomButton));

    render_cluster(dl, canvas);
}

void install_mode_selection_handlers(CustomPopupState& popup, const CustomWeightsStore& store) {
    // Escape on Mode Selection: return to Root (Notes — Escape Key Behavior).
    // While the popup is open it captures Escape at the higher ModalLayer
    // priority (install_custom_popup_handlers), so this screen handler never sees
    // it; the popup.open guard is belt-and-suspenders.
    backbone::register_key_handler(
        [&popup] {
            return !popup.open &&
                   backbone::read_screen_state().current == backbone::ScreenId::ModeSelection;
        },
        [](const backbone::KeyEvent& e) {
            if (e.type == backbone::KeyEventType::KeyDown && e.code == backbone::KeyCode::Escape) {
                on_mode_selection_escape();
                return true;
            }
            return false;
        },
        backbone::HandlerPriority::ScreenContext, "mode.escape");

    // Keyboard activation: Space (and Enter, on this non-Game screen) activates the
    // focused element, the same as clicking it — STANDARD/Aggressor/Caller launch,
    // Custom opens the popup, Home returns to Root. Cluster slots are a Z11 seam
    // (None -> unconsumed no-op). Gated identically to the click/escape handlers
    // (suppressed while the popup is open). The platform's WantCaptureKeyboard gate
    // ensures these keys only reach the router when no text field is capturing.
    backbone::register_key_handler(
        [&popup] {
            return !popup.open &&
                   backbone::read_screen_state().current == backbone::ScreenId::ModeSelection;
        },
        [&popup, &store](const backbone::KeyEvent& e) {
            if (e.type != backbone::KeyEventType::KeyDown) {
                return false;
            }
            if (e.code != backbone::KeyCode::Space && e.code != backbone::KeyCode::Enter) {
                return false;
            }
            if (!backbone::is_keyboard_mode_active()) {
                return false;  // nothing focused yet
            }
            switch (mode_activation_for_focus(backbone::get_focused_element())) {
                case ModeActivation::LaunchStandard:
                    emit_launch(launch_request_for_standard());
                    return true;
                case ModeActivation::LaunchAggressor:
                    emit_launch(launch_request_for_aggressor());
                    return true;
                case ModeActivation::LaunchCaller:
                    emit_launch(launch_request_for_caller());
                    return true;
                case ModeActivation::OpenCustomPopup:
                    open_custom_popup_for(popup, store);
                    return true;
                case ModeActivation::ReturnToRoot:
                    on_mode_selection_escape();
                    return true;
                case ModeActivation::None:
                    return false;  // Z11 cluster seam: leave the key unconsumed
            }
            return false;
        },
        backbone::HandlerPriority::ScreenContext, "mode.activate");

    // Click routing for the four launch paths + Custom popup open. Suppressed
    // while the popup is open so clicks on the modal never reach the screen
    // beneath it (Zone 11's modal infra refines this arbitration later).
    backbone::register_mouse_handler(
        [&popup] {
            return !popup.open &&
                   backbone::read_screen_state().current == backbone::ScreenId::ModeSelection;
        },
        [&popup, &store](const backbone::MouseEvent& e) {
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
                open_custom_popup_for(popup, store);
                return true;
            }
            // Persistent top-right cluster. Hit-tested at the exact rects
            // render_cluster draws (mode_button_target_rect / home_icon_rect), so
            // visible == clickable.
            if (point_in_rect(e.x, e.y, animations::home_icon_rect(canvas))) {
                // Home returns to Root (Notes — Escape Key Behavior: from Mode
                // Selection, Home and Escape share the return-to-Root path).
                on_mode_selection_escape();
                return true;
            }
            for (const animations::MorphButton icon :
                 {animations::MorphButton::Shop, animations::MorphButton::Help,
                  animations::MorphButton::Settings}) {
                if (point_in_rect(e.x, e.y, animations::mode_button_target_rect(icon, canvas))) {
                    // SEAM(Z11): Shop / Help / Settings each open their centered
                    // modal. Zone 11 (Wave 3) owns the modal system; until it lands
                    // the hit is consumed as a graceful no-op (no crash, no effect).
                    return true;
                }
            }
            return false;
        },
        backbone::HandlerPriority::ScreenContext, "mode.click");
}

}  // namespace poker_trainer::screens
