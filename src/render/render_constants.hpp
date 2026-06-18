#pragma once

// Zone 08 — shared render tuning constants for the Game screen.
//
// These are visual-implementation-pass values (sizes, spacings, durations) kept
// in one place so the table / chips / cards / dealer / HUD render TUs share a
// consistent scale. They are layout numbers, not product behavior: ARCHITECTURE
// fixes the conventions (descending denomination columns, ~300 ms chip push,
// linear stack height) but defers exact pixels to "the visual implementation
// pass". Nothing here is a theme color — colors come from theme tokens.

#include <cstdint>

namespace poker_trainer::render {

// ----- Chips -----
inline constexpr float kChipRadius = 11.0f;        // chip disk radius (px)
inline constexpr float kChipStackStep = 4.0f;      // vertical px per stacked chip (linear height)
inline constexpr float kChipColumnPitch = 30.0f;   // horizontal px between adjacent columns
inline constexpr float kLegendSlotPitch = 40.0f;   // horizontal px between legend slots
inline constexpr int kChipSegments = 24;           // disk tessellation
inline constexpr int kMaxChipsPerColumn = 30;      // on-screen guard for huge stacks

// ----- Caller chip push (ARCHITECTURE: ~300 ms ease-out, once at spawn) -----
inline constexpr std::uint64_t kChipPushDurationMs = 300;

// ----- Cards -----
inline constexpr float kCardWidth = 46.0f;
inline constexpr float kCardHeight = 64.0f;
inline constexpr float kCardFanStep = 30.0f;       // horizontal px between fanned card centers
inline constexpr float kCardRounding = 5.0f;
// First-person: the hero's hole cards sit nearest the camera, drawn larger than
// the mid-distance community board.
inline constexpr float kHeroCardScale = 1.4f;

// ----- Side pot offset (two distinct pools) -----
inline constexpr float kSidePotOffsetX = 70.0f;
inline constexpr float kSidePotOffsetY = 34.0f;

}  // namespace poker_trainer::render
