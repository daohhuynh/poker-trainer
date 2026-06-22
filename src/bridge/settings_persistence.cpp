#include "bridge/settings_persistence.hpp"

#include "backbone/game_mode.hpp"
#include "persistence/persistence_schema.hpp"
#include "settings/settings.hpp"
#include "theme/theme_tokens.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace poker_trainer::bridge {

namespace {

// Interim settings-blob layout (see settings_persistence.hpp). Little-endian /
// byte-wise; all fields are single bytes so endianness is moot.
constexpr std::array<std::uint8_t, 4> kInterimMagic{
    std::uint8_t{'P'}, std::uint8_t{'T'}, std::uint8_t{'S'}, std::uint8_t{'0'}};
constexpr std::uint8_t kInterimVersion = 0;
constexpr std::size_t kInterimBlobSize = kInterimMagic.size() + 4;  // magic + ver + theme + 2 weights

}  // namespace

std::vector<std::uint8_t> encode_interim_settings(const InterimSettings& settings) {
    std::vector<std::uint8_t> out;
    out.reserve(kInterimBlobSize);
    out.insert(out.end(), kInterimMagic.begin(), kInterimMagic.end());
    out.push_back(kInterimVersion);
    out.push_back(settings.theme_id);
    out.push_back(settings.custom_weights.aggressor_weight);
    out.push_back(settings.custom_weights.caller_weight);
    return out;
}

std::optional<InterimSettings> decode_interim_settings(
    std::span<const std::uint8_t> blob) noexcept {
    if (blob.size() < kInterimBlobSize) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < kInterimMagic.size(); ++i) {
        if (blob[i] != kInterimMagic[i]) {
            return std::nullopt;
        }
    }
    if (blob[kInterimMagic.size()] != kInterimVersion) {
        return std::nullopt;
    }
    InterimSettings settings{};
    settings.theme_id = blob[kInterimMagic.size() + 1];
    settings.custom_weights.aggressor_weight = blob[kInterimMagic.size() + 2];
    settings.custom_weights.caller_weight = blob[kInterimMagic.size() + 3];
    return settings;
}

std::uint8_t read_persisted_theme_id(const persistence::AppState& state) noexcept {
    // Format-agnostic: a full 'PTS1' blob carries the theme; otherwise fall back to
    // the interim 'PTS0'; otherwise the default. read_persisted_settings does exactly
    // this resolution, so route through it for a single source of truth.
    return read_persisted_settings(state).display.active_theme_id;
}

std::optional<backbone::CustomConfig> read_persisted_custom_weights(
    const persistence::AppState& state) noexcept {
    // "Never saved" must stay distinguishable (Reset -> 50/50), so this reports
    // nullopt only when NEITHER codec recognizes the blob. A recognized blob (full or
    // interim) always carries a custom split.
    if (const std::optional<settings::Settings> full = decode_settings(state.settings_blob)) {
        return backbone::CustomConfig{full->gameplay.custom_aggressor_weight,
                                      full->gameplay.custom_caller_weight};
    }
    if (const std::optional<InterimSettings> interim = decode_interim_settings(state.settings_blob)) {
        return interim->custom_weights;
    }
    return std::nullopt;
}

persistence::AppState with_interim_settings(const persistence::AppState& state,
                                            const InterimSettings& settings) {
    persistence::AppState next = state;
    next.settings_blob = encode_interim_settings(settings);
    return next;
}

// ===== Full settings codec ('PTS1') =====

namespace {

constexpr std::array<std::uint8_t, 4> kFullMagic{std::uint8_t{'P'}, std::uint8_t{'T'},
                                                 std::uint8_t{'S'}, std::uint8_t{'1'}};
constexpr std::uint8_t kFullVersion = 1;

void put_u8(std::vector<std::uint8_t>& o, std::uint8_t v) { o.push_back(v); }
void put_bool(std::vector<std::uint8_t>& o, bool v) {
    o.push_back(static_cast<std::uint8_t>(v ? 1 : 0));
}
void put_u16(std::vector<std::uint8_t>& o, std::uint16_t v) {
    o.push_back(static_cast<std::uint8_t>(v & 0xFFu));
    o.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
}
void put_u32(std::vector<std::uint8_t>& o, std::uint32_t v) {
    o.push_back(static_cast<std::uint8_t>(v & 0xFFu));
    o.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
    o.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFu));
    o.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFu));
}
void put_float(std::vector<std::uint8_t>& o, float f) {
    put_u32(o, std::bit_cast<std::uint32_t>(f));
}

