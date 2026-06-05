#pragma once

// Zone 07 — Custom Mode Configuration popup.
//
// This header holds the pure, unit-tested logic of the popup (the coupled
// Aggressor/Caller weight solver, the keystroke/arrow-key input rules, and the
// Save/Reset/Play action semantics over an injected persistence seam) plus the
// focus-list registration and the render entry points. The render worker itself
// (render_custom_popup) lives in custom_popup.cpp and is the deferred, ImGui
// drawing seam; everything a test needs is pure and ImGui-free here.

#include "backbone/focus_manager.hpp"
#include "backbone/game_mode.hpp"

#include "bridge/focus_registry.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace poker_trainer::screens {

// CustomConfig is the landed Phase 0 backbone contract
// (src/backbone/game_mode.hpp): produced here (the popup's Aggressor/Caller
// split) and consumed by Zone 05's bridge::request_game_launch.
using backbone::CustomConfig;

// ----- Coupled-weight solver --------------------------------------------------
//
// The Aggressor and Caller weights always sum to exactly 100, so the popup's
// real state is a single number; the four inputs (two text fields, two sliders)
// are views onto it. Changing any one input sets one side and derives the other.

enum class WeightField : std::uint8_t {
    Aggressor = 0,
    Caller = 1,
};

// Clamp a raw value to [0, 100].
[[nodiscard]] constexpr std::uint8_t clamp_weight(int raw) noexcept {
    if (raw < 0) {
        return 0;
    }
    if (raw > 100) {
        return 100;
    }
    return static_cast<std::uint8_t>(raw);
}

// Solve the coupled pair from a single edited field. The edited field takes the
// clamped value; the opposite field becomes 100 minus it, preserving the sum-100
// invariant for any input.
[[nodiscard]] constexpr CustomConfig solve_from(WeightField field, int raw_value) noexcept {
    const std::uint8_t v = clamp_weight(raw_value);
    if (field == WeightField::Aggressor) {
        return CustomConfig{v, static_cast<std::uint8_t>(100 - v)};
    }
    return CustomConfig{static_cast<std::uint8_t>(100 - v), v};
}

// Apply an arrow-key step (+1 / -1, clamped to [0, 100]) to the given field and
// re-solve the coupled pair. Up/Right = +1, Down/Left = -1 (see Notes — Keyboard
// Focus Behavior); the caller maps keys to delta and field.
[[nodiscard]] constexpr CustomConfig step_weight(const CustomConfig& current, WeightField field,
                                                 int delta) noexcept {
    const int base =
        (field == WeightField::Aggressor) ? current.aggressor_weight : current.caller_weight;
    return solve_from(field, base + delta);
}

// ----- Text-input keystroke rules --------------------------------------------

// The text inputs reject any character other than 0-9 at the keystroke level.
[[nodiscard]] constexpr bool accepts_text_char(char c) noexcept {
    return c >= '0' && c <= '9';
}

// Parse a (possibly empty, possibly out-of-range) text field into a clamped
// weight. Non-digit characters are ignored defensively; an empty field reads as
// 0. Used when committing a typed value back into the coupled solver.
[[nodiscard]] inline std::uint8_t parse_clamped_weight(std::string_view text) noexcept {
    int value = 0;
    bool any_digit = false;
    for (char c : text) {
        if (accepts_text_char(c)) {
            any_digit = true;
            value = value * 10 + (c - '0');
            if (value > 100) {
                return 100;  // early clamp; also guards against overflow on long input
            }
        }
    }
    if (!any_digit) {
        return 0;
    }
    return clamp_weight(value);
}

// ----- Save / Reset / Play action semantics ----------------------------------

// Persistence seam for the Custom popup's Save/Reset. A concrete implementation
// backed by Zone 04 (writing the custom_*_weight settings fields) is wired by
// Zone 05's main loop; tests inject a fake. This mirrors the persistence layer's
// own injected-backend pattern.
class CustomWeightsStore {
public:
    virtual ~CustomWeightsStore() = default;

    // Persist the given weights (Save).
    virtual void save(CustomConfig weights) = 0;

    // The last persisted weights, or std::nullopt if Save has never run.
    [[nodiscard]] virtual std::optional<CustomConfig> load() const = 0;
};

// Reset semantics: restore the last-saved weights, or 50/50 if Save has never
// been performed. Pure core (takes the loaded value directly).
[[nodiscard]] constexpr CustomConfig reset_weights(
    std::optional<CustomConfig> last_saved) noexcept {
    return last_saved.value_or(CustomConfig{50, 50});
}

