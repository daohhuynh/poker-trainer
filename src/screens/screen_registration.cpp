#include "screens/screen_registration.hpp"

#include "screens/custom_popup.hpp"
#include "screens/mode_selection_screen.hpp"
#include "screens/root_screen.hpp"

#include "animations/button_morph.hpp"
#include "backbone/animation_clock.hpp"
#include "backbone/screen_state.hpp"

#include <cstdint>

#include "bridge/screen_dispatch.hpp"

// The render-dispatch registration (bridge::register_screen_renderer) and the
// handler installs are issued from here, Zone 07's own init — no src/bridge/* file
// is edited to wire the screens in. The registry stores std::function callbacks
// that capture the main-loop-owned ScreensRuntime + weights store by reference;
// Z05 boot owns those for the app's lifetime, so the references never dangle.

namespace poker_trainer::screens {

namespace {

void render_root_dispatch(ScreensRuntime& runtime) {
    // Register the Root base focus list once on entry (register_focus_list resets
    // the focus pointer, so it must not run every frame).
    if (runtime.last_focus_screen != backbone::ScreenId::Root) {
        register_root_focus_list();
        runtime.last_focus_screen = backbone::ScreenId::Root;
    }

    const std::uint64_t now = backbone::total_ms_since_app_start();
    if (runtime.morph.active()) {
        // Snapshot progress before advancing: advance_morph resets the controller
        // (and commits the screen-state transition to Mode Selection) on the step
        // that crosses the finish line.
        const float global_t = runtime.morph.progress(now);
        animations::advance_morph(runtime.morph, now);
        render_root_morph_frame(global_t);
        return;
    }
    render_root_screen();
}

void render_mode_dispatch(ScreensRuntime& runtime, CustomWeightsStore& store) {
    if (runtime.last_focus_screen != backbone::ScreenId::ModeSelection) {
        register_mode_selection_focus_list();
        runtime.last_focus_screen = backbone::ScreenId::ModeSelection;
    }
    render_mode_selection_screen();
    render_custom_popup(runtime.popup, store);  // early-returns while closed
}

}  // namespace

void install_screens(ScreensRuntime& runtime, CustomWeightsStore& weights_store) {
    // Event handlers (registered once with the event router).
    install_root_handlers(runtime.morph);
    install_mode_selection_handlers(runtime.popup, weights_store);
    install_custom_popup_handlers(runtime.popup);

    // Render callbacks — last-writer-wins, so these replace Z05's blank default
    // for Root and Mode Selection.
    bridge::register_screen_renderer(backbone::ScreenId::Root,
                                     [&runtime] { render_root_dispatch(runtime); });
    bridge::register_screen_renderer(
        backbone::ScreenId::ModeSelection,
        [&runtime, &weights_store] { render_mode_dispatch(runtime, weights_store); });
}

}  // namespace poker_trainer::screens