// Sequential reader over the blob; sets ok=false on any over-read so a truncated
// blob is rejected wholesale rather than yielding garbage.
struct Reader {
    std::span<const std::uint8_t> b;
    std::size_t pos{0};
    bool ok{true};

    std::uint8_t u8() {
        if (pos >= b.size()) {
            ok = false;
            return 0;
        }
        return b[pos++];
    }
    bool boolean() { return u8() != 0; }
    std::uint16_t u16() {
        const std::uint16_t lo = u8();
        const std::uint16_t hi = u8();
        return static_cast<std::uint16_t>(lo | static_cast<std::uint16_t>(hi << 8));
    }
    std::uint32_t u32() {
        const std::uint32_t b0 = u8();
        const std::uint32_t b1 = u8();
        const std::uint32_t b2 = u8();
        const std::uint32_t b3 = u8();
        return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
    }
    float flt() { return std::bit_cast<float>(u32()); }
};

}  // namespace

std::vector<std::uint8_t> encode_settings(const settings::Settings& s) {
    std::vector<std::uint8_t> o;
    o.insert(o.end(), kFullMagic.begin(), kFullMagic.end());
    o.push_back(kFullVersion);

    const settings::GameplaySettings& g = s.gameplay;
    put_u8(o, g.street_weight_preflop);
    put_u8(o, g.street_weight_flop);
    put_u8(o, g.street_weight_turn);
    put_u8(o, g.street_weight_river);
    put_u8(o, g.custom_aggressor_weight);
    put_u8(o, g.custom_caller_weight);
    put_float(o, g.side_pot_frequency);
    put_u8(o, static_cast<std::uint8_t>(g.chip_denomination_mode));
    put_bool(o, g.bet_sizing_engine_enabled);
    put_float(o, g.difficulty_min);
    put_float(o, g.difficulty_max);
    put_bool(o, g.time_pressure_custom_enabled);
    put_u16(o, g.time_pressure_custom_seconds);
    put_bool(o, g.delta_timer_enabled);
    put_bool(o, g.show_hud);
    put_bool(o, g.show_countdown);

    put_bool(o, s.units.cash_mode);

    const settings::DisplaySettings& d = s.display;
    put_u8(o, d.active_theme_id);
    put_bool(o, d.reduce_motion);
    put_bool(o, d.background_atmospheric_movement);
    put_bool(o, d.particle_drift);

    const settings::AudioSettings& a = s.audio;
    put_u8(o, a.volume);
    put_u8(o, static_cast<std::uint8_t>(a.current_music_genre));
    put_bool(o, a.mute_all);
    put_bool(o, a.mute_sfx);
    put_bool(o, a.mute_music);

    const settings::RecapSettings& r = s.recap;
    put_bool(o, r.dealer_arrival_animation);
    put_bool(o, r.transitions_enabled);
    put_u8(o, static_cast<std::uint8_t>(r.default_aggressor_recap_tab));

    put_bool(o, s.tomatoes.shop_button_visible);
    put_bool(o, s.tomatoes.leaderboard_opt_in);

    // display_name_override: u8 length (clamped to the contract max) + bytes.
    const std::string& name = s.account.display_name_override;
    const std::size_t name_len =
        name.size() < settings::kMaxDisplayNameOverrideLength ? name.size()
                                                              : settings::kMaxDisplayNameOverrideLength;
    put_u8(o, static_cast<std::uint8_t>(name_len));
    for (std::size_t i = 0; i < name_len; ++i) {
        put_u8(o, static_cast<std::uint8_t>(name[i]));
    }

    put_bool(o, s.general.confirm_before_leaving_site);

    return o;
}

