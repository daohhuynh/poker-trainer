#pragma once

#include "assets/asset_paths.hpp"
#include "assets/tier_config.hpp"
#include "audio/audio_paths.hpp"
#include "backbone/screen_state.hpp"

// Pure tier-scheduling decision logic (ARCHITECTURE Module 3 "Loading Strategy").
//
// Z05 orchestrates WHEN each loading tier fires and WHAT happens on failure. The
// transport (serial fetch + retry/backoff) and the per-tier fatal-failure policy
// live in Zone 02 (tier_loader.{hpp,cpp} / tier_config.hpp); the live wiring that
// touches the registry and MEMFS lives in the Emscripten-only tier_orchestrator.
// This file is the slice of the orchestration that is a pure function of its
// inputs, so it is unit-testable natively with no fetch, no ImGui, no browser.
//
// It depends only on Phase-0 contract headers (asset_paths.hpp, tier_config.hpp,
// audio_paths.hpp — universally includable per CLAUDE.md section 5) and the
// backbone screen-state enum, so it adds no zone link dependency to the pure
// bridge library.

namespace poker_trainer::bridge {

// The loading tier a SFX sample belongs to. The PNG tiers come from
// asset_paths.hpp (Zone 02); the SFX are split here per Module 3: the Modal
// Open/Close Swoosh pair loads in Tier 2 (the user may open Settings/Shop/Help
// while on Root), and every other sample loads in Tier 3 (Card Deal, Button
// Click, Chip Push, Side Pot Split, Frog Toggle, Slide In, Slide Out).
[[nodiscard]] assets::AssetTier sfx_load_tier(audio::SfxId id) noexcept;

// True when an asset is required before the Game screen can render its table.
// Used by the Tier-2 navigation guard: if any required asset has fatally failed,
// the Play -> Game navigation is blocked and the error screen is shown instead
// (ARCHITECTURE Module 3 Tier 2 failure handling, "clicking Play when the table
// or card PNGs failed"). The required set is the game background, the table felt,
// the 52 card faces, and the card back — the assets without which the drill table
// cannot be drawn. Chips, the all-in marker, the dealer, and cluster glyphs are
// not gated here (they degrade without blocking the drill).
[[nodiscard]] bool is_game_launch_required_asset(assets::AssetId id) noexcept;

// True for the Root -> Mode Selection navigation edge, which fires the Tier-3
// load (remaining SFX + default music track). Z05's per-frame orchestrator reads
// the readable screen state and calls this on the transition edge.
[[nodiscard]] bool is_root_to_mode_transition(backbone::ScreenId prev,
                                              backbone::ScreenId current) noexcept;

}  // namespace poker_trainer::bridge
