#include "bridge/settings_persistence.hpp"

#include "backbone/game_mode.hpp"
#include "persistence/persistence_schema.hpp"
#include "theme/theme_tokens.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
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
    const std::optional<InterimSettings> decoded = decode_interim_settings(state.settings_blob);
    return decoded.has_value() ? decoded->theme_id : theme::kThemeIdNoLimit;
}

std::optional<backbone::CustomConfig> read_persisted_custom_weights(
    const persistence::AppState& state) noexcept {
    const std::optional<InterimSettings> decoded = decode_interim_settings(state.settings_blob);
    if (!decoded.has_value()) {
        return std::nullopt;
    }
    return decoded->custom_weights;
}

persistence::AppState with_interim_settings(const persistence::AppState& state,
                                            const InterimSettings& settings) {
    persistence::AppState next = state;
    next.settings_blob = encode_interim_settings(settings);
    return next;
}

}  // namespace poker_trainer::bridge
