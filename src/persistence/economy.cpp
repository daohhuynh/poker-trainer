#include "persistence/economy.hpp"

#include "audio/audio_paths.hpp"
#include "persistence/persistence_schema.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace poker_trainer::persistence {

namespace {

[[nodiscard]] std::uint8_t track_byte(audio::MusicTrackId track) noexcept {
    return static_cast<std::uint8_t>(track);
}

// Sorted-unique insert into a track-id vector (the persisted storage shape).
void insert_sorted_unique(std::vector<std::uint8_t>& ids, std::uint8_t id) {
    const auto it = std::lower_bound(ids.begin(), ids.end(), id);
    if (it == ids.end() || *it != id) {
        ids.insert(it, id);
    }
}

void erase_value(std::vector<std::uint8_t>& ids, std::uint8_t id) {
    const auto it = std::lower_bound(ids.begin(), ids.end(), id);
    if (it != ids.end() && *it == id) {
        ids.erase(it);
    }
}

[[nodiscard]] bool contains_value(const std::vector<std::uint8_t>& ids,
                                  std::uint8_t id) noexcept {
    return std::binary_search(ids.begin(), ids.end(), id);
}

}  // namespace

TomatoesState awarded(TomatoesState wallet, std::uint64_t amount) noexcept {
    constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();
    wallet.spendable = (wallet.spendable > kMax - amount) ? kMax : wallet.spendable + amount;
    wallet.lifetime = (wallet.lifetime > kMax - amount) ? kMax : wallet.lifetime + amount;
    return wallet;
}

void apply_pass_award(AppState& state) noexcept {
    state.tomatoes = awarded(state.tomatoes, kTomatoesPerPass);
}

std::uint32_t track_price(audio::MusicTrackId track) noexcept {
    // Each genre owns three consecutive track ids (genre*3 + position), so the position
    // within the genre is the id modulo the per-genre track count.
    constexpr std::size_t kTracksPerGenre = audio::kMusicTrackCount / audio::kMusicGenreCount;
    const std::size_t position = static_cast<std::size_t>(track) % kTracksPerGenre;
    return kTrackPriceByGenrePosition[position];
}

bool can_afford(const TomatoesState& wallet, std::uint64_t price) noexcept {
    return wallet.spendable >= price;
}

bool is_track_owned(const MusicLibraryState& lib, audio::MusicTrackId track) noexcept {
    if (audio::music_track_info(track).is_starter) {
        return true;
    }
    return contains_value(lib.unlocked_track_ids, track_byte(track));
}

bool is_track_in_pool(const MusicLibraryState& lib, audio::MusicTrackId track) noexcept {
    return contains_value(lib.active_pool_track_ids, track_byte(track));
}

bool purchase_track(AppState& state, audio::MusicTrackId track) {
    if (is_track_owned(state.music_library, track)) {
        return false;  // already owned (starter or previously purchased)
    }
    const std::uint64_t price = track_price(track);
    if (!can_afford(state.tomatoes, price)) {
        return false;  // insufficient Spendable Tomatoes
    }
    state.tomatoes.spendable -= price;  // only spendable decreases; lifetime is the metric
    insert_sorted_unique(state.music_library.unlocked_track_ids, track_byte(track));
    return true;
}

void add_track_to_pool(MusicLibraryState& lib, audio::MusicTrackId track) {
    if (!is_track_owned(lib, track)) {
        return;  // cannot rotate an unowned track
    }
    insert_sorted_unique(lib.active_pool_track_ids, track_byte(track));
}

void remove_track_from_pool(MusicLibraryState& lib, audio::MusicTrackId track) {
    erase_value(lib.active_pool_track_ids, track_byte(track));
}

}  // namespace poker_trainer::persistence
