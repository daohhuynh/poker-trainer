#include "tutorial/forced_settings.hpp"

#include "settings/settings.hpp"

#include <gtest/gtest.h>

namespace pt = poker_trainer::tutorial;
namespace st = poker_trainer::settings;

TEST(ForcedSettings, ApplySetsHudOnCountdownOffBetSizingOn) {
    st::Settings s{};
    s.gameplay.show_hud = false;
    s.gameplay.show_countdown = true;
    s.gameplay.bet_sizing_engine_enabled = false;

    pt::apply_forced(s);

    EXPECT_TRUE(s.gameplay.show_hud);
    EXPECT_FALSE(s.gameplay.show_countdown);
    EXPECT_TRUE(s.gameplay.bet_sizing_engine_enabled);
}

TEST(ForcedSettings, CaptureApplyRestoreIsSymmetric) {
    // A user state that differs from the forced values in every field.
    st::Settings s{};
    s.gameplay.show_hud = false;
    s.gameplay.show_countdown = true;
    s.gameplay.bet_sizing_engine_enabled = false;

    const pt::SavedSettings saved = pt::capture_forced(s);
    pt::apply_forced(s);
    pt::restore_forced(s, saved);

    EXPECT_FALSE(s.gameplay.show_hud);
    EXPECT_TRUE(s.gameplay.show_countdown);
    EXPECT_FALSE(s.gameplay.bet_sizing_engine_enabled);
}

TEST(ForcedSettings, RestoreIsExactAcrossAllEightCombos) {
    for (int mask = 0; mask < 8; ++mask) {
        st::Settings s{};
        s.gameplay.show_hud = (mask & 1) != 0;
        s.gameplay.show_countdown = (mask & 2) != 0;
        s.gameplay.bet_sizing_engine_enabled = (mask & 4) != 0;

        const pt::SavedSettings saved = pt::capture_forced(s);
        pt::apply_forced(s);
        pt::restore_forced(s, saved);

        EXPECT_EQ(s.gameplay.show_hud, (mask & 1) != 0);
        EXPECT_EQ(s.gameplay.show_countdown, (mask & 2) != 0);
        EXPECT_EQ(s.gameplay.bet_sizing_engine_enabled, (mask & 4) != 0);
    }
}

TEST(ForcedSettings, RestoreDoesNotTouchUnrelatedSettings) {
    st::Settings s{};
    s.display.active_theme_id = 2;       // Ocean
    s.display.reduce_motion = true;
    s.units.cash_mode = false;           // BB mode

    const pt::SavedSettings saved = pt::capture_forced(s);
    pt::apply_forced(s);
    pt::restore_forced(s, saved);

    // Reduce Motion / Theme / Units are not part of the forced set — untouched.
    EXPECT_EQ(s.display.active_theme_id, 2);
    EXPECT_TRUE(s.display.reduce_motion);
    EXPECT_FALSE(s.units.cash_mode);
}
