#include "audio/choreography.hpp"

#include <utility>

namespace poker_trainer::audio {

std::vector<ScheduledSfx> choreograph(engine::ScenarioType type, bool side_pot,
                                      std::uint64_t now_ms) {
    std::vector<ScheduledSfx> schedule;
    schedule.push_back(ScheduledSfx{SfxId::CardDeal, now_ms, kCardDealGain});
    // Side-pot takes precedence over the Caller chip push: an all-in side-pot
    // Caller plays the split cue, not the chip push.
    if (side_pot) {
        schedule.push_back(
            ScheduledSfx{SfxId::SidePotSplit, now_ms + kChoreographyDelayMs, kChipCueGain});
    } else if (type == engine::ScenarioType::Caller) {
        schedule.push_back(
            ScheduledSfx{SfxId::ChipPush, now_ms + kChoreographyDelayMs, kChipCueGain});
    }
    return schedule;
}

void ChoreographyScheduler::schedule(engine::ScenarioType type, bool side_pot,
                                     std::uint64_t now_ms) {
    pending_ = choreograph(type, side_pot, now_ms);
}

std::vector<ScheduledSfx> ChoreographyScheduler::poll(std::uint64_t now_ms) {
    std::vector<ScheduledSfx> due;
    std::vector<ScheduledSfx> remaining;
    for (const ScheduledSfx& entry : pending_) {
        if (entry.fire_at_ms <= now_ms) {
            due.push_back(entry);
        } else {
            remaining.push_back(entry);
        }
    }
    pending_ = std::move(remaining);
    return due;
}

}  // namespace poker_trainer::audio
