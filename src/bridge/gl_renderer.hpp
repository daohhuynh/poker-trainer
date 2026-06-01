#pragma once

struct ImDrawData;

// A minimal Dear ImGui renderer backend for WebGL2 (GLES3), written in-house so
// the WebAssembly app needs no third-party renderer backend beyond the vendored
// ImGui core (CLAUDE.md keeps the dependency set to ImGui / stb_image /
// miniaudio). It uploads ImGui's font atlas once and draws ImGui draw data each
// frame. Emscripten-only; never compiled into the native test build.

namespace poker_trainer::bridge {

// Build the shader program, buffers, and font-atlas texture. Requires a current
// WebGL2 context. Returns false on shader/program failure.
[[nodiscard]] bool gl_renderer_init();

// Render one frame of ImGui draw data. No-op when draw_data is null or the
// framebuffer is zero-sized.
void gl_renderer_render(ImDrawData* draw_data);

// Release GL objects. Called at shutdown (rarely reached in a browser tab).
void gl_renderer_shutdown();

}  // namespace poker_trainer::bridge
