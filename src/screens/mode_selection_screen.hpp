#pragma once

// Zone 07 — Mode Selection screen (body only).
//
// Renders the STANDARD button (top-left, the morph target of Play) and the
// centered Aggressor / Caller / Custom row. The persistent top-right cluster is
// Zone 11's; Zone 07 only reserves its four slots in the focus order. The four
// launch paths are emitted here as pure LaunchRequests and forwarded to Zone 05
// via bridge::request_game_launch — Zone 07 never selects scenario types or
// touches the engine.

#include "screens/custom_popup.hpp"

#include "backbone/focus_manager.hpp"
#include "backbone/game_mode.hpp"
#include "backbone/screen_state.hpp"

#include <array>
#include <optional>

namespace poker_trainer::screens {

// GameMode / CustomConfig are the landed Phase 0 backbone contract
// (src/backbone/game_mode.hpp).
using backbone::CustomConfig;
using backbone::GameMode;

// ----- Launch paths -----------------------------------------------------------
//
// Each launch path produces a (mode, optional config) pair and nothing else.
// STANDARD/Aggressor/Caller carry no config; Custom carries the popup's weights.

struct LaunchRequest {
    GameMode mode{GameMode::Standard};
    std::optional<CustomConfig> config{};
};

[[nodiscard]] constexpr LaunchRequest launch_request_for_standard() noexcept {
    return LaunchRequest{GameMode::Standard, std::nullopt};
}
[[nodiscard]] constexpr LaunchRequest launch_request_for_aggressor() noexcept {
    return LaunchRequest{GameMode::Aggressor, std::nullopt};
}
[[nodiscard]] constexpr LaunchRequest launch_request_for_caller() noexcept {
    return LaunchRequest{GameMode::Caller, std::nullopt};
}
[[nodiscard]] constexpr LaunchRequest launch_request_for_custom(CustomConfig weights) noexcept {
    return LaunchRequest{GameMode::Custom, weights};
}

// Forward a launch request to Zone 05 (bridge::request_game_launch). Defined in
// the .cpp so the bridge include and the launch call stay out of the header and
// out of the unit-test link; tests exercise launch_request_for_* directly.
void emit_launch(const LaunchRequest& request);

// ----- Escape behavior --------------------------------------------------------
//
// Notes — Escape Key Behavior: on Mode Selection, Escape returns to Root.
inline void on_mode_selection_escape() noexcept {
    backbone::set_screen(backbone::ScreenId::Root, std::nullopt);
}

// ----- Focus list -------------------------------------------------------------
//
// Order per ARCHITECTURE: STANDARD -> Aggressor -> Caller -> Custom -> Shop ->
// Help -> Settings -> Home, wrapping back to STANDARD; default pointer 0
// (STANDARD). The four cluster slots (Shop/Help/Settings/Home) are reserved here
// but rendered and handled by Zone 11.

inline constexpr backbone::FocusableId kFocusStandard =
    backbone::make_focusable_id("mode.standard");
inline constexpr backbone::FocusableId kFocusAggressorButton =
    backbone::make_focusable_id("mode.aggressor");
inline constexpr backbone::FocusableId kFocusCallerButton =
    backbone::make_focusable_id("mode.caller");
inline constexpr backbone::FocusableId kFocusCustomButton =
    backbone::make_focusable_id("mode.custom");
inline constexpr backbone::FocusableId kFocusModeShop =
    backbone::make_focusable_id("cluster.shop");
inline constexpr backbone::FocusableId kFocusModeHelp =
    backbone::make_focusable_id("cluster.help");
inline constexpr backbone::FocusableId kFocusModeSettings =
    backbone::make_focusable_id("cluster.settings");
inline constexpr backbone::FocusableId kFocusModeHome =
    backbone::make_focusable_id("cluster.home");

inline constexpr std::array<backbone::FocusableId, 8> kModeSelectionFocusOrder{
    kFocusStandard,   kFocusAggressorButton, kFocusCallerButton, kFocusCustomButton,
    kFocusModeShop,   kFocusModeHelp,        kFocusModeSettings, kFocusModeHome,
};

// Register the Mode Selection focus list as the base context. Called on screen
// entry (idempotent — register_focus_list replaces the active list).
inline void register_mode_selection_focus_list() noexcept {
    backbone::register_focus_list(backbone::ScreenId::ModeSelection, kModeSelectionFocusOrder);
}

// ----- Render + handler wiring -------------------------------------------------

// ZONES.md export. Draws the Mode Selection body (STANDARD + Aggressor/Caller/
// Custom row). Stateless; the persistent cluster is Zone 11's.
void render_mode_selection_screen();

// Install the Mode Selection event-router handlers (Escape -> Root; STANDARD/
// Aggressor/Caller -> emit_launch; Custom -> open popup; Home -> Root). Takes the
// main-loop-owned CustomPopupState (the Custom button opens it) and the weights
// store (the popup seeds from the last-saved split). All Mode Selection handlers
// no-op while the popup is open, so the modal captures interaction. Registers via
// the event router; called once from Z05 boot.
void install_mode_selection_handlers(CustomPopupState& popup, const CustomWeightsStore& store);

}  // namespace poker_trainer::screens
