#pragma once

#include "backbone/event_router.hpp"

// The keyboard sibling of platform.cpp's router_should_see_mouse / WantCaptureMouse
// gate (the SEAM(Z05) keyboard path). DOM key events are always fed to ImGui IO
// (so InputText boxes receive typed characters); this pure decision then gates the
// SECOND consumer — the backbone event router — so a key that ImGui is consuming
// for text editing is not ALSO dispatched as a screen-level command.
//
// Pure logic (no ImGui / Emscripten): lives in the native-testable bridge library
// so the gating decision is unit-tested exactly as platform.cpp applies it over
// the live ImGuiIO::WantCaptureKeyboard flag.

namespace poker_trainer::bridge {

// True when the backbone event router should receive `code` as a screen-level
// command, given whether ImGui currently wants the keyboard (an InputText is
// active / a text field is focused).
//
//   * ImGui is NOT capturing the keyboard -> the router sees every key, exactly
//     as before the gate existed.
//   * ImGui IS capturing the keyboard -> the router is suppressed for text-
//     producing and text-editing keys (digits, '.', '-', Backspace, ...) so they
//     edit the active box instead of also firing a screen command (e.g. "1" types
//     into the box rather than re-focusing input box 1).
//
// Tab, Enter, Escape, and the four arrow keys are the exception: they always
// reach the router even while a text field is active.
//
// Tab / Enter / Escape carry no text-editing meaning the app delegates to ImGui
// (keyboard nav is disabled), and ARCHITECTURE requires them to keep working
// mid-edit: Module 5 — "Enter submits all answers ... regardless of which input
// is focused"; Notes — Keyboard Focus Behavior — Tab advances the focus pointer;
// Notes — Escape Key Behavior — Escape dismisses the modal. The focus_manager /
// screens own these, so they are routed regardless of capture.
//
// Arrows (Up/Down/Left/Right) are routed too because the Custom popup's
// slider/input adjust handler is the lone registered arrow consumer and needs
// them while its InputText is active. This does NOT remove arrows from ImGui:
// feed_imgui_keyboard feeds every key into ImGui IO unconditionally, so the text
// cursor still moves on Left/Right; the gate governs only the router. No other
// screen registers an arrow handler (Game's math boxes are intentionally not
// arrow-bound; the bet-size group uses digits 1-4), so routing arrows everywhere
// is inert off the popup — hence a global exemption rather than a popup-gated one.
[[nodiscard]] bool router_should_see_key(bool imgui_wants_keyboard,
                                         backbone::KeyCode code) noexcept;

}  // namespace poker_trainer::bridge
