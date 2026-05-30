#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace poker_trainer::audio {

// Identifiers for each sound effect in the trainer. The enum value is also the
// index into kSfxPaths below. The set and ordering follow the architecture's
// SFX library. Note: there is deliberately no correct/incorrect (Pass/Fail)
// performance-feedback SFX — the Post-Round Screen plays only the Slide In SFX
// on entry.
enum class SfxId : std::uint8_t {
    // Plays once when a scenario spawns. A single sound for the entire deal
    // (hole + community), not per-card.
    CardDeal = 0,

    // Plays when the user presses 1-6 to focus a math input box. Subtle and
    // fast; confirms keyboard input registered (especially valuable when the
    // HUD is hidden).
    ButtonClickConfirmation = 1,

    // Chip slide / push (Caller scenarios when the active opponent pushes
    // chips toward the pot).
    ChipPush = 2,

    // Side pot split (used when an All-In side pot triggers and chips
    // visualize splitting into the two pots).
    SidePotSplit = 3,

    // Modal open (any modal entering the screen).
    ModalSwooshOpen = 4,

    // Modal close (any modal leaving the screen).
    ModalSwooshClose = 5,

    // Frog easter egg trigger (played once when 22 consecutive dealer clicks
    // complete the Butler <-> Frog toggle).
    FrogToggle = 6,

    // Slide in (Game -> Post-Round 350ms slide transition, played at scenario
    // submission as the screen content slides right-to-left).
    SlideIn = 7,

    // Slide out (Post-Round -> Game 350ms slide transition, played when the
    // user commits the Again button as the content slides left-to-right).
    SlideOut = 8,
};

// The total number of SFX samples. Used to size arrays.
inline constexpr std::size_t kSfxCount = 9;

// Path to each SFX file, indexed by SfxId. All paths are relative to the asset
// root, which is the directory served as the CDN root in production builds.
inline constexpr std::array<std::string_view, kSfxCount> kSfxPaths = {
    "assets/audio/sfx/card_deal.ogg",
    "assets/audio/sfx/button_click_confirmation.ogg",
    "assets/audio/sfx/chip_push.ogg",
    "assets/audio/sfx/side_pot_split.ogg",
    "assets/audio/sfx/modal_swoosh_open.ogg",
    "assets/audio/sfx/modal_swoosh_close.ogg",
    "assets/audio/sfx/frog_toggle.ogg",
    "assets/audio/sfx/slide_in.ogg",
    "assets/audio/sfx/slide_out.ogg",
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
    bool is_starter;               // true if free / unlocked by default
    std::uint32_t price_tomatoes;  // 0 for starters, 25 for paid unlocks
};

// Per-track metadata indexed by MusicTrackId. The path field gives the
// asset-root-relative path to the streaming file (tracks are transcoded to
// ~3MB MP3 during asset preparation). The display_name field is used for Shop
// and Settings rendering.
inline constexpr std::array<MusicTrackInfo, kMusicTrackCount> kMusicTracks = {{
    // Lounge Jazz
    {"After Hours",       "assets/audio/music/lounge_jazz/after_hours.mp3",
        MusicGenre::LoungeJazz, true,  0},
    {"Smoke and Mirrors", "assets/audio/music/lounge_jazz/smoke_and_mirrors.mp3",
        MusicGenre::LoungeJazz, false, 25},
    {"Penthouse Suite",   "assets/audio/music/lounge_jazz/penthouse_suite.mp3",
        MusicGenre::LoungeJazz, false, 25},

    // Classical
    {"Nocturne",          "assets/audio/music/classical/nocturne.mp3",
        MusicGenre::Classical, true,  0},
    {"Counterpoint",      "assets/audio/music/classical/counterpoint.mp3",
        MusicGenre::Classical, false, 25},
    {"Adagio",            "assets/audio/music/classical/adagio.mp3",
        MusicGenre::Classical, false, 25},

    // Bossa Nova
    {"Ipanema Night",     "assets/audio/music/bossa_nova/ipanema_night.mp3",
        MusicGenre::BossaNova, true,  0},
    {"Sao Paulo",         "assets/audio/music/bossa_nova/sao_paulo.mp3",
        MusicGenre::BossaNova, false, 25},
    {"Copacabana",        "assets/audio/music/bossa_nova/copacabana.mp3",
        MusicGenre::BossaNova, false, 25},

    // Ambient
    {"Velvet Room",       "assets/audio/music/ambient/velvet_room.mp3",
        MusicGenre::Ambient,   true,  0},
    {"Slow Tide",         "assets/audio/music/ambient/slow_tide.mp3",
        MusicGenre::Ambient,   false, 25},
    {"Distant Lights",    "assets/audio/music/ambient/distant_lights.mp3",
        MusicGenre::Ambient,   false, 25},
}};

// Helper to look up info for a given track.
[[nodiscard]] constexpr const MusicTrackInfo& music_track_info(MusicTrackId id) noexcept {
    return kMusicTracks[static_cast<std::size_t>(id)];
}

}  // namespace poker_trainer::audio
