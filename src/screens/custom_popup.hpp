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
    pop_custom_popup_focus();
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
