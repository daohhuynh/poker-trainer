#pragma once

#include "bridge/canvas_sizing.hpp"

// Emscripten platform layer: owns the WebGL2 context, the Dear ImGui context,
// browser input wiring (DOM events -> backbone event_router + ImGui IO), and the
// canvas<->viewport binding. Emscripten-only; never compiled into the native
// test build. Written in-house so the app needs no SDL/GLFW dependency — only
// emscripten's bundled html5 + WebGL APIs.

namespace poker_trainer::bridge {

// Create the WebGL2 context, the ImGui context, the GL renderer, and register
// DOM input callbacks. Returns false if the WebGL2 context or GL renderer could
// not be created.
[[nodiscard]] bool platform_init();

// True if the launch User-Agent identified a mobile browser. Evaluated once and
// cached.
[[nodiscard]] bool platform_launch_is_mobile() noexcept;

// Sync the canvas drawing buffer and ImGui DisplaySize/FramebufferScale to the
// current viewport, and return the CSS-pixel canvas dims for this frame.
CanvasDims platform_sync_viewport() noexcept;

// Clear the framebuffer to the given RGBA and present the current ImGui draw
// data. Call after ImGui::Render().
void platform_present(float r, float g, float b, float a) noexcept;

}  // namespace poker_trainer::bridge