std::optional<settings::Settings> decode_settings(std::span<const std::uint8_t> blob) noexcept {
    if (blob.size() < kFullMagic.size() + 1) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < kFullMagic.size(); ++i) {
        if (blob[i] != kFullMagic[i]) {
            return std::nullopt;
        }
    }
    if (blob[kFullMagic.size()] != kFullVersion) {
        return std::nullopt;
    }

    Reader rd{blob, kFullMagic.size() + 1, true};
    settings::Settings s{};

    settings::GameplaySettings& g = s.gameplay;
    g.street_weight_preflop = rd.u8();
    g.street_weight_flop = rd.u8();
    g.street_weight_turn = rd.u8();
    g.street_weight_river = rd.u8();
    g.custom_aggressor_weight = rd.u8();
    g.custom_caller_weight = rd.u8();
    g.side_pot_frequency = rd.flt();
    g.chip_denomination_mode = static_cast<settings::ChipDenominationMode>(rd.u8());
    g.bet_sizing_engine_enabled = rd.boolean();
    g.difficulty_min = rd.flt();
    g.difficulty_max = rd.flt();
    g.time_pressure_custom_enabled = rd.boolean();
    g.time_pressure_custom_seconds = rd.u16();
    g.delta_timer_enabled = rd.boolean();
    g.show_hud = rd.boolean();
    g.show_countdown = rd.boolean();

    s.units.cash_mode = rd.boolean();

    settings::DisplaySettings& d = s.display;
    d.active_theme_id = rd.u8();
    d.reduce_motion = rd.boolean();
    d.background_atmospheric_movement = rd.boolean();
    d.particle_drift = rd.boolean();

    settings::AudioSettings& a = s.audio;
    a.volume = rd.u8();
    a.current_music_genre = static_cast<settings::ActiveMusicGenre>(rd.u8());
    a.mute_all = rd.boolean();
    a.mute_sfx = rd.boolean();
    a.mute_music = rd.boolean();

    settings::RecapSettings& r = s.recap;
    r.dealer_arrival_animation = rd.boolean();
    r.transitions_enabled = rd.boolean();
    r.default_aggressor_recap_tab = static_cast<settings::DefaultAggressorRecapTab>(rd.u8());

    s.tomatoes.shop_button_visible = rd.boolean();
    s.tomatoes.leaderboard_opt_in = rd.boolean();

    const std::uint8_t name_len = rd.u8();
    const std::size_t clamped_len =
        name_len < settings::kMaxDisplayNameOverrideLength ? name_len
                                                           : settings::kMaxDisplayNameOverrideLength;
    std::string name;
    name.reserve(clamped_len);
    for (std::size_t i = 0; i < clamped_len; ++i) {
        name.push_back(static_cast<char>(rd.u8()));
    }
    s.account.display_name_override = name;

    s.general.confirm_before_leaving_site = rd.boolean();

    if (!rd.ok) {
        return std::nullopt;  // truncated blob -> reject wholesale
    }
    return s;
}

settings::Settings read_persisted_settings(const persistence::AppState& state) noexcept {
    if (std::optional<settings::Settings> full = decode_settings(state.settings_blob)) {
        return *full;
    }
    // Migrate the interim 'PTS0' (theme + custom split) into a defaults Settings so a
    // returning user's saved theme/weights survive the format change.
    settings::Settings s{};
    if (const std::optional<InterimSettings> interim = decode_interim_settings(state.settings_blob)) {
        s.display.active_theme_id = interim->theme_id;
        s.gameplay.custom_aggressor_weight = interim->custom_weights.aggressor_weight;
        s.gameplay.custom_caller_weight = interim->custom_weights.caller_weight;
    }
    return s;
}

persistence::AppState with_settings(const persistence::AppState& state,
                                    const settings::Settings& s) {
    persistence::AppState next = state;
    next.settings_blob = encode_settings(s);
    return next;
}

}  // namespace poker_trainer::bridge
