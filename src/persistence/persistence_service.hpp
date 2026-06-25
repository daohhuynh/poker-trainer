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
    // users whose session-start reconcile has succeeded) queues a background
    // server sync. Before that reconcile the write stays durable in IDBFS but
    // is not pushed — the server is the source of truth on reconciliation.
    void save_state(const AppState& state);

    [[nodiscard]] const AppState& state() const noexcept;

    // The owned IDBFS store, for app-root collaborators that need the raw store handle
    // (e.g. Z05's persistence-backed Custom-weights store). The service is the single
    // owner of the store; this hands out a reference, not ownership.
    [[nodiscard]] IdbfsStore& store() noexcept { return store_; }

    // Session-start reconciliation: for a logged-in user the server is the
    // source of truth and IDBFS reconciles to match (a never-seeded account
    // is seeded from local state instead). A no-op for guests.
    void reconcile_on_session_start();

    // Stay-signed-in: at boot, silently restore a durable Auth0 session (refresh
    // token) and reconcile against the server — the same path as an interactive
    // sign-in. Returns true when a session was restored (the user is now signed
    // in and the server state adopted); false when there was nothing to restore
    // (the user stays a guest). Boot calls this once before building the live
    // settings snapshot so an adopted server state is reflected from the first
    // frame. Never throws and never surfaces an auth error — a failed restore is
    // simply "guest this load".
    [[nodiscard]] bool try_restore_session();

    // --- Auth (all via the auth seam) ---

    [[nodiscard]] std::expected<void, AuthError> sign_in(
        const AuthCredentials& credentials);
    [[nodiscard]] std::expected<void, AuthError> sign_up(
        const AuthCredentials& credentials, std::string_view display_name);
    [[nodiscard]] std::expected<void, AuthError> sign_out();
    [[nodiscard]] std::expected<void, AuthError> delete_account();
    [[nodiscard]] std::expected<void, AuthError> change_password();

    // Logged-out password reset: trigger Auth0's reset email for an arbitrary address
    // WITHOUT an authenticated session (the Sign In screen's "Forgot password?"). Distinct
    // from change_password(), which resets the CURRENT account's email. Auth0 owns the
    // reset flow end to end; Z04 only triggers the email.
    [[nodiscard]] std::expected<void, AuthError> send_password_reset(std::string_view email);

    // Auth0 health check (cached per kAuth0HealthCheckCacheTtl). Callers:
    // Z11/Z12/Z14 before opening auth-dependent modals.
    [[nodiscard]] bool auth0_health_check();

    // Upload the current local guest state to the current account as the
    // initial account state. Normally driven automatically by sign-in/up;
    // exposed per the ZONES.md export. Returns false if not authenticated or
    // the upload fails.
    [[nodiscard]] bool migrate_guest_to_account();

    // --- Sync ---

    // Drive the sync schedule (called each frame by Z05's main loop). While the
    // session-start reconcile has not yet succeeded, retries that reconcile per
    // the backoff schedule; once it has, drives the pending-push retry.
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
