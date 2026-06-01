#pragma once

// Z05 boot entry point. app_init() initializes the communication backbone in the
// fixed order, brings up the Emscripten platform (WebGL2 + ImGui + input), runs
// zone-level init (kicks the Tier-1 asset load and reconciles persistence via
// Zone 04 load_state()), parses the ?scenario= URL parameter and resolves the
// boot route, registers the service worker, and hands control to the main loop.
// Called once from main(). Emscripten-only.

namespace poker_trainer::bridge {

void app_init();

}  // namespace poker_trainer::bridge
