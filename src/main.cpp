// Application entry point.
//
// In the WebAssembly build this hands off to Z05's boot path (app_init), which
// initializes the backbone, the Emscripten platform, and the zones, then enters
// the main loop and never returns. In a native build (the test configuration)
// there is no browser, so main is an empty success — the app's behavior is
// exercised through the unit-test binaries, not this entry point.

#ifdef __EMSCRIPTEN__
#include "bridge/boot.hpp"
#endif

int main() {
#ifdef __EMSCRIPTEN__
    poker_trainer::bridge::app_init();
#endif
    return 0;
}
