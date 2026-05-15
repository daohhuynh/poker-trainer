#pragma once

#include "backbone/screen_state.hpp"

#include <cstdint>
#include <functional>
#include <string_view>

namespace poker_trainer::backbone {

// Keyboard event types the router dispatches.
enum class KeyEventType : std::uint8_t {
    KeyDown = 0,
    KeyUp = 1,
};

// Mouse event types the router dispatches.
enum class MouseEventType : std::uint8_t {
    MouseDown = 0,
    MouseUp = 1,
    MouseMove = 2,
    Wheel = 3,
};

// Key codes. Subset of physical keys the trainer cares about. The
// bridge layer maps browser key events to these codes.
enum class KeyCode : std::uint16_t {
    Unknown = 0,
    Tab = 1,
    Enter = 2,
    Escape = 3,
    Space = 4,
    ArrowUp = 5,
    ArrowDown = 6,
    ArrowLeft = 7,
    ArrowRight = 8,
    Backspace = 9,
    Delete = 10,

    // Digit row (used for math input boxes and bet size tier selection).
    Digit0 = 16, Digit1 = 17, Digit2 = 18, Digit3 = 19, Digit4 = 20,
    Digit5 = 21, Digit6 = 22, Digit7 = 23, Digit8 = 24, Digit9 = 25,

    // Letter keys (rare; mostly for the search bar fuzzy-match input).
    LetterA = 32, LetterB = 33, LetterC = 34, LetterD = 35, LetterE = 36,
    LetterF = 37, LetterG = 38, LetterH = 39, LetterI = 40, LetterJ = 41,
    LetterK = 42, LetterL = 43, LetterM = 44, LetterN = 45, LetterO = 46,
    LetterP = 47, LetterQ = 48, LetterR = 49, LetterS = 50, LetterT = 51,
    LetterU = 52, LetterV = 53, LetterW = 54, LetterX = 55, LetterY = 56,
    LetterZ = 57,
};

// Modifier mask. Bits OR'd together when multiple modifiers are held.
enum class ModMask : std::uint8_t {
    None = 0,
    Shift = 1 << 0,
    Ctrl = 1 << 1,
    Alt = 1 << 2,
    Meta = 1 << 3,  // Command on macOS, Windows key on Windows
};

[[nodiscard]] constexpr ModMask operator|(ModMask a, ModMask b) noexcept {
    return static_cast<ModMask>(
        static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr bool has_mod(ModMask mask, ModMask flag) noexcept {
    return (static_cast<std::uint8_t>(mask) &
            static_cast<std::uint8_t>(flag)) != 0;
}

// A keyboard event passed to handlers.
struct KeyEvent {
    KeyEventType type;
    KeyCode code;
    ModMask mods;
};

// A mouse event passed to handlers.
struct MouseEvent {
    MouseEventType type;
    float x;            // Canvas coordinates, top-left origin
    float y;
    std::int32_t button;  // 0 = left, 1 = middle, 2 = right (for MouseDown/Up)
    float wheel_dy;       // Vertical scroll delta (for Wheel; 0 otherwise)
};

// Handler return value: true if the handler consumed the event,
// false to pass through to the next handler in the stack.
using KeyHandler = std::function<bool(const KeyEvent&)>;
using MouseHandler = std::function<bool(const MouseEvent&)>;

// Opaque handle for an installed handler. Returned by install_*
// functions; passed to uninstall_handler to remove the handler.
struct HandlerHandle {
    std::uint64_t value{0};
    constexpr bool operator==(const HandlerHandle&) const noexcept = default;
};

inline constexpr HandlerHandle kInvalidHandlerHandle{0};

// Handler priority levels. Lower numbers run first (higher priority).
// The router walks the stack in ascending priority order; the first
// handler that returns true consumes the event.
enum class HandlerPriority : std::uint8_t {
    ModalLayer = 0,           // Top: modal handlers (Esc-to-close, etc.)
    TutorialOverlay = 1,      // Z14's escape handler during tutorial
    ScreenContext = 2,        // Per-screen handlers (Z07, Z08, Z13)
    BackgroundCatchAll = 3,   // Lowest: catch-all (default behaviors)
};

// Install a keyboard handler at the given priority. Returns a handle
// that can be used to uninstall the handler later. Handler is owned
// by the router until uninstalled.
//
// The `tag` parameter is a human-readable string used for debugging
// (printed in router logs when the handler is installed/uninstalled).
// It does not affect behavior.
HandlerHandle install_key_handler(KeyHandler handler,
                                  HandlerPriority priority,
                                  std::string_view tag) noexcept;

// Install a mouse handler. Same semantics as install_key_handler.
HandlerHandle install_mouse_handler(MouseHandler handler,
                                    HandlerPriority priority,
                                    std::string_view tag) noexcept;

// Uninstall a previously-installed handler. The handle is invalidated
// by this call. Calling uninstall on an invalid handle is a no-op.
void uninstall_handler(HandlerHandle handle) noexcept;

// Dispatch a keyboard event through the handler stack. Called by Z05
// from the browser event bridge.
void dispatch_key_event(const KeyEvent& event) noexcept;

// Dispatch a mouse event through the handler stack.
void dispatch_mouse_event(const MouseEvent& event) noexcept;

// Clear all installed handlers. Used by the integration test only.
void reset_for_testing() noexcept;

}  // namespace poker_trainer::backbone
