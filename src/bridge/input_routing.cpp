#include "bridge/input_routing.hpp"

#include "backbone/event_router.hpp"

namespace poker_trainer::bridge {

bool router_should_see_key(bool imgui_wants_keyboard, backbone::KeyCode code) noexcept {
    if (!imgui_wants_keyboard) {
        return true;  // ImGui not capturing: the router sees every key, as today.
    }
    // ImGui owns the keyboard (a text field is active): suppress screen-level
    // dispatch for everything except the navigation/command keys the
    // focus_manager and screens own.
    switch (code) {
        case backbone::KeyCode::Tab:
        case backbone::KeyCode::Enter:
        case backbone::KeyCode::Escape:
            return true;
        default:
            return false;
    }
}

}  // namespace poker_trainer::bridge
