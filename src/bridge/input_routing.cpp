#include "bridge/input_routing.hpp"

#include "backbone/event_router.hpp"

namespace poker_trainer::bridge {

bool router_should_see_key(bool imgui_wants_keyboard, backbone::KeyCode code) noexcept {
    if (!imgui_wants_keyboard) {
        return true;  // ImGui not capturing: the router sees every key, as today.
    }
    // ImGui owns the keyboard (a text field is active): suppress screen-level
    // dispatch for everything except the navigation/command keys the
    // focus_manager and screens own, plus the arrow keys.
    //
    // Why arrows pass through even while ImGui captures: feed_imgui_keyboard
    // (platform.cpp) feeds EVERY key into ImGui IO unconditionally, so the text
    // cursor still moves on Left/Right regardless of this gate — this decision
    // only governs the SECOND consumer, the backbone event router. The single
    // registered arrow consumer is the Custom popup's slider/input adjust handler
    // (ModalLayer, gated on popup-open); it needs Up/Down/Left/Right while an
    // InputText is active to nudge the focused slider/weight. Routing arrows does
    // not take them from ImGui (the cursor move still happens for inputs), and on
    // every other screen no handler claims arrows, so ImGui's cursor behavior is
    // unchanged there. The exemption is therefore global rather than popup-gated.
    switch (code) {
        case backbone::KeyCode::Tab:
        case backbone::KeyCode::Enter:
        case backbone::KeyCode::Escape:
        case backbone::KeyCode::ArrowUp:
        case backbone::KeyCode::ArrowDown:
        case backbone::KeyCode::ArrowLeft:
        case backbone::KeyCode::ArrowRight:
            return true;
        default:
            return false;
    }
}

}  // namespace poker_trainer::bridge
