#include "audio/shuffle_pool.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace poker_trainer::audio {

ShufflePool::ShufflePool(std::uint64_t seed) noexcept : rng_(seed) {}

bool ShufflePool::contains(MusicTrackId track) const noexcept {
    return std::find(members_.begin(), members_.end(), track) != members_.end();
}

void ShufflePool::add(MusicTrackId track) {
    if (!contains(track)) {
        members_.push_back(track);
    }
}

void ShufflePool::remove(MusicTrackId track) {
    const auto member = std::find(members_.begin(), members_.end(), track);
    if (member != members_.end()) {
        members_.erase(member);
    }
    // Drop the track from the not-yet-played tail of the current cycle so it is not
    // selected again this cycle. Already-played entries (before the cursor) are
    // irrelevant. (A removal that empties the pool leaves the cursor past the end,
    // so next() reshuffles into the empty membership and reports silence.)
    if (cursor_ < order_.size()) {
        const auto tail = order_.begin() + static_cast<std::ptrdiff_t>(cursor_);
        order_.erase(std::remove(tail, order_.end(), track), order_.end());
    }
    if (last_played_ == track) {
        last_played_.reset();
    }
}

void ShufflePool::reshuffle() {
    order_.assign(members_.begin(), members_.end());
    std::shuffle(order_.begin(), order_.end(), rng_);
    // Avoid a back-to-back repeat across the cycle boundary when more than one
    // track is available to choose from.
    if (order_.size() > 1 && last_played_.has_value() && order_.front() == *last_played_) {
        std::swap(order_.front(), order_.back());
    }
    cursor_ = 0;
}

std::optional<MusicTrackId> ShufflePool::next() {
    if (members_.empty()) {
        order_.clear();
        cursor_ = 0;
        return std::nullopt;
    }
    if (cursor_ >= order_.size()) {
        reshuffle();
    }
    const MusicTrackId track = order_[cursor_];
    ++cursor_;
    last_played_ = track;
    return track;
}

}  // namespace poker_trainer::audio
