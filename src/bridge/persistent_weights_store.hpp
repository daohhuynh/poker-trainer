#pragma once

#include "screens/custom_popup.hpp"

#include "backbone/game_mode.hpp"
#include "persistence/idbfs.hpp"

#include <optional>

// The production CustomWeightsStore (Zone 07's Save/Reset persistence seam),
// backed by Zone 04's IdbfsStore through the interim settings codec. Replaces the
// test FakeStore in the live app: the Custom popup's Save writes the
// custom_*_weight fields durably (IDBFS), and Reset / re-open read them back.
//
// Pure logic (no ImGui / Emscripten): lives in the native-testable bridge lib so
// the Save -> reload -> persist round-trip is unit-tested against an in-memory
// StorageBackend, exactly as it behaves over the real IDBFS backend in the app.

namespace poker_trainer::bridge {

class PersistentCustomWeightsStore final : public screens::CustomWeightsStore {
public:
    explicit PersistentCustomWeightsStore(persistence::IdbfsStore& store) noexcept;

    // Persist the weights into AppState::settings_blob via the interim codec,
    // preserving any already-saved theme id, and write through to IDBFS.
    void save(backbone::CustomConfig weights) override;

    // The persisted weights, or std::nullopt when Save has never run (so Reset
    // falls back to 50/50 per the CustomWeightsStore contract).
    [[nodiscard]] std::optional<backbone::CustomConfig> load() const override;

private:
    persistence::IdbfsStore& store_;
};

}  // namespace poker_trainer::bridge
