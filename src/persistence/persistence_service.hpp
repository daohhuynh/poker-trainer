#pragma once

#include "persistence/auth.hpp"
#include "persistence/clock.hpp"
#include "persistence/idbfs.hpp"
#include "persistence/migration.hpp"
#include "persistence/persistence_schema.hpp"
#include "persistence/sync.hpp"

#include <expected>

namespace poker_trainer::persistence {

// Zone 04 facade — the single persistence-layer entry point other zones hold.
//
// Composes the four collaborators (IdbfsStore, SyncEngine, Migrator,
// AuthManager) over the three injected seams (storage, auth, sync) plus the
// clock, and exposes the ZONES.md exports. Z05 constructs one of these during
// boot with the production seams; native tests construct one with mocks. It is
// not a singleton — its lifetime is owned explicitly by whoever constructs it.
class PersistenceService {
public:
    PersistenceService(StorageBackend& storage, AuthBackend& auth,
                       SyncBackend& server, Clock& clock) noexcept;

    // Non-copyable and non-movable: auth_ holds references to the sibling
    // store_ and migrator_ members, so copying or moving the facade would
    // leave those references dangling at the source object. Construct it in
    // place (guaranteed copy elision still allows `auto s = make(...)`).
    PersistenceService(const PersistenceService&) = delete;
    PersistenceService& operator=(const PersistenceService&) = delete;
    PersistenceService(PersistenceService&&) = delete;
    PersistenceService& operator=(PersistenceService&&) = delete;

    // --- IDBFS state ---

    // Load persisted state at boot (fresh-default fallback on corrupt/missing).
    AppState load_state();

    // Persist a state change: writes IDBFS immediately, then (for logged-in
    // users) queues a background server sync.
    void save_state(const AppState& state);

    [[nodiscard]] const AppState& state() const noexcept;

    // Session-start reconciliation: for a logged-in user the server is the
    // source of truth and IDBFS reconciles to match (a never-seeded account
    // is seeded from local state instead). A no-op for guests.
    void reconcile_on_session_start();

    // --- Auth (all via the auth seam) ---

    [[nodiscard]] std::expected<void, AuthError> sign_in(
        const AuthCredentials& credentials);
    [[nodiscard]] std::expected<void, AuthError> sign_up(
        const AuthCredentials& credentials, std::string_view display_name);
    [[nodiscard]] std::expected<void, AuthError> sign_out();
    [[nodiscard]] std::expected<void, AuthError> delete_account();
    [[nodiscard]] std::expected<void, AuthError> change_password();

    // Auth0 health check (cached per kAuth0HealthCheckCacheTtl). Callers:
    // Z11/Z12/Z14 before opening auth-dependent modals.
    [[nodiscard]] bool auth0_health_check();

    // Upload the current local guest state to the current account as the
    // initial account state. Normally driven automatically by sign-in/up;
    // exposed per the ZONES.md export. Returns false if not authenticated or
    // the upload fails.
    [[nodiscard]] bool migrate_guest_to_account();

    // --- Sync ---

    // Drive the sync retry schedule (called each frame by Z05's main loop).
    void pump_sync();

    // --- Tutorial flags ---

    [[nodiscard]] bool has_seen_tutorial_prompt() const noexcept;
    void mark_tutorial_prompt_seen();
    [[nodiscard]] bool has_completed_tutorial() const noexcept;
    void mark_tutorial_completed();

private:
    IdbfsStore store_;
    SyncEngine sync_;
    Migrator migrator_;
    AuthManager auth_;
};

}  // namespace poker_trainer::persistence
