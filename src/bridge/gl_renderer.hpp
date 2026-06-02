#pragma once

struct ImDrawData;

// A minimal Dear ImGui renderer backend for WebGL2 (GLES3), written in-house so
// the WebAssembly app needs no third-party renderer backend beyond the vendored
// ImGui core (CLAUDE.md keeps the dependency set to ImGui / stb_image /
// miniaudio). It uploads ImGui's font atlas once and draws ImGui draw data each
// frame. Emscripten-only; never compiled into the native test build.
//
// ----- Why this is bespoke -----
// CLAUDE.md §3 vendors Dear ImGui as a core-only source drop (no imgui_impl_*
// backend), to keep the third-party surface to exactly ImGui / stb_image /
// miniaudio. The upstream OpenGL3/SDL/GLFW backends are therefore deliberately
// absent, so the bridge supplies its own ~250-line GLES3 backend instead of
// adding a fourth vendored dependency.
//
// ----- What it covers (and what it does not) -----
// Covered: the single ImGui render path used by this app — one shader program,
// one streamed vertex/index buffer pair, the font-atlas RGBA32 texture, the
// per-command scissor/clip rect, and the orthographic projection from
// DisplayPos/DisplaySize. That is everything the Loading / Error / Root / Mode
// Selection screens need today (solid fills, text, lines, arcs, themed widgets).
// Not covered: multi-texture user images (only the font atlas is bound today;
// the Z02 art-texture upload is its own seam), custom draw callbacks, and
// base-vertex draws (WebGL2 has none, so platform.cpp keeps vtx offsets at 0).
//
// ----- Swap path (if the official backend is ever vendored) -----
// The backend is reached ONLY through the three functions below; no call site
// touches GL directly. To swap in upstream imgui_impl_opengl3:
//   1. Vendor imgui_impl_opengl3.{h,cpp} under third_party/imgui/ (needs CLAUDE
//      .md §3 dependency-set sign-off first).
//   2. Map gl_renderer_init  -> ImGui_ImplOpenGL3_Init("#version 300 es")
//          gl_renderer_render -> ImGui_ImplOpenGL3_RenderDrawData
//          gl_renderer_shutdown -> ImGui_ImplOpenGL3_Shutdown
//      (add the matching ImGui_ImplOpenGL3_NewFrame() call in the frame loop).
//   3. Delete gl_renderer.cpp and drop it from the bridge_platform target.
// platform.cpp / main_loop.cpp call sites stay unchanged — they only ever name
// these three functions.

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
