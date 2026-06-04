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

// The Game-screen focus segment Z09 owns: every numeric box (in spawn order)
// then the bet-size group when present. Z08 composes this with the cluster tail
// (Shop/Help/Settings/X) in W3 -- // SEAM(Z08/Z11).
[[nodiscard]] std::vector<backbone::FocusableId> build_focus_segment(const InterrogatorState& state);

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
// OWN keyboard focus (it decides WantCaptureKeyboard and where typed characters
// land). Nothing couples them, so the outline and the typing target drift apart.
// The render path reconciles ImGui to focus_manager on the frame focus changes so
// the outlined element is always the one that owns keyboard input.

// What the render path must do this frame to keep ImGui's keyboard focus in sync.
enum class ImGuiFocusAction : std::uint8_t {
    None,          // focus unchanged, or moved to a non-Z09 element: leave ImGui alone
    FocusTextBox,  // SetKeyboardFocusHere on `target` -- a numeric box gained focus
    YieldKeyboard, // ClearActiveID -- a non-text element (the bet group) gained focus
};

struct FocusReconcile {
    ImGuiFocusAction action{ImGuiFocusAction::None};
    backbone::FocusableId target{backbone::kNoFocus};  // the box to focus; valid iff FocusTextBox
};

// Decide how to reconcile ImGui's keyboard focus, given the element ImGui was
// last reconciled to (`prev`) and the element focus_manager now reports
// (`current`). Pure (no ImGui calls), so the decision is unit-tested and the
// render glue only applies it. Returns None when focus is unchanged, or when it
// moved to an element Z09 does not own (a Z08/Z11 cluster stop -- SEAM(Z08/Z11)).
[[nodiscard]] FocusReconcile reconcile_imgui_focus(const InterrogatorState& state,
                                                   backbone::FocusableId prev,
                                                   backbone::FocusableId current) noexcept;

// Mouse click on a numeric box: move the focus_manager outline to it (ImGui has
// already taken text focus from the click) and activate keyboard mode, so the
// single focused element follows the click.
void focus_box_on_click(const NumericBox& box) noexcept;

}  // namespace poker_trainer::interrogator
