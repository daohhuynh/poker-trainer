#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace poker_trainer::audio {

// Identifiers for each sound effect in the trainer. The enum value
// is also the index into kSfxPaths below.
enum class SfxId : std::uint8_t {
    // Chip slide / push (Caller scenarios when the active opponent
    // pushes chips toward the pot).
    ChipPush = 0,

    // Chip stack landing in the pot (used at scenario resolution
    // when chips animate from their source to the pot).
    ChipLand = 1,

    // Side pot split (used when a side pot resolves in a multi-way
    // all-in scenario).
    SidePotSplit = 2,

    // Modal open (any modal entering the screen).
    ModalSwooshOpen = 3,

    // Modal close (any modal leaving the screen).
    ModalSwooshClose = 4,

    // Scenario spawn (used at the start of every scenario; played
    // as part of the audio choreography sequence).
    ScenarioSpawn = 5,

    // Pass result (played when the user passes a scenario, on the
    // Post-Round Screen entry).
    Pass = 6,

    // Fail result (played when the user fails a scenario, on the
    // Post-Round Screen entry).
    Fail = 7,

    // Frog easter egg toggle click (played when the user clicks
    // the dealer asset the trigger number of times to toggle to
    // the Frog).
    FrogToggle = 8,
};

// The total number of SFX samples. Used to size arrays.
inline constexpr std::size_t kSfxCount = 9;

// Path to each SFX file, indexed by SfxId. All paths are relative
// to the asset root, which is the directory served as the CDN root
// in production builds.
inline constexpr std::array<std::string_view, kSfxCount> kSfxPaths = {
    "assets/audio/sfx/chip_push.ogg",
    "assets/audio/sfx/chip_land.ogg",
    "assets/audio/sfx/side_pot_split.ogg",
    "assets/audio/sfx/modal_swoosh_open.ogg",
    "assets/audio/sfx/modal_swoosh_close.ogg",
    "assets/audio/sfx/scenario_spawn.ogg",
    "assets/audio/sfx/pass.ogg",
    "assets/audio/sfx/fail.ogg",
    "assets/audio/sfx/frog_toggle.ogg",
};

// Helper to look up the path for a given SfxId.
[[nodiscard]] constexpr std::string_view sfx_path(SfxId id) noexcept {
    return kSfxPaths[static_cast<std::size_t>(id)];
}

// Identifiers for each music genre. The enum value is also the index
// into kMusicGenreNames below.
enum class MusicGenre : std::uint8_t {
    LoungeJazz = 0,
    Classical = 1,
    BossaNova = 2,
    Ambient = 3,
};

inline constexpr std::size_t kMusicGenreCount = 4;

// Human-readable genre names, used for Shop and Settings display.
inline constexpr std::array<std::string_view, kMusicGenreCount> kMusicGenreNames = {
    "Lounge Jazz",
    "Classical",
    "Bossa Nova",
    "Ambient",
};

// Identifiers for each music track. Each genre has 3 tracks; track 0
// in each genre is the free starter, tracks 1 and 2 are paid unlocks.
// Track IDs are globally unique across genres.
enum class MusicTrackId : std::uint8_t {
    // Lounge Jazz
    LoungeJazz_Starter = 0,
    LoungeJazz_Track2 = 1,
    LoungeJazz_Track3 = 2,

    // Classical
    Classical_Starter = 3,
    Classical_Track2 = 4,
    Classical_Track3 = 5,

    // Bossa Nova
    BossaNova_Starter = 6,
    BossaNova_Track2 = 7,
    BossaNova_Track3 = 8,

    // Ambient
    Ambient_Starter = 9,
    Ambient_Track2 = 10,
    Ambient_Track3 = 11,
};

inline constexpr std::size_t kMusicTrackCount = 12;

// Metadata for a music track.
struct MusicTrackInfo {
    std::string_view display_name;
    std::string_view path;
    MusicGenre genre;
    bool is_starter;            // true if free / unlocked by default
    std::uint32_t price_cents;  // 0 for starters, 2500 ($25) for paid unlocks
};

// Per-track metadata indexed by MusicTrackId. The path field gives
// the asset-root-relative path to the streaming file. The display_name
// field is used for Shop and Settings rendering.
inline constexpr std::array<MusicTrackInfo, kMusicTrackCount> kMusicTracks = {{
    // Lounge Jazz
    {"After Hours",       "assets/audio/music/lounge_jazz/after_hours.ogg",
        MusicGenre::LoungeJazz, true,  0},
    {"Smoke and Mirrors", "assets/audio/music/lounge_jazz/smoke_and_mirrors.ogg",
        MusicGenre::LoungeJazz, false, 2500},
    {"Penthouse Suite",   "assets/audio/music/lounge_jazz/penthouse_suite.ogg",
        MusicGenre::LoungeJazz, false, 2500},

    // Classical
    {"Nocturne",          "assets/audio/music/classical/nocturne.ogg",
        MusicGenre::Classical, true,  0},
    {"Counterpoint",      "assets/audio/music/classical/counterpoint.ogg",
        MusicGenre::Classical, false, 2500},
    {"Adagio",            "assets/audio/music/classical/adagio.ogg",
        MusicGenre::Classical, false, 2500},

    // Bossa Nova
    {"Ipanema Night",     "assets/audio/music/bossa_nova/ipanema_night.ogg",
        MusicGenre::BossaNova, true,  0},
    {"Sao Paulo",         "assets/audio/music/bossa_nova/sao_paulo.ogg",
        MusicGenre::BossaNova, false, 2500},
    {"Copacabana",        "assets/audio/music/bossa_nova/copacabana.ogg",
        MusicGenre::BossaNova, false, 2500},

    // Ambient
    {"Velvet Room",       "assets/audio/music/ambient/velvet_room.ogg",
        MusicGenre::Ambient,   true,  0},
    {"Slow Tide",         "assets/audio/music/ambient/slow_tide.ogg",
        MusicGenre::Ambient,   false, 2500},
    {"Distant Lights",    "assets/audio/music/ambient/distant_lights.ogg",
        MusicGenre::Ambient,   false, 2500},
}};

// Helper to look up info for a given track.
[[nodiscard]] constexpr const MusicTrackInfo& music_track_info(MusicTrackId id) noexcept {
    return kMusicTracks[static_cast<std::size_t>(id)];
}

}  // namespace poker_trainer::audio
