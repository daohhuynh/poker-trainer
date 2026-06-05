#pragma once

#include "bridge/focus_registry.hpp"
#include "bridge/shared_scenario.hpp"

#include "assets/registry.hpp"
#include "assets/tier_loader.hpp"
#include "engine/scenario_id.hpp"

#include <memory>

// The application-root runtime owned by Z05's boot path: the asset registry and
// tier loader, the resolved boot route, and the boot phase the main loop drives.
// This is the one piece of top-level mutable state the bridge owns (the app
// object every program has); it is created once in app_init() and reached via
// runtime(). Emscripten-only — included only by the wasm app's render / main-loop
// / boot translation units, never by the pure (native-testable) bridge library.

namespace poker_trainer::bridge {

// Boot phase distinct from the screen-state singleton: while Loading, Z05 draws
// the loading screen and no app screen renders; once Running, the screen-state
// screen renders (Root/Game via the dispatch registry, or the Error screen).
enum class BootPhase : std::uint8_t {
    Loading = 0,
    Running = 1,
};

struct BridgeRuntime {
    // Declared before tier_loader so it outlives the loader (members destruct in
    // reverse order); the TierLoader holds a reference to this registry.
    assets::AssetRegistry registry;
    std::unique_ptr<assets::TierLoader> tier_loader;

    BootRoute route{BootRoute::NormalRoot};
    engine::ScenarioId shared_id{engine::kInvalidScenarioId};
    BootPhase phase{BootPhase::Loading};

    // The single shared focus/input reconciliation registry (CLAUDE.md §10: app-
    // root state owned here, not a global). Surfaces populate it by reference as
    // they register their focus lists; the reconcile + dispatch helpers read it.
    FocusRegistry focus_registry;
};

// The single app-root runtime. Valid only after app_init() has constructed it.
[[nodiscard]] BridgeRuntime& runtime() noexcept;

}  // namespace poker_trainer::bridge