// Reset against the store (loads then applies reset_weights). Does not launch,
// does not persist.
[[nodiscard]] inline CustomConfig reset_to_saved(const CustomWeightsStore& store) {
    return reset_weights(store.load());
}

// Save action: persists the current weights. Does not launch, does not close.
inline void save_weights(CustomWeightsStore& store, CustomConfig current) {
    store.save(current);
}

// ----- Focus list -------------------------------------------------------------
//
// Order per ARCHITECTURE: Aggressor input -> Aggressor slider -> Caller input ->
// Caller slider -> Save -> Reset -> Play -> X close, wrapping back to the
// Aggressor input; default focus pointer 0 (Aggressor input).

inline constexpr backbone::FocusableId kFocusAggressorInput =
    backbone::make_focusable_id("custom.aggressor_input");
inline constexpr backbone::FocusableId kFocusAggressorSlider =
    backbone::make_focusable_id("custom.aggressor_slider");
inline constexpr backbone::FocusableId kFocusCallerInput =
    backbone::make_focusable_id("custom.caller_input");
inline constexpr backbone::FocusableId kFocusCallerSlider =
    backbone::make_focusable_id("custom.caller_slider");
inline constexpr backbone::FocusableId kFocusSave =
    backbone::make_focusable_id("custom.save");
inline constexpr backbone::FocusableId kFocusReset =
    backbone::make_focusable_id("custom.reset");
inline constexpr backbone::FocusableId kFocusPlay =
    backbone::make_focusable_id("custom.play");
inline constexpr backbone::FocusableId kFocusClose =
    backbone::make_focusable_id("custom.close");

inline constexpr std::array<backbone::FocusableId, 8> kCustomPopupFocusOrder{
    kFocusAggressorInput, kFocusAggressorSlider, kFocusCallerInput, kFocusCallerSlider,
    kFocusSave,           kFocusReset,           kFocusPlay,        kFocusClose,
};

// Push the popup's focus context (modal focus trap) on open, with focus at the
// default (Aggressor input); pop it on close.
inline void push_custom_popup_focus() {
    backbone::push_focus_context(kCustomPopupFocusOrder, kCustomPopupFocusOrder[0],
                                 "custom_popup");
}
inline void pop_custom_popup_focus() { backbone::pop_focus_context(); }

// ----- Render-side state + entry points (deferred ImGui seam) -----------------

// View-model the main loop owns while the popup is open. Holds the live weights
// plus the per-field text buffers ImGui's InputText needs across frames.
struct CustomPopupState {
    CustomConfig weights{50, 50};
    std::array<char, 4> aggressor_buf{};
    std::array<char, 4> caller_buf{};
    bool open{false};

    // Opening-frame guard. The DOM mousedown that opens the popup (the Custom
    // button click, routed through the event router) is also queued into ImGui's
    // IO, so on the popup's first rendered frame ImGui reports IsMouseClicked at a
    // point outside the just-appeared window — which the click-outside path would
    // otherwise read as a dismiss, closing the popup the same frame it opened.
    // Set true on open; render_custom_popup keeps it raised (suppressing
    // click-outside dismissal) until that mouse button is released, then arms the
    // dismissal for any subsequent click outside the modal.
    bool just_opened{false};

    // ----- Shared focus/input reconciliation substrate (Stage 3) -----
    //
    // The single FocusRegistry owned off BridgeRuntime, wired in by boot via
    // install_screens (under __EMSCRIPTEN__; null in native tests, where the
    // registry-dependent code paths no-op). render_custom_popup populates it with
    // the popup's eight focusables on open and reconciles ImGui's keyboard focus
    // through it; the dispatch handler routes arrows/Space/Enter through it. The
    // popup adds no reconcile/dispatch of its own — it calls the bridge substrate
    // (begin_focus_reconcile / draw_focus_ring / dispatch_focus_key). Mirrors how
    // Z09 (InterrogatorRuntime::focus_registry) holds the same shared registry.
    bridge::FocusRegistry* focus_registry{nullptr};

    // The focus id ImGui's keyboard focus was last reconciled to, threaded into
    // bridge::begin_focus_reconcile so the render path steers ImGui only on the
    // frame focus changes (never re-grabbing in sync, which would trap the caret).
    // Reset to kNoFocus when the registry is (re)populated on open. Mirrors
    // InterrogatorState::last_synced_focus.
    backbone::FocusableId last_synced_focus{backbone::kNoFocus};

