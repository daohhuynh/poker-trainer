#pragma once

#include "persistence/persistence_schema.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace poker_trainer::persistence {

// Storage seam — raw byte-blob persistence.
//
// In deployment this is backed by Emscripten's IDBFS: a file under the
// IDBFS-mounted directory holds the serialized AppState blob, and writes are
// flushed to IndexedDB via FS.syncfs. Native unit tests inject an in-memory
// backend so the serialization / reconciliation / migration logic is
// exercised without a browser. Z04 never branches on which backend is
// installed — the backend is constructor-injected and swapped at this seam.
//
// The seam deals only in opaque bytes. Serialization of AppState into and out
// of those bytes is Z04's own logic (serialize_app_state / deserialize_app_
// state), which is why it stays unit-testable against the in-memory backend.
class StorageBackend {
public:
    virtual ~StorageBackend() = default;

    // Read the persisted blob. std::nullopt when nothing has ever been
    // persisted (first launch) or the underlying store is empty.
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> read()
        const = 0;

    // Persist the blob, replacing any prior contents. In deployment this
    // writes the IDBFS file and schedules an FS.syncfs flush.
    virtual void write(std::span<const std::uint8_t> bytes) = 0;

    // Erase all persisted state. Used by the delete-account local wipe.
    virtual void clear() = 0;
};

// Serialize an AppState into the on-disk blob format: a magic tag, the
// current schema version, the field payload, and a trailing FNV-1a checksum
// for corruption detection. Always writes kCurrentSchemaVersion regardless of
// the value in state.schema_version.
[[nodiscard]] std::vector<std::uint8_t> serialize_app_state(
    const AppState& state);

// Outcome of deserializing a blob produced by serialize_app_state.
struct DeserializeResult {
    // Ok / NeedsMigration / Unsupported / Corrupt. On Ok the state field is
    // fully populated; on any other value the state field is left default and
    // callers fall back to a fresh AppState.
    SchemaValidationResult result{SchemaValidationResult::Corrupt};

    // The reconstructed state. Meaningful only when result == Ok.
    AppState state{};
};

// Deserialize a blob. Verifies the magic tag and checksum (mismatch ->
// Corrupt), validates the schema version against the supported range, and on
// a supported current-version blob reconstructs the AppState losslessly. A
// truncated or structurally invalid payload yields Corrupt rather than
// undefined behavior.
[[nodiscard]] DeserializeResult deserialize_app_state(
    std::span<const std::uint8_t> bytes);

// IDBFS-backed application-state store.
//
// Owns the authoritative in-memory copy of AppState and mediates every read /
// write against the storage seam. State changes write through to the seam
// immediately (the spec's "IDBFS writes immediately on state change"); the
// separate background sync to the server is orchestrated by SyncEngine.
class IdbfsStore {
public:
    explicit IdbfsStore(StorageBackend& storage) noexcept;

    // Load persisted state into the cache and return a copy. A missing blob
    // (first launch) or an unreadable / corrupt / unsupported blob yields a
    // fresh default AppState — a damaged local blob never bricks the app
    // (logged-in users repopulate from the server during reconciliation).
    AppState load_state();

    // Persist a full state. Updates the cache and writes through to the seam
    // immediately. This is the entry point Wave-2+ zones call after mutating
    // wallet / unlocks / shuffle pool / settings / history.
    void save_state(const AppState& state);

    // The current authoritative in-memory state.
    [[nodiscard]] const AppState& state() const noexcept { return state_; }

    // Adopt server state as authoritative (the "IDBFS reconciles to match"
    // mechanic). Replaces every field with the server's values except the
    // account linkage, which stays pinned to the locally authenticated
    // identity, and normalizes the schema version. Writes through immediately.
    void adopt_server_state(const AppState& server_state);

    // Replace only the account-linkage fields and write through. Used by the
    // auth flow on sign-in / sign-out without disturbing wallet / unlocks.
    void update_account(const AccountState& account);

    // Local wipe: clear the persisted blob and reset the cache to a fresh
    // guest-mode default. Used by the delete-account flow.
    void wipe();

    // has_seen_tutorial_prompt lifecycle: set once when the first-launch
    // "Take the tutorial?" prompt has been shown (accepted or skipped), so it
    // never re-appears. Writes through immediately.
    [[nodiscard]] bool has_seen_tutorial_prompt() const noexcept {
        return state_.tutorial.has_seen_tutorial_prompt;
    }
    void mark_tutorial_prompt_seen();

    // has_completed_tutorial lifecycle (set by Z14 on reaching the Tutorial
    // Complete screen; persisted here). Writes through immediately.
    [[nodiscard]] bool has_completed_tutorial() const noexcept {
        return state_.tutorial.has_completed_tutorial;
    }
    void mark_tutorial_completed();

private:
    void persist();  // serialize the cache and write through to the seam

    StorageBackend& storage_;
    AppState state_{};
};

}  // namespace poker_trainer::persistence
