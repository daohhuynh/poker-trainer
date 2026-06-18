#pragma once

#include "render/layout.hpp"

struct ImDrawList;

// Zone 08 — the dealer on the right side of the Game screen (ARCHITECTURE: Butler
// in profile by default; the Frog easter-egg alternate renders front-facing).
// Procedural placeholder until the Butler/Frog art + GPU-upload seam land: a
// labeled rounded-rect silhouette anchored on the right. The placeholder still
// occupies the dealer hit region (layout.dealer_*) that game_screen.cpp tests for
// the mouse-only 22-click easter egg; the dealer is NOT in any focus list.

namespace poker_trainer::render {

// Draw the dealer placeholder. When `frog_active`, the Frog alternate is drawn
// (front-facing label); otherwise the Butler (profile) placeholder.
void draw_dealer(ImDrawList* dl, const GameLayout& layout, bool frog_active);

}  // namespace poker_trainer::render
