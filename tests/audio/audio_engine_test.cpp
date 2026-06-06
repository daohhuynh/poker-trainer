// Zone 03 output-gain math: the autoplay gesture gate (no playback before the
// first gesture) and the volume / mute hierarchy (mute_all vs mute_music vs
// mute_sfx, composed with the global volume and per-call gain).

#include "audio/audio_engine.hpp"

#include <gtest/gtest.h>

namespace {

using poker_trainer::audio::AudioEngine;
using poker_trainer::audio::kDefaultVolume;

constexpr float kEps = 1e-5f;

TEST(AudioEngine, DefaultState) {
    AudioEngine engine{1};
    EXPECT_EQ(engine.master_volume, kDefaultVolume);  // 50
    EXPECT_FALSE(engine.mute_all);
    EXPECT_FALSE(engine.mute_music);
    EXPECT_FALSE(engine.mute_sfx);
    EXPECT_FALSE(engine.gate_open());
}

TEST(AudioEngine, GateClosedBeforeGestureSilencesEverything) {
    AudioEngine engine{1};
    // No on_first_user_gesture yet -> both channels gated to silence.
    EXPECT_NEAR(engine.sfx_gain(1.0f), 0.0f, kEps);
    EXPECT_NEAR(engine.music_gain(), 0.0f, kEps);
}

TEST(AudioEngine, GainsApplyAfterGesture) {
    AudioEngine engine{1};
    engine.gesture_started = true;  // gate open
    EXPECT_NEAR(engine.sfx_gain(1.0f), 0.5f, kEps);   // master 50 -> 0.5
    EXPECT_NEAR(engine.music_gain(), 0.5f, kEps);
}

TEST(AudioEngine, PerCallGainMultiplies) {
    AudioEngine engine{1};
    engine.gesture_started = true;
    EXPECT_NEAR(engine.sfx_gain(0.70f), 0.5f * 0.70f, kEps);  // the choreography 70%
}

TEST(AudioEngine, MuteAllSilencesBothChannels) {
    AudioEngine engine{1};
    engine.gesture_started = true;
    engine.mute_all = true;
    EXPECT_NEAR(engine.sfx_gain(1.0f), 0.0f, kEps);
    EXPECT_NEAR(engine.music_gain(), 0.0f, kEps);
}

TEST(AudioEngine, MuteMusicSilencesMusicOnly) {
    AudioEngine engine{1};
    engine.gesture_started = true;
    engine.mute_music = true;
    EXPECT_NEAR(engine.music_gain(), 0.0f, kEps);
    EXPECT_NEAR(engine.sfx_gain(1.0f), 0.5f, kEps);  // SFX unaffected
}

TEST(AudioEngine, MuteSfxSilencesSfxOnly) {
    AudioEngine engine{1};
    engine.gesture_started = true;
    engine.mute_sfx = true;
    EXPECT_NEAR(engine.sfx_gain(1.0f), 0.0f, kEps);
    EXPECT_NEAR(engine.music_gain(), 0.5f, kEps);  // music unaffected
}

TEST(AudioEngine, SetVolumeClampsTo0_100) {
    AudioEngine engine{1};
    engine.set_volume(150);
    EXPECT_EQ(engine.master_volume, 100);
    engine.set_volume(-10);
    EXPECT_EQ(engine.master_volume, 0);
    engine.set_volume(73);
    EXPECT_EQ(engine.master_volume, 73);
}

TEST(AudioEngine, VolumeScalesGain) {
    AudioEngine engine{1};
    engine.gesture_started = true;
    engine.set_volume(100);
    EXPECT_NEAR(engine.music_gain(), 1.0f, kEps);
    EXPECT_NEAR(engine.sfx_gain(1.0f), 1.0f, kEps);
    engine.set_volume(0);
    EXPECT_NEAR(engine.music_gain(), 0.0f, kEps);   // zero volume -> silence
    EXPECT_NEAR(engine.sfx_gain(1.0f), 0.0f, kEps);
}

}  // namespace
