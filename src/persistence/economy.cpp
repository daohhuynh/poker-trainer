#include "persistence/economy.hpp"

#include "audio/audio_paths.hpp"
#include "persistence/persistence_schema.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace poker_trainer::persistence {

namespace {

[[nodiscard]] std::uint8_t track_byte(audio::MusicTrackId track) noexcept {
    return static_cast<std::uint8_t>(track);
}

// Sorted-unique insert into a track-id vector. Used for unlocked_track_ids, which IS a
// set (the schema documents it sorted) — NOT for the rotation, whose order is the user's
// playlist order.
void insert_sorted_unique(std::vector<std::uint8_t>& ids, std::uint8_t id) {
    const auto it = std::lower_bound(ids.begin(), ids.end(), id);
    if (it == ids.end() || *it != id) {
        ids.insert(it, id);
    }
}

[[nodiscard]] bool contains_value(const std::vector<std::uint8_t>& ids,
                                  std::uint8_t id) noexcept {
    return std::ranges::find(ids, id) != ids.end();
}

// Rotation edits. Both preserve the order of every element they do not touch, which is
// what makes active_pool_track_ids a queue rather than a set.
void append_if_absent(std::vector<std::uint8_t>& ids, std::uint8_t id) {
    if (!contains_value(ids, id)) {
        ids.push_back(id);
    }
}

void erase_value(std::vector<std::uint8_t>& ids, std::uint8_t id) {
    const auto it = std::ranges::find(ids, id);
    if (it != ids.end()) {
        ids.erase(it);
    }
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
    append_if_absent(lib.active_pool_track_ids, track_byte(track));
}

void remove_track_from_pool(MusicLibraryState& lib, audio::MusicTrackId track) {
    erase_value(lib.active_pool_track_ids, track_byte(track));
}

std::vector<audio::MusicTrackId> rotation_tracks(const MusicLibraryState& lib) {
    std::vector<audio::MusicTrackId> out;
    out.reserve(lib.active_pool_track_ids.size());
    for (const std::uint8_t id : lib.active_pool_track_ids) {
        if (id < audio::kMusicTrackCount) {
            out.push_back(static_cast<audio::MusicTrackId>(id));
        }
    }
    return out;
}

void add_starter_tracks_to_pool(MusicLibraryState& lib) {
    // Catalog order is genre-major with the starter first in each genre, so a forward
    // walk seeds Lounge Jazz, Classical, Bossa Nova, Ambient in that order.
    for (std::size_t i = 0; i < audio::kMusicTrackCount; ++i) {
        const auto track = static_cast<audio::MusicTrackId>(i);
        if (audio::music_track_info(track).is_starter) {
            add_track_to_pool(lib, track);
        }
    }
}

void normalize_rotation(MusicLibraryState& lib) {
    // Ownership is deliberately NOT re-checked here. add_track_to_pool already gates on
    // it, so an unowned member can only come from a damaged blob — and going quiet on a
    // user who had tracks is a worse failure than playing one track they no longer own.
    std::vector<std::uint8_t> clean;
    clean.reserve(lib.active_pool_track_ids.size());
    for (const std::uint8_t id : lib.active_pool_track_ids) {
        if (id < audio::kMusicTrackCount) {
            append_if_absent(clean, id);
        }
    }
    lib.active_pool_track_ids = std::move(clean);
}

}  // namespace poker_trainer::persistence
