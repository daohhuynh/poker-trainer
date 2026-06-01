#pragma once

#include <cstdint>
#include <string_view>

// Canvas sizing and the small-window / mobile fallback decisions
// (ARCHITECTURE Notes — Canvas Layout and Scaling).
//
// These are pure functions of the viewport and the launch User-Agent string so
// the layout decision is testable without a browser. The emscripten side (in
// the platform layer) reads the live viewport, sets the canvas pixel dims, and
// feeds the values here; nothing in this header touches emscripten or ImGui.

namespace poker_trainer::bridge {

// Canvas pixel dimensions. The canvas always fills the viewport — there is no
// letterboxing and no enforced aspect ratio.
struct CanvasDims {
    int width{0};
    int height{0};
    constexpr bool operator==(const CanvasDims&) const noexcept = default;
};

// The minimum canvas height (in CSS/pixel units) below which the standard
// layout degrades to unreadable. At or above this the standard layout renders;
// below it, the "Please use a larger window." fallback renders. ARCHITECTURE
// gives "approximately 600 pixels".
inline constexpr int kMinCanvasHeightPx = 600;

// The canvas dimensions for a given viewport: a direct passthrough (the canvas
// matches the viewport every frame). Negative inputs clamp to 0 so a bogus
// viewport can never produce negative dims.
[[nodiscard]] CanvasDims canvas_dims_from_viewport(int viewport_w,
                                                   int viewport_h) noexcept;

// True when the canvas height is below the minimum-size threshold.
[[nodiscard]] constexpr bool is_below_min_size(int canvas_height) noexcept {
    return canvas_height < kMinCanvasHeightPx;
}

// True when the launch User-Agent string identifies a mobile browser (iOS
// Safari, Android Chrome, etc.). Case-insensitive substring match against a
// fixed set of mobile UA tokens. Desktop Chrome / Firefox / Safari UAs do not
// match. (Known limitation: iPadOS 13+ reports a desktop "Macintosh" UA and is
// therefore treated as desktop — acceptable per the spec's UA-check approach.)
[[nodiscard]] bool is_mobile_user_agent(std::string_view user_agent) noexcept;

// What the canvas should render this frame, independent of the app's screen.
enum class DisplayMode : std::uint8_t {
    // Render the standard layout (the active screen).
    Normal = 0,
    // Render only the centered "Please use a larger window." message.
    TooSmall = 1,
    // Render only the centered "designed for desktop" message.
    Mobile = 2,
};

// Resolve the display mode. Mobile detection (done once at launch) takes
// precedence and is sticky regardless of size; otherwise the min-size threshold
// is re-checked every frame/resize. The standard layout resumes the moment the
// window grows back past the threshold.
[[nodiscard]] DisplayMode resolve_display_mode(bool is_mobile,
                                               int canvas_height) noexcept;

#ifdef __EMSCRIPTEN__
// Render the centered fallback message for TooSmall ("Please use a larger
// window.") or Mobile ("This trainer is designed for desktop...") on a
// bg_primary fill. No-op for DisplayMode::Normal. Definition lives in the wasm
// render layer (render_screens.cpp).
void render_fallback(DisplayMode mode);
#endif

}  // namespace poker_trainer::bridge
