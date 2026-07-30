#pragma once

#include "engine/scenario.hpp"

#include <algorithm>
#include <cstdint>
#include <string>

struct ImDrawList;

// Zone 08 — HUD overlay (ARCHITECTURE Game Screen). The top-left info stack is the
// denomination legend (drawn by the chip layer), then the pot size, then the
// blinds. The Caller floating bet (call amount) sits near the pushed bet chips.
// Honors Show/Hide HUD: when off, the floating NUMBERS hide (call amount, pot
// total, blinds, opponent stack numbers) while the legend, the pushed-forward
// chips, the cards, and the position labels stay. The toggle UI is Z12's; Z08
// only reads settings.show_hud.

namespace poker_trainer::render {

// ----- Canvas-relative type scale (shared by every Game-screen readout) -----
//
// ImGui rasterizes its font atlas once at a fixed pixel size, so text drawn at
// ImGui::GetFontSize() measures the same on a 1280-wide canvas and on a 4K one.
// ARCHITECTURE (Notes — Canvas Layout and Scaling) requires the opposite: "Text
// and icon sizes scale uniformly with the canvas... When the canvas is larger
// (e.g., on a 4K monitor), text, buttons, and icons render at a larger pixel
// size while maintaining the same proportional relationship to the rest of the
// layout." Every Game readout therefore derives its size from canvas_ui_scale()
// instead of the atlas size, so the seat numbers, the top-left info stack and
// the top-center board readout hold their proportions at any canvas size.

// The nominal design canvas is 1920x1080 (ARCHITECTURE, Asset resolution).
inline constexpr float kReferenceCanvasWidth = 1920.0f;
inline constexpr float kReferenceCanvasHeight = 1080.0f;

// Readout body size in pixels at the reference canvas.
inline constexpr float kReadoutBaseSize = 24.0f;

// Size multipliers relative to the body size, so the amounts and the labels stay
// in proportion to one another at every canvas size.
inline constexpr float kReadoutRelPrimary = 1.0f;    // amounts: pot, blinds, stacks, bets
inline constexpr float kReadoutRelSecondary = 0.9f;  // labels: legend values, position letters

// The Game screen's canvas-relative size multiplier: 1.0 at the reference canvas,
// 0.67 at 1280x720, 2.0 at 4K. Multiplies type sizes and the top-center board
// readout's card size. Takes the SMALLER of the two dimension ratios so a canvas
// that is tall for its width scales to the width it actually has — otherwise the
// top row (legend on the left, board readout in the middle) grows past it and the
// two run into each other.
[[nodiscard]] float canvas_ui_scale();

// The same scale for an explicitly-given canvas, header-only so callers that know
// their canvas but must not link this library can use it. Zone 14's tutorial
// spotlights are the reason: they resolve against a Canvas handed in by the caller,
// and `tutorial` deliberately does not link `game`. Without this they would have to
// re-derive the formula, and a duplicated constant is exactly how the Game-screen
// spotlights drifted out of alignment with the table in the first place.
[[nodiscard]] inline float canvas_ui_scale_for(float canvas_w, float canvas_h) noexcept {
    if (canvas_w <= 0.0f || canvas_h <= 0.0f) {
        return 1.0f;
    }
    return std::min(canvas_w / kReferenceCanvasWidth, canvas_h / kReferenceCanvasHeight);
}

// Type size and line height for an explicitly-given canvas, header-only for the same
// reason as canvas_ui_scale_for. These omit the atlas-size floor that the ImGui-facing
// versions apply, since that needs a live ImGui frame; a spotlight only needs the
// element's proportions, so a few pixels at very small canvases does not matter.
[[nodiscard]] inline float readout_font_size_for(float rel, float canvas_w,
                                                 float canvas_h) noexcept {
    return kReadoutBaseSize * rel * canvas_ui_scale_for(canvas_w, canvas_h);
}
[[nodiscard]] inline float readout_line_height_for(float rel, float canvas_w,
                                                   float canvas_h) noexcept {
    return readout_font_size_for(rel, canvas_w, canvas_h) * 1.35f;
}

// Pixel type size for a readout drawn at `rel` x the body size on the current
// canvas. Floored at the atlas size: below it the glyphs are downsampled and stop
// resolving, and the minimum supported canvas is only 600px tall.
[[nodiscard]] float readout_font_size(float rel);

// Line advance for text at readout_font_size(rel) — the size plus the style's
// item spacing, mirroring ImGui::GetTextLineHeightWithSpacing(). Used to stack the
// top-left info lines and to clear a floating amount off a chip stack.
[[nodiscard]] float readout_line_height(float rel);

// Format a whole-dollar amount for display: "$<n>" in Cash mode, "<n> BB" (the
// big-blind multiple) in Big Blinds mode. A non-integer BB multiple shows one
// decimal. Used by the HUD and the opponent stack numbers (Units toggle).
[[nodiscard]] std::string format_amount(int dollars, bool cash_mode, int big_blind);

// Draw the pot size left-aligned at (x, y) in the top-left info stack (only when
// show_hud). Returns the line height advanced.
float draw_pot_size(ImDrawList* dl, float x, float y, const engine::ScenarioState& scenario,
                    bool cash_mode, bool show_hud);

// Draw the blinds left-aligned at (x, y) in the top-left info stack (only when
// show_hud). Returns the line height advanced.
float draw_blinds(ImDrawList* dl, float x, float y, const engine::ScenarioState& scenario,
                  bool cash_mode, bool show_hud);

// Draw the "To Call: $X" line left-aligned at (x, y) in the top-left info stack,
// directly below the blinds. Caller scenarios only (Aggressor faces no bet) and
// only when show_hud. Reads scenario.faced_bet — the same value the floating call
// number by the pushed chips shows — so the two can never disagree. Returns the
// line height advanced (even when nothing is drawn, for stable stacking).
float draw_to_call(ImDrawList* dl, float x, float y, const engine::ScenarioState& scenario,
                   bool cash_mode, bool show_hud);

// Draw the Caller floating bet amount beside the pushed bet chips (only when
// show_hud): left edge at anchor_x, vertically centered on anchor_y. ARCHITECTURE
// allows the call number "adjacent to or above the chips"; it sits beside them
// because the corridor between the active seat and the pot also carries that seat's
// position label and the pot's own stack, and at a small canvas there is no vertical
// room left for a third line. The chips themselves are drawn by the chip layer and
// stay visible regardless.
void draw_floating_bet(ImDrawList* dl, float anchor_x, float anchor_y, int bet_dollars,
                       bool cash_mode, int big_blind, bool show_hud);

// Draw the "Opp Fold: X%" line left-aligned at (x, y) in the top-left info stack,
// directly below the blinds. Aggressor scenarios only (the opponent's fold frequency
// is the Aggressor's given datum, the mirror of the Caller's To Call line) and only
// when show_hud. `viewed_tier` picks which bet tier's P(fold) is shown -- the tier
// currently on screen, so a multi-tier walk updates it per screen. Returns the line
// height advanced (even when nothing is drawn, for stable stacking).
float draw_opp_fold(ImDrawList* dl, float x, float y, const engine::ScenarioState& scenario,
                    std::uint8_t viewed_tier, bool show_hud);

// Draw the "Opp fold: X%" readout centered on the felt at (anchor_x, anchor_y): a
// general on-table HUD value NOT anchored to any opponent seat -- the Aggressor's
// analog of the Caller's floating call amount. Aggressor only, HUD-gated. `viewed_tier`
// selects the bet tier whose P(fold) is shown (same source as draw_opp_fold, so the
// two never disagree).
void draw_table_opp_fold(ImDrawList* dl, float anchor_x, float anchor_y,
                         const engine::ScenarioState& scenario, std::uint8_t viewed_tier,
                         bool show_hud);

}  // namespace poker_trainer::render
