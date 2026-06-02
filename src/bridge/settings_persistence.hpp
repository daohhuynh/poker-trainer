#pragma once

#include "backbone/game_mode.hpp"
#include "persistence/persistence_schema.hpp"
#include "theme/theme_tokens.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

// ============================================================================
// SEAM(Z12): interim settings-blob codec.
// ============================================================================
//
// The persisted settings live inside AppState::settings_blob as opaque bytes.
// The full Settings (de)serializer that owns that blob's format is Zone 12's
// (settings_modal / sections), which is a later wave and is NOT built yet. Until
// it lands, the bridge needs just two settings fields to round-trip at boot:
//   * DisplaySettings::active_theme_id  — applied at boot (theme-at-boot)
//   * GameplaySettings::custom_*_weight — the Mode Selection Custom popup's
//                                         Save/Reset
//
// This is a minimal, self-describing INTERIM encoding for exactly those fields.
// It is deliberately bridge-owned (NOT in src/settings/, so Zone 12's files stay
// untouched) and is a throwaway seam: when Zone 12 ships its real serializer it
// owns the settings_blob format outright and supersedes this. The interim blob
// carries its own magic+version ('PTS0') so Zone 12 can detect it and migrate or
// cleanly discard it (a one-time reset of theme/custom-weights when Z12 ships is
// the accepted cost). This magic is the INNER settings_blob's, distinct from
// Z04's OUTER AppState blob magic ('PTAS').
//
// Pure logic (no ImGui / Emscripten): lives in the native-testable bridge lib.

namespace poker_trainer::bridge {

// The subset of settings the interim codec round-trips. Defaults match the
// Settings struct defaults (No Limit theme, 50/50 custom split).
struct InterimSettings {
    std::uint8_t theme_id{theme::kThemeIdNoLimit};
    backbone::CustomConfig custom_weights{};  // {50, 50}
};

// Encode the interim settings into the settings_blob byte layout (8 bytes:
// magic 'P','T','S','0', u8 version, u8 theme_id, u8 aggressor, u8 caller).
[[nodiscard]] std::vector<std::uint8_t> encode_interim_settings(const InterimSettings& settings);

// Decode an interim settings blob. Returns std::nullopt when the blob is absent,
// too short, or not an interim ('PTS0') blob — i.e. "never written by this
// codec" — so callers can distinguish "no saved settings" from a saved default.
[[nodiscard]] std::optional<InterimSettings> decode_interim_settings(
    std::span<const std::uint8_t> blob) noexcept;

// The persisted active theme id, or kThemeIdNoLimit when nothing valid is saved.
// This is the value the boot path applies via theme::set_theme before the first
// frame.
[[nodiscard]] std::uint8_t read_persisted_theme_id(const persistence::AppState& state) noexcept;

// The persisted Custom-mode split, or std::nullopt when the popup's Save has
// never run (so Reset falls back to 50/50 per the CustomWeightsStore contract).
[[nodiscard]] std::optional<backbone::CustomConfig> read_persisted_custom_weights(
    const persistence::AppState& state) noexcept;

// A copy of `state` whose settings_blob carries `settings`, preserving every
// other AppState field. The write path for both theme and custom weights.
[[nodiscard]] persistence::AppState with_interim_settings(const persistence::AppState& state,
                                                          const InterimSettings& settings);

}  // namespace poker_trainer::bridge
