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
//     producing and text-editing keys (digits, '.', '-', Backspace, arrows, ...)
//     so they edit the active box instead of also firing a screen command (e.g.
//     "1" types into the box rather than re-focusing input box 1).
//
// Tab, Enter, and Escape are the exception: they always reach the router even
// while a text field is active. They carry no text-editing meaning the app
// delegates to ImGui (keyboard nav is disabled), and ARCHITECTURE requires them
// to keep working mid-edit: Module 5 — "Enter submits all answers ... regardless
// of which input is focused"; Notes — Keyboard Focus Behavior — Tab advances the
// focus pointer; Notes — Escape Key Behavior — Escape dismisses the modal. The
// focus_manager / screens own these, so they are routed regardless of capture.
[[nodiscard]] bool router_should_see_key(bool imgui_wants_keyboard,
                                         backbone::KeyCode code) noexcept;

}  // namespace poker_trainer::bridge
