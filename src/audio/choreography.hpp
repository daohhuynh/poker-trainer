#pragma once

#include "audio/audio_paths.hpp"

#include "engine/scenario.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace poker_trainer::audio {

// Delay between the Card Deal SFX (T=0) and the scenario-type cue (T=400ms),
// driven by the animation clock at fire time (ARCHITECTURE Module 2 choreography).
inline constexpr std::uint64_t kChoreographyDelayMs = 400;

// Relative gains for the spawn choreography (multiplied on top of the global
// output volume): the Card Deal at full, the chip / side-pot cue softer behind it.
inline constexpr float kCardDealGain = 1.0f;
inline constexpr float kChipCueGain = 0.70f;

// One scheduled sound: which SFX, when to fire (absolute animation-clock ms), and
// the relative gain to play it at.
struct ScheduledSfx {
    SfxId id{};
    std::uint64_t fire_at_ms{0};
    float gain{1.0f};

    bool operator==(const ScheduledSfx&) const noexcept = default;
};

// The pure spawn-choreography decision: given a scenario's seed-locked type and
// side-pot status and the current clock, return the ordered SFX schedule.
//   T=0   : Card Deal at 100%.
//   T=400 : side-pot -> Side Pot Split at 70%; else Caller -> Chip Push at 70%;
//           else (Aggressor, non-side-pot) -> nothing.
[[nodiscard]] std::vector<ScheduledSfx> choreograph(engine::ScenarioType type,
                                                    bool side_pot,
                                                    std::uint64_t now_ms);

// Holds the pending spawn schedule and releases each entry when its fire time
// arrives. A new schedule supersedes any unfired entries from a prior scenario.
class ChoreographyScheduler {
public:
    // Replace the pending schedule with the choreography for a freshly spawned
    // scenario.
    void schedule(engine::ScenarioType type, bool side_pot, std::uint64_t now_ms);

    // Return every entry whose fire time has arrived (fire_at_ms <= now_ms), in
    // fire order, and remove them from the pending set.
    [[nodiscard]] std::vector<ScheduledSfx> poll(std::uint64_t now_ms);

    void clear() noexcept { pending_.clear(); }
    [[nodiscard]] std::size_t pending_count() const noexcept { return pending_.size(); }

private:
    std::vector<ScheduledSfx> pending_;
};

}  // namespace poker_trainer::audio