    // Raised once the registry is populated for the current open session; cleared
    // on close so the next open repopulates. Keeps populate off the per-frame path
    // (the entries capture state/store by reference and read live values, so they
    // need building only once per open).
    bool focus_registered{false};

    // Deferred-dismiss flag for the keyboard activate path. The Play/X activate
    // closures live IN the shared registry; doing the dismiss work directly inside
    // them would mutate (clear()) the very registry slot whose std::function is
    // mid-invocation — a use-after-free. This applies to BOTH the close (clears the
    // registry) AND the launch (request_game_launch fires scenario_spawned, whose
    // Z09 handler also clear()s + repopulates the shared registry). So the closures
    // only raise flags; the dispatch handler performs the close-then-launch after
    // dispatch_focus_key returns, with no entry closure left on the stack. Mirrors
    // render_custom_popup's existing `dismissed` deferral.
    bool request_close{false};

    // Captured Custom launch config for a deferred Play. Set (to the live weights)
    // by Play's activate closure (keyboard) and by the Play button (mouse), both
    // alongside the dismiss request. The dismiss path closes the popup FULLY first
    // (pop focus context + clear registry) and THEN fires the launch with this
    // config — so the Game screen's Z09 setup registers its focus list into the
    // freshly-restored base context and repopulates the just-cleared registry, the
    // same clean slate the three preset Mode buttons give it. nullopt for a
    // no-launch dismiss (X / click-outside / Escape). See take_pending_launch_and_close.
    std::optional<CustomConfig> pending_launch{};
};

// ZONES.md export. Opens the popup: pushes the focus context and returns the
// starting weights for this popup session (50/50 default, or the store-loaded
// value when wired). The live, coupled, multi-frame popup is render_custom_popup.
CustomConfig open_custom_popup();

// Dismiss the popup without launching: clears the open flag and pops the focus
// context. The single dismissal path shared by the X button, click-outside, and
// Escape; also called after Play launches. Never persists, never launches.
inline void close_custom_popup(CustomPopupState& state) {
    state.open = false;
    state.just_opened = false;
    state.request_close = false;
    state.focus_registered = false;
    state.pending_launch.reset();  // a closed popup carries no deferred launch
    // Drop the popup's eight entries from the shared registry so no stale popup
    // closures linger on it once we are back on Mode Selection (the next open
    // repopulates). Null in native tests. Safe here because every caller invokes
    // close OUTSIDE an executing registry entry — the render dismiss path runs
    // after ImGui::End, the Escape handler is separate from dispatch, and the
    // keyboard Play/X path defers via request_close — so no entry's std::function
    // is mid-invocation when clear() destroys the slots.
    if (state.focus_registry != nullptr) {
        state.focus_registry->clear();
    }
    pop_custom_popup_focus();
}

// Drain a deferred Play/dismiss: close the popup FULLY (reset flags, clear the
// shared registry, pop the focus context) and return the captured Custom launch
// config, or nullopt for a no-launch dismiss (X / click-outside / Escape). The
// close runs BEFORE the caller fires the launch on purpose: request_game_launch's
// scenario_spawned makes Z09 register the Game focus list into the now-active base
// context and repopulate the registry, so the Game screen starts from the same
// clean focus state the preset Mode buttons produce. The launch is deliberately
// NOT fired here — the caller (render dismiss path / dispatch handler) fires it
// after this returns, keeping every registry-clearing call off the stack of an
// executing registry activate closure (the use-after-free guard). The captured
// config is read before close() resets it, so the returned value survives the reset.
[[nodiscard]] inline std::optional<CustomConfig> take_pending_launch_and_close(
    CustomPopupState& state) {
    const std::optional<CustomConfig> launch = state.pending_launch;
    close_custom_popup(state);
    return launch;
}

// The live popup render worker (deferred ImGui seam). Draws the rows, sliders,
// and Save/Reset/Play row; keeps all four inputs coupled via solve_from; routes
// Save/Reset/Play through save_weights / reset_to_saved / bridge launch.
void render_custom_popup(CustomPopupState& state, CustomWeightsStore& store);

// Install the popup's modal-layer Escape handler (Notes — Escape Key Behavior:
// the modal captures Escape and dismisses without launching). Deferred wiring
// seam: registered at ModalLayer priority via the event router; takes the
// main-loop-owned CustomPopupState by reference. Called by Zone 05, not this wave.
void install_custom_popup_handlers(CustomPopupState& state);

}  // namespace poker_trainer::screens
