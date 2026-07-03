#include "tutorial/forced_settings.hpp"

#include "settings/settings.hpp"

namespace poker_trainer::tutorial {

SavedSettings capture_forced(const settings::Settings& live) noexcept {
    return SavedSettings{
        .show_hud = live.gameplay.show_hud,
        .show_countdown = live.gameplay.show_countdown,
        .bet_sizing_enabled = live.gameplay.bet_sizing_engine_enabled,
    };
}

void apply_forced(settings::Settings& live) noexcept {
    live.gameplay.show_hud = true;                  // HUD forced ON
    live.gameplay.show_countdown = false;           // countdown forced OFF
    live.gameplay.bet_sizing_engine_enabled = true;  // multi-tier forced ON
}

void restore_forced(settings::Settings& live, const SavedSettings& saved) noexcept {
    live.gameplay.show_hud = saved.show_hud;
    live.gameplay.show_countdown = saved.show_countdown;
    live.gameplay.bet_sizing_engine_enabled = saved.bet_sizing_enabled;
}

}  // namespace poker_trainer::tutorial
