#pragma once

#include <cstdint>

// Phase 0 contract (added during the Z05 wave, authorized as a §14 amendment).
//
// GameMode and CustomConfig are shared cross-zone value types: Zone 07 (Mode
// Selection) produces them — the four launch buttons select a GameMode, and the
// Custom popup's open_custom_popup() returns a CustomConfig — and Zone 05's
// game_launch seam consumes them via request_game_launch(GameMode, optional<
// CustomConfig>). Because Zone 07 does not depend on Zone 05 (and vice versa is
// the only declared edge), the types cannot live in either zone's private
// headers; they live here in the universally-includable backbone so both
// producer and consumer reference one definition. Per CLAUDE.md §5 this header
// is a frozen contract once the Z05 wave is signed off.
//
// Note: a GameMode is a UI/launch concept, NOT a generator input. The scenario
// generator never receives a mode (the seed alone fixes the scenario type — see
// engine::peek_type). Z05 turns a mode into a concrete seed via its reject loop;
// the mode is a filter over candidate ids, never a branch into generation.

namespace poker_trainer::backbone {

// The four game-launch options presented on the Mode Selection screen. Each
// determines how Z05's reject loop filters candidate scenario ids by their
// seed-locked type (engine::peek_type).
enum class GameMode : std::uint8_t {
    // No filter: any candidate id is accepted. The ~50/50 Aggressor/Caller mix
    // emerges from the natural distribution of seed-locked types.
    Standard = 0,

    // Accept only ids whose seed-locked type is one of the three Aggressor
    // sub-types (engine::is_aggressor(peek_type(id)) == true).
    Aggressor = 1,

    // Accept only ids whose seed-locked type is Caller.
    Caller = 2,

    // Honor the Aggressor/Caller split weights carried in CustomConfig: each
    // launch draws a side per the weights, then accepts the next candidate id
    // matching that side.
    Custom = 3,
};

// Session-only Aggressor/Caller split for a Custom-mode launch. This is the
// transient configuration the Custom popup's Play button hands to game_launch;
// it is distinct from the persisted settings::GameplaySettings::custom_*_weight
// fields. Play uses these popup-current weights for the launched session
// without persisting them (only the popup's Save action writes the persisted
// fields), so the launch path needs its own carrier value.
//
// Invariant: aggressor_weight + caller_weight == 100. The two are integer
// percentages; the popup's coupled controls enforce the sum.
struct CustomConfig {
    std::uint8_t aggressor_weight{50};
    std::uint8_t caller_weight{50};
};

}  // namespace poker_trainer::backbone
