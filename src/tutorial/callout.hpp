#pragma once

#include "animations/button_morph.hpp"

#include <optional>
#include <string_view>

struct ImDrawList;

// Zone 14 — the tutorial overlay's presentation primitives: the grey lens with a
// spotlight cutout, the instructional callout panel + Next button, the bottom-center
// Skip Tutorial button, and the first-launch prompt. Rect computation is pure
// (canvas → rect) so the overlay's router mouse handler can hit-test the same rects
// the render path draws (clicks are routed through the event router, not ImGui).
//
// Colors come only from theme tokens (callout panel: bg_modal / text_primary /
// border_default; buttons: the standard button tokens; the grey lens reuses the
// modal-scrim dimming token — there is no separate fixed-neutral token and
// theme_tokens.hpp is sealed; see the report).

namespace poker_trainer::tutorial {

using animations::Canvas;
using animations::Rect;

// ----- Pure layout (canvas-relative; shared by render + hit-test) -----

// The bottom-center "Skip Tutorial" button.
[[nodiscard]] Rect skip_button_rect(Canvas canvas) noexcept;

// The callout panel, placed to avoid covering `spotlight`: to its right (3 o'clock)
// when there is room, else to its left, else above / below; bottom-center when there
// is no spotlight. The width narrows when boxed beside a wide central spotlight (the
// Post-Round recap), so the results stay visible (Group C).
[[nodiscard]] Rect callout_panel_rect(Canvas canvas, const std::optional<Rect>& spotlight) noexcept;

// The Next button inside a callout panel (bottom-right of the panel).
[[nodiscard]] Rect next_button_rect(const Rect& panel) noexcept;

// The Previous / Back button inside a callout panel (bottom-left of the panel).
[[nodiscard]] Rect prev_button_rect(const Rect& panel) noexcept;

// First-launch prompt: the centered panel and its two buttons.
[[nodiscard]] Rect prompt_panel_rect(Canvas canvas) noexcept;
[[nodiscard]] Rect prompt_start_rect(const Rect& panel) noexcept;
[[nodiscard]] Rect prompt_skip_rect(const Rect& panel) noexcept;

// ----- Render (ImGui draw-list; browser-verified) -----

// The 60%-opacity grey lens. With a spotlight it draws four scrim bands around the
// cutout (the spotlit element, already drawn into the same draw list by the screen,
// shows through) plus an accent ring; without one it scrims the whole canvas.
void render_lens(ImDrawList* dl, Canvas canvas, const std::optional<Rect>& spotlight);

// The callout panel: bg_modal fill, border_default outline, wrapped text, and the
// optional Next / Previous buttons (each with its own focus ring when focused, so the
// trapped tutorial control shows where keyboard focus is -- Group A/D).
void render_callout(ImDrawList* dl, const Rect& panel, std::string_view text, bool show_next,
                    bool next_focused, bool show_prev, bool prev_focused);

// The Skip Tutorial button.
void render_skip_button(ImDrawList* dl, Canvas canvas, bool focused);

// The first-launch prompt panel with Start Tutorial / Skip buttons.
void render_prompt(ImDrawList* dl, Canvas canvas, std::string_view message, bool start_focused,
                   bool skip_focused);

}  // namespace poker_trainer::tutorial
