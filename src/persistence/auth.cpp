#include "persistence/auth.hpp"

#include "persistence/clock.hpp"
#include "persistence/idbfs.hpp"
#include "persistence/migration.hpp"
#include "persistence/sync.hpp"

#include "persistence/auth0_config.hpp"
#include "persistence/persistence_schema.hpp"

#include <chrono>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace poker_trainer::persistence {

AuthManager::AuthManager(AuthBackend& auth, IdbfsStore& store,
                         Migrator& migrator, SyncEngine& sync,
                         SyncBackend& server, Clock& clock) noexcept
    : auth_(auth),
      store_(store),
      migrator_(migrator),
      sync_(sync),
      server_(server),
      clock_(clock) {}

std::expected<void, AuthError> AuthManager::sign_in(
    const AuthCredentials& credentials) {
    std::expected<AuthSession, AuthError> result = auth_.sign_in(credentials);
    if (!result.has_value()) {
        return std::unexpected(result.error());
    }
    establish_session(std::move(result.value()));
    return {};
}

std::expected<void, AuthError> AuthManager::sign_up(
    const AuthCredentials& credentials, std::string_view display_name) {
    std::expected<AuthSession, AuthError> result =
        auth_.sign_up(credentials, display_name);
    if (!result.has_value()) {
        return std::unexpected(result.error());
    }
    establish_session(std::move(result.value()));
    return {};
}

bool AuthManager::restore_session() {
    std::optional<AuthSession> restored = auth_.restore_session();
    if (!restored.has_value()) {
        return false;  // no durable session: stay a guest this load
    }
    // Same post-auth path as an interactive sign-in: pin the identity and
    // reconcile against the (authoritative) server. A returning user lands
    // signed in with the server's state adopted.
    establish_session(std::move(restored.value()));
    return true;
}

std::expected<void, AuthError> AuthManager::sign_out() {
    std::expected<void, AuthError> result = auth_.sign_out();
    if (!result.has_value()) {
        return std::unexpected(result.error());
    }
    // IDBFS state remains intact for offline guest use; only the account
    // linkage and the in-memory session are dropped. Re-close the push gate so
    // a subsequent sign-in must reconcile before any of its writes are pushed.
    store_.update_account(AccountState{});
    session_.reset();
    sync_.reset_session_gate();
    return {};
}

std::expected<void, AuthError> AuthManager::delete_account() {
    if (!store_.state().account.is_authenticated) {
        return std::unexpected(AuthError::NotAuthenticated);
    }
    const std::string user_id = store_.state().account.auth0_user_id;

    // Delete the server-side row FIRST, while the session token is still live —
    // the Auth0 backend's delete clears the in-memory session, after which a
    // Supabase request would carry no bearer and RLS would reject it. Best
    // effort: a failed remote delete (offline) still proceeds to the terminal
    // local wipe, leaving the row to be reaped server-side (see report). The
    // privileged Auth0 user-record deletion remains stubbed.
    static_cast<void>(server_.delete_account_state(user_id));

    std::expected<void, AuthError> result = auth_.delete_account(user_id);
    if (!result.has_value()) {
        return std::unexpected(result.error());
    }
    // Auth0 deletion succeeded: wipe local IDBFS and revert to a fresh guest.
    // Re-close the push gate; the fresh guest has no account to push to.
    store_.wipe();
    session_.reset();
    sync_.reset_session_gate();
    return {};
}

std::expected<void, AuthError> AuthManager::change_password() {
    if (!store_.state().account.is_authenticated) {
        return std::unexpected(AuthError::NotAuthenticated);
    }
    // Auth0 owns the reset flow end to end; Z04 only triggers the email.
    return auth_.change_password(store_.state().account.email);
}

std::expected<void, AuthError> AuthManager::send_password_reset(std::string_view email) {
    // No auth gate: the Sign In "Forgot password?" path runs while logged out. Auth0's
    // change_password endpoint takes any address; it sends the email only if an account
    // exists (and never reveals which), so this is safe to forward verbatim.
    return auth_.change_password(email);
}

bool AuthManager::auth0_health_check() {
    const std::chrono::steady_clock::time_point now = clock_.now();
    if (health_ok_at_.has_value() &&
        now - *health_ok_at_ < kAuth0HealthCheckCacheTtl) {
        return true;  // a recent successful probe is still fresh
    }
    const bool healthy = auth_.health_check();
    if (healthy) {
        health_ok_at_ = now;
    }
    // Failures are never cached: the next modal-open re-probes immediately.
    return healthy;
}

void AuthManager::reconcile_account(std::string_view auth0_user_id) {
    const FetchResult fetched = server_.fetch(auth0_user_id);
    switch (fetched.outcome) {
        case FetchOutcome::Found:
            // Server is the source of truth: IDBFS reconciles to match. The
            // authoritative baseline is now established, so open the push gate.
            store_.adopt_server_state(fetched.state);
            sync_.note_session_reconciled();
            break;
        case FetchOutcome::NotFound: {
            // Brand-new account: seed the server from the guest's local state.
            // A successful seed makes the server authoritative and opens the
            // gate; a failed upload leaves the gate closed and schedules a
            // reconcile retry, which re-attempts the seed.
            const bool seeded =
                migrator_.migrate(store_.state(), auth0_user_id);
            if (seeded) {
                sync_.note_session_reconciled();
            } else {
                sync_.note_session_reconcile_failed();
            }
            break;
        }
        case FetchOutcome::Failed:
            // Offline / server error at session start: keep local state and
            // proceed. The push gate stays closed (no push could clobber the
            // not-yet-fetched authoritative state) and the reconcile is retried
            // per the backoff schedule on the next pump.
            sync_.note_session_reconcile_failed();
            break;
    }
}

void AuthManager::establish_session(AuthSession session) {
    session_ = std::move(session);

    // Persist only the non-sensitive identity; the access token stays in the
    // in-memory session and is never written through the storage seam.
    AccountState account{};
    account.is_authenticated = true;
    account.auth0_user_id = session_->auth0_user_id;
    account.display_name = session_->display_name;
    account.email = session_->email;
    store_.update_account(account);

    // Copy the id before reconciling: a Found outcome replaces the cached
    // state, which would otherwise invalidate a view into store_.state().
    const std::string user_id = session_->auth0_user_id;
    reconcile_account(user_id);
}

}  // namespace poker_trainer::persistence
