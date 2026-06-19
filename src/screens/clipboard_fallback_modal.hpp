#pragma once

#include <array>
#include <cstddef>
#include <string_view>

// Zone 13 — the Copy / Share browser interop and the Z13-internal clipboard
// fallback mini-modal (ARCHITECTURE Post-Round "Scenario ID utility buttons").
//
// Copy writes the raw Scenario ID to navigator.clipboard; Share invokes the Web
// Share API with the /?scenario=<id> URL and falls back to copying that URL. If
// the clipboard is unavailable (Copy) or both Web Share AND its clipboard fallback
// fail (Share), this mini-modal opens: a centered overlay with "Copy this:", a
// selectable auto-selected field pre-filled with the id/URL, and an X close (the
// only dismiss control). The actual browser calls are EM_JS in the .cpp; the
// native build links pure stubs that report failure (never rendered in tests).

namespace poker_trainer::screens {

// Max length of the text shown in the fallback field (a 64-bit id, or the
// /?scenario=<id> URL with a domain) plus the null terminator.
inline constexpr std::size_t kClipboardTextCapacity = 128;

// ----- Browser interop (EM_JS in the .cpp; native stubs return false) -----

// Copy `text` to the system clipboard. Returns true on a successful (or
// optimistically-issued async) write, false when no clipboard path is available.
[[nodiscard]] bool platform_copy_text(std::string_view text);

// Invoke the Web Share API with `url`. Returns true when navigator.share exists
// and was invoked, false when unsupported (the caller then tries copy).
[[nodiscard]] bool platform_web_share(std::string_view url);

// ----- The fallback mini-modal -----

struct ClipboardFallback {
    bool open{false};
    bool needs_autoselect{false};  // set on open; the field auto-selects next frame
    std::array<char, kClipboardTextCapacity> text{};
};

// Open the fallback pre-filled with `text` (the raw id for Copy failures, the URL
// for Share failures). Pushes a modal focus context and notifies modal_state so the
// Post-Round key handlers (Again/Exit) are suppressed while it is open.
void open_clipboard_fallback(ClipboardFallback& modal, std::string_view text);

// Close the fallback (the X button is the only dismiss control).
void close_clipboard_fallback(ClipboardFallback& modal);

// Render the fallback (ImGui). Early-returns while closed. Handles the X close and
// the field auto-select. No-op outside the wasm build's live ImGui frame.
void render_clipboard_fallback(ClipboardFallback& modal);

}  // namespace poker_trainer::screens
