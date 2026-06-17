#pragma once

// The Emscripten main loop (ARCHITECTURE Module 3 Main Loop). Driven by
// requestAnimationFrame via emscripten_set_main_loop. Per frame it advances the
// animation clock once (milliseconds, never frame counts), pumps asset loading,
// advances the boot phase, syncs the canvas to the viewport, and renders the
// active screen (the loading / error / fallback screens directly, every other
// screen through the render-dispatch registry). Emscripten-only.

namespace poker_trainer::bridge {

// The per-frame entry (ZONES.md Z05 export app_frame()). Advances the animation
// clock, runs the per-frame tick registry, pumps asset loading, advances the boot
// phase, syncs the canvas, and renders the active screen. Registered as the
// requestAnimationFrame callback by start_main_loop.
void app_frame();

// Tear down the main loop (ZONES.md Z05 export app_shutdown()). Cancels the rAF
// loop; see the definition for why that is the full teardown under EXIT_RUNTIME=0.
void app_shutdown();

// Register app_frame with emscripten and hand control to the browser event loop.
// Does not return (simulate-infinite-loop).
void start_main_loop();

}  // namespace poker_trainer::bridge
