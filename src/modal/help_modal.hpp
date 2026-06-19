#pragma once

#include "backbone/focus_manager.hpp"

// Zone 11 — Equation Reference (Help) modal (ARCHITECTURE L563 / L521). A centered,
// scrollable, read-only overlay with the icon-pill "Help" header and five stacked
// sections (Formulas, Math input definitions, Scenario types, Grading rules) plus a
// full-width "Open Tutorial" button at the bottom. The Tutorial button initiates the
// tutorial overlay flow — a Z14 seam (inert this wave) — and is rendered disabled
// during an active tutorial (the modal lock also covers it).

namespace poker_trainer::modal {

// Help modal focusables: the Tutorial button, then the X close (the only
// interactive elements; the body is read-only). Default focus = Tutorial.
inline constexpr backbone::FocusableId kHelpTutorial =
    backbone::make_focusable_id("help.tutorial");
inline constexpr backbone::FocusableId kHelpClose =
    backbone::make_focusable_id("help.close");

}  // namespace poker_trainer::modal
