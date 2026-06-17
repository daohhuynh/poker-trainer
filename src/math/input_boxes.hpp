#pragma once

#include "math/interrogator.hpp"

#include "engine/scenario.hpp"

#include <optional>
#include <string_view>
#include <vector>

// Numeric input boxes (Module 5): the keystroke filter, the per-branch box
// spawner, the focus-segment builder, buffer parsing, and the render entry.

namespace poker_trainer::interrogator {

// Keystroke-level numeric filter. Returns true if appending `ch` to `current`
// keeps it a valid partial numeric entry:
//   - digits '0'..'9' are always accepted;
//   - a single '.' is accepted only when `allow_decimal` and none is present;
//   - a leading '-' is accepted only when `allow_minus` and `current` is empty.
// Every other character (percent signs, commas, letters, a second '.' or a
// non-leading '-') is rejected. Raw numbers: '30' means 30%, not 0.30.
[[nodiscard]] bool accepts_numeric_char(std::string_view current, char ch,
                                        bool allow_decimal, bool allow_minus) noexcept;

// The visible numeric boxes for a scenario branch, in spawn order. Bet Size is
// NOT a numeric box (it is the focus group; see bet_size_buttons.hpp), so it
// never appears here.
//   Caller:                      Pot Odds, Outs, Equity, EV
//   Aggressor Pure Bluff/Value:  Fold% (per active tier), EV (per active tier)
//   Aggressor Semi-Bluff:        Fold% / EV (per active tier), Equity-if-Called
// Multi-tier (Bet Sizing Engine on) spawns Fold%/EV once per tier (0..3);
// single-tier spawns them once for the presented tier. Equity-if-Called is
// bet-size-independent: it spawns exactly once and is reused for every tier.
[[nodiscard]] std::vector<NumericBox> build_boxes(const engine::ScenarioState& s);

// The Game-screen focus segment Z09 owns for the CURRENT screen: the numeric
// boxes visible right now (in focus order) then the bet-size group when present.
// For Caller / single-tier Aggressor that is every box (one screen). For a
// multi-tier Aggressor it is only the current tier's boxes (Fold, EV, plus the
// tier-1 Equity-if-Called for Semi-Bluff) -- the sequential per-tier focus list.
// Z08 composes this with the cluster tail (Shop/Help/Settings/X) in W3 --
// // SEAM(Z08/Z11). Re-run on every tier advance (see tier_flow / keybinds).
[[nodiscard]] std::vector<backbone::FocusableId> build_focus_segment(const InterrogatorState& state);

// True when `box`'s typed buffer parses to a value (Outs via int, every other box
// via double). A partial / empty entry ("", "-", ".", "-.") is NOT filled and the
// evaluator grades it incorrect. Shared by the submit-all fill check and the
// per-tier advance gate.
[[nodiscard]] bool box_filled(const NumericBox& box) noexcept;

// True when `box` belongs on the CURRENT screen. Caller / single-tier Aggressor:
// always true (all inputs on one screen). Multi-tier Aggressor: a per-tier box
// (Fold / EV) only on its own tier; the bet-size-independent Equity-if-Called
// (the tier-less Aggressor box) only on tier 1 (index 0).
[[nodiscard]] bool box_in_current_view(const InterrogatorState& state,
                                       const NumericBox& box) noexcept;

// The boxes on the current screen, in focus/render order (a filtered, order-
// preserving view over `state.boxes`). Pointers are valid until `boxes` is
// rebuilt (configure_for_scenario). Used by the focus-segment builder, the
// per-tier advance gate, and number-key targeting.
[[nodiscard]] std::vector<const NumericBox*> current_view_boxes(const InterrogatorState& state);

// (Re)spawn the boxes + bet group + focus segment for `s`, clearing all typed
// buffers, the prior grade, and any bet selection. Called from the
// scenario_spawned subscription and on scenario-id change in the render hook.
void configure_for_scenario(InterrogatorState& state, const engine::ScenarioState& s);

// Parse a box's typed buffer. Empty or incomplete entries ("", "-", ".", "-.")
// yield nullopt -- an unfilled box, graded incorrect by the evaluator.
[[nodiscard]] std::optional<double> parse_box_double(const NumericBox& box) noexcept;
[[nodiscard]] std::optional<int> parse_box_int(const NumericBox& box) noexcept;

// ----- ImGui keyboard-focus reconciliation (Module 5 focus unification) -----
//
// focus_manager is the single source of truth for which element is focused: Tab
// and the 1-6 digit-focus keys move it and it draws the outline. ImGui keeps its
// OWN keyboard focus. The coupling glue -- the reconcile DECISION, the
// SetKeyboardFocusHere / ClearActiveID driving, and the focus-ring draw -- now
// lives in the shared bridge substrate (src/bridge/focus_registry.hpp); Z09
// registers its elements there (boxes text, bet group non-text) and the render
// hook drives it. Nothing reconcile-related remains in this header.

// Mouse click on a numeric box: move the focus_manager outline to it (ImGui has
// already taken text focus from the click) and activate keyboard mode, so the
// single focused element follows the click.
void focus_box_on_click(const NumericBox& box) noexcept;

}  // namespace poker_trainer::interrogator
