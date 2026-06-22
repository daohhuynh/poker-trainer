#include "bridge/persistent_weights_store.hpp"

#include "bridge/settings_persistence.hpp"

#include "backbone/game_mode.hpp"
#include "persistence/idbfs.hpp"
#include "persistence/persistence_schema.hpp"

#include <optional>

namespace poker_trainer::bridge {

PersistentCustomWeightsStore::PersistentCustomWeightsStore(persistence::IdbfsStore& store) noexcept
    : store_(store) {}

void PersistentCustomWeightsStore::save(backbone::CustomConfig weights) {
    // Read-modify-write the FULL settings ('PTS1'): read_persisted_settings preserves
    // every other field (theme, audio, etc.) — and migrates a legacy interim blob —
    // so saving the Custom split never clobbers the rest. Only the custom_*_weight
    // fields change here. Write through to IDBFS immediately (save_state persists).
    const persistence::AppState& current = store_.state();
    settings::Settings s = read_persisted_settings(current);
    s.gameplay.custom_aggressor_weight = weights.aggressor_weight;
    s.gameplay.custom_caller_weight = weights.caller_weight;
    store_.save_state(with_settings(current, s));
}

std::optional<backbone::CustomConfig> PersistentCustomWeightsStore::load() const {
    return read_persisted_custom_weights(store_.state());
}

}  // namespace poker_trainer::bridge
