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
    // Preserve a previously-saved theme id (if any); only the weights change.
    const persistence::AppState& current = store_.state();
    InterimSettings settings{};
    if (const std::optional<InterimSettings> existing =
            decode_interim_settings(current.settings_blob)) {
        settings = *existing;
    }
    settings.custom_weights = weights;
    // Write through to IDBFS immediately (IdbfsStore::save_state persists).
    store_.save_state(with_interim_settings(current, settings));
}

std::optional<backbone::CustomConfig> PersistentCustomWeightsStore::load() const {
    return read_persisted_custom_weights(store_.state());
}

}  // namespace poker_trainer::bridge
