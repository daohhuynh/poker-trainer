#pragma once

#include "engine/scenario.hpp"

#include "backbone/focus_manager.hpp"

#include "settings/settings.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

// Zone 09 — Math Interrogator (Module 5), public umbrella header.
//
// Z09 owns the math-input UI, input state, the numeric keystroke filter, the
// dynamic per-branch input spawner, the Bet Size group, multi-tier input state,
// submission, and the *grading-margin comparison* — but NOT the truth math. The
// True Evaluator and the locked V8.1 EV/fold formulas are Z01's; the grading
// margins themselves (ev_margin, +/-5pp, exact outs, EV-tolerant bet size) are
// also already implemented in Z01's evaluator (ZONES.md: Z01 exports
// `evaluate(ScenarioState, UserAnswers) -> GradingResult` and `is_pass`). Z09
// therefore assembles a UserAnswers from the typed boxes + bet selection and
// calls engine::evaluate; it re-implements none of the truth or margin math
// (CLAUDE.md sec.10: no duplicated logic).

namespace poker_trainer::interrogator {

// Capacity of a numeric box's text buffer. Enough for any plausible typed answer
// ("-1234.56" etc.) plus the null terminator; entry past this is dropped by the
// keystroke filter / ImGui InputText.
inline constexpr std::size_t kBoxBufferCapacity = 16;

// ----- Focusable ids for the math-input + bet-size segment -----
//
// One stable id per possible box. The caller (Caller) and aggressor-equity-if-
// called boxes never coexist in one scenario, so they share kFocusEquity; the
// per-tier Fold / EV boxes get one id each.
inline constexpr backbone::FocusableId kFocusPotOdds =
    backbone::make_focusable_id("game.math.pot_odds");
inline constexpr backbone::FocusableId kFocusOuts =
    backbone::make_focusable_id("game.math.outs");
inline constexpr backbone::FocusableId kFocusEquity =
    backbone::make_focusable_id("game.math.equity");
inline constexpr backbone::FocusableId kFocusEvCaller =
    backbone::make_focusable_id("game.math.ev");
inline constexpr backbone::FocusableId kFocusBetSizeGroup =
    backbone::make_focusable_id("game.math.bet_size_group");

inline constexpr std::array<backbone::FocusableId, engine::kBetTierCount> kFocusFoldTier = {
    backbone::make_focusable_id("game.math.fold.t0"),
    backbone::make_focusable_id("game.math.fold.t1"),
    backbone::make_focusable_id("game.math.fold.t2"),
    backbone::make_focusable_id("game.math.fold.t3"),
};
inline constexpr std::array<backbone::FocusableId, engine::kBetTierCount> kFocusEvTier = {
    backbone::make_focusable_id("game.math.ev.t0"),
    backbone::make_focusable_id("game.math.ev.t1"),
    backbone::make_focusable_id("game.math.ev.t2"),
    backbone::make_focusable_id("game.math.ev.t3"),
};

// ----- Live input model -----

// One numeric input box: which math quantity it asks for, its per-tier index
// (set only for the bet-size-dependent Aggressor inputs Fold/EV), its focusable
// id, the keystroke-filter policy, and the user's typed text.
struct NumericBox {
    engine::InputId input{engine::InputId::PotOdds};
    std::optional<std::uint8_t> tier;            // 0..3 for per-tier Aggressor inputs
    backbone::FocusableId focus_id{};
    bool allow_decimal{true};                    // false for Outs (integer)
    bool allow_minus{false};                     // true for EV only
    std::array<char, kBoxBufferCapacity> text{};  // null-terminated typed value
};

// The Bet Size group: a single focus stop holding the four tier buttons.
struct BetSizeGroup {
    bool present{false};
    std::optional<engine::BetTier> selected;     // nullopt => graded incorrect
    backbone::FocusableId focus_id{kFocusBetSizeGroup};
};

// The live, mutable interrogator state for the active scenario.
struct InterrogatorState {
    std::optional<engine::ScenarioState> scenario;     // cached active scenario (truth source)
    std::vector<NumericBox> boxes;                      // visible numeric boxes, in spawn order
    BetSizeGroup bet_group;
    std::vector<backbone::FocusableId> focus_segment;   // boxes then bet group, in focus order
    std::optional<engine::GradingResult> last_result;   // last submission's grade (for Z13)
    bool last_math_pass{false};                         // is_pass(last_result)

    // The focus id ImGui's keyboard focus was last reconciled to (see
    // input_boxes.cpp::reconcile_imgui_focus). Tracks the focused element across
    // frames so the render path drives ImGui's keyboard focus / capture only on
    // the frame focus actually changes -- never every frame (which would trap the
    // text caret) and never re-grabbing after a click already coupled them.
    backbone::FocusableId last_synced_focus{backbone::kNoFocus};
};

// Math-correctness + within-target-time -> overall pass (the dealer-expression
// input, Module 5 output #2). The time component is Z10's (Module 6, W4); until
// it lands, callers inject `within_target_time` -- see compute_pass.
struct PassState {
    bool math_correct{false};
    bool within_target_time{false};   // SEAM(Z10): supplied by the temporal layer
    bool overall_pass{false};         // math_correct && within_target_time
};

// App-lifetime Z09 state, owned by boot (CLAUDE.md sec.10), threaded by
// reference into the render hook, key handlers, and bus subscription that
// install_interrogator registers. Mirrors Z07's ScreensRuntime ownership.
struct InterrogatorRuntime {
    InterrogatorState state;
    // Source of the live Settings. Boot injects the persisted/live settings at
    // integration; when unset, documented defaults are used (Settings{}), keeping
    // the zone self-contained for tests and W2 smoke-testing. Used only by the
    // seed-regeneration FALLBACK below; the normal path reads the single
    // authoritative ScenarioState from the bridge (bridge::active_scenario()).
    std::function<settings::Settings()> settings_source;

    // SEAM(Z10): the single time-injection point. Returns elapsed milliseconds
    // since the active scenario spawned. Z10 (Temporal, W4) supplies it; until
    // then it is unset and elapsed time reports as 0. on_submit reads it for the
    // GradingComplete payload, and in W4 compute_pass's within_target_time will
    // derive from this same source (elapsed_ms vs the computed target time).
    std::function<std::uint32_t()> elapsed_ms_source;
};

// ----- Self-registration -----

// Called once from boot at pass-2 integration (boot.cpp is NOT edited by this
// zone -- integration adds one install_interrogator(runtime) call). Registers
// the Game-screen render hook, the Enter-submit + number-key handlers, and the
// scenario_spawned reset subscription; the Game focus segment is registered on
// screen entry from inside the render hook.
void install_interrogator(InterrogatorRuntime& runtime);

// Render the math inputs for `scenario` (Z09's ZONES.md `render_math_inputs`
// export). Caches the scenario and (re)spawns the visible boxes when the
// scenario id changes. Z08's Game renderer calls this in W3.
void render_math_inputs(InterrogatorRuntime& runtime, const engine::ScenarioState& scenario);

}  // namespace poker_trainer::interrogator
