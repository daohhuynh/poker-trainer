#pragma once

#include "screens/again_button.hpp"
#include "screens/clipboard_fallback_modal.hpp"

#include "render/stat_modal.hpp"

#include "animations/button_morph.hpp"

#include "engine/scenario.hpp"

#include "backbone/focus_manager.hpp"

#include "settings/settings.hpp"

#include <cstdint>
#include <functional>
#include <vector>

// Zone 13 — the Post-Round Screen. Owns ScreenId::PostRound's renderer (single
// slot): the front-facing dealer, the translucent stat modal, the Scenario ID
// block with Copy/Share, the Exit door, the Again double-confirm button, and the
// inert persistent cluster. It captures the grading result on GradingComplete
// (Zone 09's handoff), drives the screen into PostRound, and renders the recap.

namespace poker_trainer::interrogator {
struct InterrogatorRuntime;
}

namespace poker_trainer::screens {

// The recap frozen at scenario completion. The dealer expression, the result, and
// the scenario are all snapshotted here so they hold constant across tab
// navigation (and are immune to Z09 clearing last_result on the next spawn).
struct PostRoundSnapshot {
    bool valid{false};
    engine::ScenarioState scenario{};
    engine::GradingResult result{};
    bool pass{false};               // is_pass (math + time per the event; time is a Z10 seam)
    std::uint32_t elapsed_ms{0};    // SEAM(Z10): 0 until the Temporal layer supplies it
    bool frog_active{false};        // Butler vs Frog (Z08's easter-egg query, at completion)
    std::uint64_t arrival_start_ms{0};  // dealer-arrival fade-in clock origin
};

// App-lifetime Z13 state, owned by boot and threaded by reference into the render
// hook, the key handlers, and the GradingComplete subscription that
// install_post_round_screen registers. Mirrors Z08's GameScreenRuntime ownership.
struct PostRoundRuntime {
    PostRoundSnapshot snap;
    AgainButton again;
    ClipboardFallback clip;
    render::RecapTab active_tab{render::RecapTab::Tier1};

    // The current screen's focus head (no cluster tail — the head+cluster
    // composition is the open Z11-wave decision; the cluster renders as inert
    // no-focus stubs this wave). Stored so register_focus_list spans live memory.
    std::vector<backbone::FocusableId> focus_head;
    bool focus_registered{false};       // focus list registered for this entry

    std::uint64_t copy_flash_ms{0};     // last successful Copy/Share; drives the "Copied" flash
    bool copy_flash_active{false};

    // Copy / Share hit rects cached from the last render, read by the in-gesture
    // MouseDown handler so the navigator.clipboard / navigator.share calls fire
    // inside the browser's user-gesture callstack (the rAF render path's
    // ImGui::IsMouseClicked fires a frame later, outside the gesture). See
    // install_post_round_screen.
    animations::Rect copy_rect{};
    animations::Rect share_rect{};

    // Live-settings source (Recap: dealer arrival animation + default recap tab).
    // Boot wires this to the same provider it injects into Z08/Z09. Unset ->
    // documented defaults, keeping the zone self-contained for tests.
    std::function<settings::Settings()> settings_source;
};

// Called once from boot at integration (one install_post_round_screen call;
// boot.cpp is otherwise unedited by this zone). Registers the PostRound renderer,
// the GradingComplete capture + transition, and the screen's key handlers. Reads
// the grading result from `interrogator` (Zone 09's result handoff).
void install_post_round_screen(PostRoundRuntime& runtime,
                               interrogator::InterrogatorRuntime& interrogator);

// The registered ScreenId::PostRound renderer body. Renders the whole screen from
// the captured snapshot; clears to the background when no snapshot is valid.
void render_post_round_screen(PostRoundRuntime& runtime);

}  // namespace poker_trainer::screens
