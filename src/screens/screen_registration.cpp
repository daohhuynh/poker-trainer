#include "screens/screen_registration.hpp"

#include "screens/custom_popup.hpp"
#include "screens/mode_selection_screen.hpp"
#include "screens/root_screen.hpp"

#include "animations/button_morph.hpp"
#include "backbone/animation_clock.hpp"
#include "backbone/screen_state.hpp"

#include <cstdint>

#include "bridge/ambient.hpp"
#include "bridge/frame_tick.hpp"
#include "bridge/screen_dispatch.hpp"

#ifdef __EMSCRIPTEN__
// Wasm-only: the app-root BridgeRuntime owns the single shared focus registry.
// bridge_runtime.hpp is an Emscripten-only header (it pulls the asset/loader
// state), so it is included only on the wasm build; the native test build skips
// this and leaves the popup's registry pointer null (its substrate paths no-op).
#include "bridge/bridge_runtime.hpp"
#endif

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
    if (runtime.morph.active() && !runtime.morph_to_root) {
        if (bridge::effective_reduce_motion()) {
            // Reduce Motion (in-app or OS): suppress the tween. Draw the end-state
            // (progress == 1, the Mode-Selection layout) for this one frame and commit
            // the Root->Mode transition immediately — the same reset + set_screen a
            // completed advance_morph performs — so the buttons arrive instantly with
            // no motion and no pop.
            render_root_morph_frame(1.0f);
            runtime.morph.reset();
            backbone::set_screen(backbone::ScreenId::ModeSelection, std::nullopt);
            return;
        }
        // Snapshot progress before advancing: advance_morph resets the controller
        // (and commits the screen-state transition to Mode Selection) on the step
        // that crosses the finish line.
        const float global_t = runtime.morph.progress(now);
        animations::advance_morph(runtime.morph, now);
        render_root_morph_frame(global_t);
        return;
    }
    render_root_screen();
    // Drawn after the Root body so it sits over the screen's own art, and only on
    // the static Root frame — never during the morph, so the frog is gone the
    // instant the user commits to Play. It keeps drawing while a modal is open;
    // the router predicate above is what stops it taking clicks.
    render_root_frog(runtime.frog);
}

void render_mode_dispatch(ScreensRuntime& runtime, CustomWeightsStore& store) {
    if (runtime.last_focus_screen != backbone::ScreenId::ModeSelection) {
        register_mode_selection_focus_list();
        runtime.last_focus_screen = backbone::ScreenId::ModeSelection;
    }

    // Reverse morph (Mode Selection -> Root): the STANDARD button + top-right icon cluster
    // morph back into the Root 2x2 grid. Reuses the MorphController timeline in reverse —
    // render_root_morph_frame(t) with t running 1 -> 0 is the exact reverse of the forward
    // morph. Mirrors render_root_dispatch's forward handling (Reduce Motion draws the Root
    // end-state for one frame and commits immediately).
    if (runtime.morph.active() && runtime.morph_to_root) {
        const std::uint64_t now = backbone::total_ms_since_app_start();
        const bool reduce = bridge::effective_reduce_motion();
        const bool complete = reduce || runtime.morph.is_complete(now);
        const float global_t = complete ? 0.0f : 1.0f - runtime.morph.progress(now);
        render_root_morph_frame(global_t);
        if (complete) {
            runtime.morph.reset();
            runtime.morph_to_root = false;
            backbone::set_screen(backbone::ScreenId::Root, std::nullopt);
        }
        return;
    }

    render_mode_selection_screen();
    render_custom_popup(runtime.popup, store);  // early-returns while closed
}

// Start the reverse (Mode Selection -> Root) button morph. MorphController::start debounces
// a second trigger mid-morph. Reduce Motion is honored by render_mode_dispatch, which
// commits to Root on the first frame (no visible morph), mirroring the forward path. Wired
// into the bridge return-to-root seam so the Zone 11 cluster Home icon and Zone 07's Escape
// both reach it.
void begin_mode_to_root_morph(ScreensRuntime& runtime) {
    runtime.morph.start(backbone::total_ms_since_app_start());
    runtime.morph_to_root = true;
}

// Per-frame re-entry watcher. Z07's dispatchers run only on Root / Mode, so they
// cannot observe a Game / Post-Round visit that overwrites the base focus context
// (Z09 / Z13 register_focus_list). This runs every frame via the bridge frame-tick
// registry and, on ANY screen change, invalidates the once-per-entry guard so the
// next Root / Mode render re-registers its base list. Without it, returning to Mode
// from Game (e.g. leave-drill -> Yes) leaves the stale Game focus list active and Tab
// is dead on Mode.
void watch_screen_for_focus_reentry(ScreensRuntime& runtime) {
    const backbone::ScreenStateSnapshot screen = backbone::read_screen_state();
    const backbone::ScreenId current = screen.current;
    // The frog's appearance roll is once per ENTRY to Root, not once per frame —
    // re-rolling every frame would make it strobe. This watcher is the only place
    // Zone 07 sees a screen CHANGE (the Root dispatch runs every frame while Root
    // is current and cannot tell entry from a repeat), so the roll hangs off it.
    // The prompt latch, by contrast, must run every frame: Zone 14 raises the
    // first-run prompt during its own render, which can land after this frame's
    // entry roll.
    note_tutorial_phase(runtime.frog, screen.tutorial_state.phase);
    if (current != runtime.observed_screen) {
        runtime.observed_screen = current;
        if (current == backbone::ScreenId::Root) {
            on_root_entry(runtime.frog, backbone::total_ms_since_app_start(),
                          screen.tutorial_state.phase);
        }
        runtime.last_focus_screen = backbone::ScreenId::Error;  // force re-register on next render
        // The button morph is only valid while Root (forward) or Mode Selection (reverse)
        // is current. If we left both some other way — e.g. an instant-cut launch clicked
        // mid-reverse-morph jumps straight to Game — drop the orphaned morph so a stale
        // start timestamp can't fire a bogus completion on a later Root/Mode visit.
        if (runtime.morph.active() && current != backbone::ScreenId::Root &&
            current != backbone::ScreenId::ModeSelection) {
            runtime.morph.reset();
            runtime.morph_to_root = false;
        }
    }
}

}  // namespace

void install_screens(ScreensRuntime& runtime, CustomWeightsStore& weights_store) {
#ifdef __EMSCRIPTEN__
    // Hand the Custom popup the single shared focus/input reconciliation registry
    // (owned off the app-root BridgeRuntime). The popup populates it on open and
    // reconciles + dispatches through it; this mirrors how boot wires the same
    // registry into Z09 (InterrogatorRuntime::focus_registry). Native tests skip
    // this block, so the pointer stays null and the popup's substrate paths no-op.
    runtime.popup.focus_registry = &bridge::runtime().focus_registry;
#endif

    // Per-frame screen watcher: keeps the once-per-entry focus-registration guard
    // honest across Game / Post-Round visits that replace the base focus context.
    bridge::register_frame_tick([&runtime] { watch_screen_for_focus_reentry(runtime); });

    // Wire the Mode Selection -> Root return so both triggers (Zone 11's cluster Home icon
    // and Zone 07's Escape / Home-key) play the button morph in reverse. Zone 07 owns the
    // controller; the bridge seam is the cross-zone indirection (Zone 11 does not depend on
    // Zone 07). Reduce Motion collapses it to an instant cut inside the reverse render.
    bridge::set_mode_to_root_transition([&runtime] { begin_mode_to_root_morph(runtime); });

    // Event handlers (registered once with the event router).
    seed_root_frog(runtime.frog);
    install_root_handlers(runtime.morph, runtime.frog);
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
