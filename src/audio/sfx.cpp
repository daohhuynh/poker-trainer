#include "audio/audio.hpp"

#include "audio/audio_engine.hpp"
#include "audio/audio_paths.hpp"
#include "audio/backend.hpp"

namespace poker_trainer::audio {

void play_sfx(SfxId id, float gain) {
    const float out = audio_engine().sfx_gain(gain);
    if (out <= 0.0f) {
        return;  // gesture gate closed, muted, or zero volume -> silent
    }
    backend::sfx_play(sfx_path(id), out);
}

}  // namespace poker_trainer::audio
