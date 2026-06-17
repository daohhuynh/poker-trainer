#include "bridge/tier_schedule.hpp"

#include "assets/asset_paths.hpp"
#include "assets/tier_config.hpp"
#include "audio/audio_paths.hpp"
#include "backbone/screen_state.hpp"

#include <cstdint>

namespace poker_trainer::bridge {

assets::AssetTier sfx_load_tier(audio::SfxId id) noexcept {
    switch (id) {
        case audio::SfxId::ModalSwooshOpen:
        case audio::SfxId::ModalSwooshClose:
            return assets::AssetTier::Tier2;
        case audio::SfxId::CardDeal:
        case audio::SfxId::ButtonClickConfirmation:
        case audio::SfxId::ChipPush:
        case audio::SfxId::SidePotSplit:
        case audio::SfxId::FrogToggle:
        case audio::SfxId::SlideIn:
        case audio::SfxId::SlideOut:
            return assets::AssetTier::Tier3;
    }
    // Unreachable; the switch is exhaustive over SfxId.
    return assets::AssetTier::Tier3;
}

bool is_game_launch_required_asset(assets::AssetId id) noexcept {
    switch (id) {
        case assets::AssetId::BackgroundGame:
        case assets::AssetId::TableFelt:
        case assets::AssetId::CardBack:
            return true;
        default:
            break;
    }
    // The 52 card faces occupy a contiguous AssetId range (CardSpadeA through
    // CardClubK, asset_paths.hpp). The drill table cannot render without them.
    const auto value = static_cast<std::uint16_t>(id);
    return value >= static_cast<std::uint16_t>(assets::AssetId::CardSpadeA) &&
           value <= static_cast<std::uint16_t>(assets::AssetId::CardClubK);
}

bool is_root_to_mode_transition(backbone::ScreenId prev,
                                backbone::ScreenId current) noexcept {
    return prev == backbone::ScreenId::Root &&
           current == backbone::ScreenId::ModeSelection;
}

}  // namespace poker_trainer::bridge
