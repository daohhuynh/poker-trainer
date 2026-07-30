#pragma once

#include "render/layout.hpp"

struct ImDrawList;

// Zone 08 — poker table + room background (ARCHITECTURE Game Screen: a felt table
// integrated into the room, not a flat sticker). Draws background_game.png as the
// room and table_felt.png as the table surface, with the per-theme BgTableFelt
// wash over it.
//
// The felt art is authored in this renderer's own first-person D projection (see
// tools/gen_table_felt.py, which mirrors rim_spot() from layout.hpp) rather than
// as a flat top-down oval, so its painted edge coincides with the rim the seats
// and chip stacks are positioned from.
//
// Each image falls back to a procedural draw when its texture is unavailable —
// still loading, failed, or no GL context at all, which is the case in the native
// test build.

namespace poker_trainer::render {

// Draw the full-canvas room background wash and the felt table oval.
void draw_table(ImDrawList* dl, const GameLayout& layout);

}  // namespace poker_trainer::render
