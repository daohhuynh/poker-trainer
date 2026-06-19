#include "screens/mode_selection_screen.hpp"

#include "screens/custom_popup.hpp"
#include "screens/render_util.hpp"

#include "assets/asset_paths.hpp"

#include "animations/button_morph.hpp"
#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"
#include "backbone/modal_state.hpp"
#include "backbone/screen_state.hpp"

#include <optional>

#include <imgui.h>

#include "bridge/game_launch.hpp"
#include "theme/theme_tokens.hpp"

#include "modal/modals.hpp"

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

// The persistent top-right cluster. Zone 11 owns the cluster (render + activation);
// Mode Selection hands it the morph target rects + focus ids. It keeps Zone 07's
// morph-handoff look via the MorphButton style: the Root -> Mode morph animates
// buttons into these exact rects, so Shop/Help/Settings render as buttons-with-labels
// at rest (no pop at handoff); the fourth icon is Home (a glyph at the stationary
// home_icon_rect). render_persistent_cluster caches the geometry/ids so the click
// handler (cluster_hit_test) and the Z11 cluster keyboard handler resolve a hit /
// focus to an action (open Shop/Help/Settings modal, or Home -> Root).
void render_cluster(ImDrawList* dl, const animations::Canvas& canvas) {
    const std::array<animations::Rect, 4> rects{
        animations::mode_button_target_rect(animations::MorphButton::Shop, canvas),
        animations::mode_button_target_rect(animations::MorphButton::Help, canvas),
        animations::mode_button_target_rect(animations::MorphButton::Settings, canvas),
        animations::home_icon_rect(canvas)};
    const std::array<backbone::FocusableId, 4> ids{kFocusModeShop, kFocusModeHelp,
                                                   kFocusModeSettings, kFocusModeHome};
    modal::render_persistent_cluster(
        dl, modal::ClusterContext{.screen = modal::ClusterScreen::ModeSelection,
                                  .style = modal::ClusterStyle::MorphButton,
                                  .rects = rects,
                                  .ids = ids});
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
            return !popup.open && !backbone::is_any_modal_open() &&
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
            return !popup.open && !backbone::is_any_modal_open() &&
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
            return !popup.open && !backbone::is_any_modal_open() &&
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
            // Persistent top-right cluster: Zone 11 resolves the hit from the geometry
            // it cached in render_persistent_cluster and performs the action
            // (Shop/Help/Settings open their modal; Home returns to Root).
            if (const std::optional<modal::ClusterIcon> icon =
                    modal::cluster_hit_test(e.x, e.y)) {
                modal::activate_cluster_icon(*icon);
                return true;
            }
            return false;
        },
        backbone::HandlerPriority::ScreenContext, "mode.click");
}

}  // namespace poker_trainer::screens
