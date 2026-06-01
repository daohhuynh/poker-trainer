#pragma once

// The Emscripten main loop (ARCHITECTURE Module 3 Main Loop). Driven by
// requestAnimationFrame via emscripten_set_main_loop. Per frame it advances the
// animation clock once (milliseconds, never frame counts), pumps asset loading,
// advances the boot phase, syncs the canvas to the viewport, and renders the
// active screen (the loading / error / fallback screens directly, every other
// screen through the render-dispatch registry). Emscripten-only.

namespace poker_trainer::bridge {

// Register the per-frame callback with emscripten and hand control to the
// browser event loop. Does not return (simulate-infinite-loop).
void start_main_loop();

}  // namespace poker_trainer::bridge
