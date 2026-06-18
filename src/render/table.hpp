#pragma once

#include "render/layout.hpp"

struct ImDrawList;

// Zone 08 — poker table + room background (ARCHITECTURE Game Screen: a felt table
// integrated into the room, not a flat sticker). Procedural placeholder until the
// table-felt / background art + GPU-upload seam land: a bg_primary room wash and a
// felt-tinted oval (BgTableFelt) with a subtle rim. The background_game.png slot
// resolves to the bg_primary wash, matching the existing Z07/Z05 placeholder
// behavior; the GPU-upload bind point is the same deferred Z05 seam.

namespace poker_trainer::render {

// Draw the full-canvas room background wash and the felt table oval.
void draw_table(ImDrawList* dl, const GameLayout& layout);

}  // namespace poker_trainer::render
