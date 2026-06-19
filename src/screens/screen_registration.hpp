#pragma once

#include "screens/custom_popup.hpp"

#include "animations/button_morph.hpp"
#include "backbone/screen_state.hpp"

// Zone 07 — render/handler registration into Zone 05's communication backbone.
//
// Zone 07 owns the rendering of Root and Mode Selection but does not own the main
// loop; Zone 05's loop invokes "the active screen" each frame through its
// render-dispatch registry (bridge::register_screen_renderer). This is Zone 07's
// side of that seam: it self-registers its real renders (replacing Z05's blank
// default), installs its event-router handlers, and threads the main-loop-owned
// animation/popup state. The registration lives here in Zone 07, not in any
// src/bridge/* file.

namespace poker_trainer::screens {

// Main-loop-owned, app-lifetime state the Zone 07 renders need. Held by Z05 boot
// (the bridge owns the app-root runtime); install_screens threads references into
// the dispatch registry. Not global state — Z05 owns exactly one of these.
struct ScreensRuntime {
    // The Root -> Mode Selection morph (started by the Root Play handler, drawn
    // and advanced by the Root render).
    animations::MorphController morph;

    // The Custom popup view-model (live weights + the InputText buffers ImGui
    // needs across frames + the open flag).
    CustomPopupState popup;

    // The last screen whose base focus list was registered. The renders register
    // a screen's list once on entry: register_focus_list resets the focus pointer
    // to 0, so it must NOT run every frame. The sentinel Error means "no Zone 07
    // list registered yet", so the first Root/Mode render registers.
    backbone::ScreenId last_focus_screen{backbone::ScreenId::Error};

    // Screen observed by the per-frame re-entry watcher (install_screens registers a
    // frame tick). Zone 07's dispatchers only run on Root / Mode, so they cannot see
    // a Game / Post-Round visit that REPLACES the base focus context (Z09 / Z13 call
    // register_focus_list); the watcher invalidates last_focus_screen on any screen
    // change so the next Root / Mode render re-registers. Error = nothing observed yet.
    backbone::ScreenId observed_screen{backbone::ScreenId::Error};
};

// Wire Zone 07 into Zone 05's render-dispatch registry and the event router:
//   * registers the Root render (draws the in-flight morph, else the static grid)
//     and the Mode Selection render (draws the screen + the Custom popup over it),
//   * installs the Root / Mode Selection / Custom popup event handlers,
//   * threads the main-loop-owned ScreensRuntime and the persistence-backed
//     weights store the Custom popup Saves/Resets through.
// register_screen_renderer is last-writer-wins, so Zone 07's renders replace the
// blank default for Root and Mode Selection. Called once from Z05 boot, after the
// platform (ImGui) and the dispatch registry are up.
void install_screens(ScreensRuntime& runtime, CustomWeightsStore& weights_store);

}  // namespace poker_trainer::screens
