#pragma once

// Z06 ImGui style bridge.
//
// Dear ImGui keeps a global style structure whose Colors[] array drives every
// standard widget. This translation unit writes the active theme's tokens into
// that array. Custom-drawn elements read the Theme struct directly via
// get_color(); ImGui's style array is the second consumer of the same source
// of truth (ARCHITECTURE: "The Theme struct is the single source of truth;
// ImGui's style array is one of two consumers of it").

struct ImGuiStyle;  // Defined in imgui.h, included by the matching .cpp.

namespace poker_trainer::theme {

// Write the active theme's color tokens into the given ImGui style's Colors[]
// slots. Pure with respect to global ImGui state — it mutates only the passed
// style — which makes it directly testable without an ImGui context.
void apply_theme_to_imgui_style(ImGuiStyle& style) noexcept;

// Apply the active theme to the global ImGui style (ImGui::GetStyle()). A
// no-op if no ImGui context exists yet. Called by set_theme() on every switch
// and at startup once the context is created.
void refresh_active_theme_style() noexcept;

}  // namespace poker_trainer::theme
