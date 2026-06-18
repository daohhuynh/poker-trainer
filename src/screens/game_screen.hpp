#pragma once

#include "easter_egg/frog_toggle.hpp"

#include "backbone/focus_manager.hpp"

#include "engine/scenario.hpp"
#include "settings/settings.hpp"

#include <array>
#include <cstdint>
#include <functional>

// Zone 08 — Game screen render + registration into Zone 05's communication
// backbone. Z08 owns the rendering of the Game screen (table, dealer, cards,
// chips, legend, HUD, animations, Frog easter egg) but NOT the math inputs: those
// are Z09's. The render-dispatch registry is single-slot last-writer-wins, so Z08
// takes over ScreenId::Game's renderer (replacing Z09's W2 placeholder) and
// composes Z09 by CALLING interrogator::render_math_inputs from inside its own
// renderer — draw order is call order, so the inputs land on top of the table.

namespace poker_trainer::interrogator {
struct InterrogatorRuntime;
}

namespace poker_trainer::screens {

// ----- Persistent-cluster focusable ids (Game screen) -----
//
// The top-right cluster icons (Shop / Help / Settings / X) are focus stops on the
// Game screen: the focus list tail Z08 appends after Z09's math segment so Tab
// reaches them and wraps (ARCHITECTURE Game-screen focus list). Declared HERE in
// Z08 (not in Z09) so Z09 takes no Z08 dependency — install_game_screen hands the
// id list to Z09's InterrogatorRuntime, which appends it at focus-list
// registration. Their click/Enter activation (opening the modals / the Leave-Drill
// popup) is the Z11 no-op seam; here they are inert focus stops with a focus ring.
inline constexpr backbone::FocusableId kFocusGameClusterShop =
    backbone::make_focusable_id("game.cluster.shop");
inline constexpr backbone::FocusableId kFocusGameClusterHelp =
    backbone::make_focusable_id("game.cluster.help");
inline constexpr backbone::FocusableId kFocusGameClusterSettings =
    backbone::make_focusable_id("game.cluster.settings");
inline constexpr backbone::FocusableId kFocusGameClusterClose =
    backbone::make_focusable_id("game.cluster.close");

// The four cluster ids in focus / left-to-right draw order (Shop, Help, Settings,
// X), matching ARCHITECTURE's tail order after the bet-size group.
inline constexpr std::array<backbone::FocusableId, 4> kGameClusterFocusIds{
    kFocusGameClusterShop, kFocusGameClusterHelp, kFocusGameClusterSettings,
    kFocusGameClusterClose};

// HUD / Units snapshot the Game render reads each frame (from the live settings).
struct GameUiState {
    bool show_hud{true};
    bool cash_mode{true};
    settings::ChipDenominationMode denomination_mode{settings::ChipDenominationMode::StakeScaled};
};

// App-lifetime Z08 state owned by Z05 boot (CLAUDE.md sec.10), threaded by
// reference into the render hook and the scenario_spawned subscription that
// install_game_screen registers. Mirrors Z07's ScreensRuntime / Z09's
// InterrogatorRuntime ownership.
struct GameScreenRuntime {
    easter_egg::FrogToggleState frog;
    std::uint64_t spawn_ms{0};   // animation-clock ms the active scenario spawned at
    bool spawn_seen{false};      // a scenario_spawned has set spawn_ms this session
    // Live settings source (boot-injected; the same provider Z09 / the launch path
    // read). Unset -> documented Settings{} defaults, so the zone is self-contained.
    std::function<settings::Settings()> settings_source;
};

// Wire Z08 into Z05's render-dispatch registry and the scenario bus. Called once
// from Z05 boot AFTER install_interrogator, so Z08's renderer wins the Game slot:
//   * registers the Game-screen renderer (which draws the table then calls
//     interrogator::render_math_inputs so the inputs still render on top),
//   * subscribes to scenario_spawned (capture the spawn timestamp for the chip
//     push, reset the Frog click counter between scenarios).
void install_game_screen(GameScreenRuntime& runtime,
                         interrogator::InterrogatorRuntime& interrogator);

// Render the Game screen for `scenario` (ZONES.md Z08 `render_game_screen`
// export): table, opponents, cards, chips, dealer, HUD, then the math inputs on
// top. The registered renderer calls this; exposed for completeness / future use.
void render_game_screen(GameScreenRuntime& runtime,
                        interrogator::InterrogatorRuntime& interrogator,
                        const engine::ScenarioState& scenario, const GameUiState& ui);

// Easter-egg-active query, read later by Z13 for the front-facing Frog. Returns
// the installed runtime's Frog state; false before install_game_screen runs.
[[nodiscard]] bool easter_egg_frog_active() noexcept;

}  // namespace poker_trainer::screens
