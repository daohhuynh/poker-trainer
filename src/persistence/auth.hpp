#pragma once

#include <chrono>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace poker_trainer::persistence {

class IdbfsStore;
class Migrator;
class SyncBackend;
class SyncEngine;
class Clock;

// Categorized auth failure. Auth0 owns the actual credential check; these are
// the outcomes the seam surfaces so the form layer (Z11/Z12) can render the
// right message. Z04 never inspects passwords to derive these.
enum class AuthError : std::uint8_t {
    // Sign-in rejected: wrong email/password (Auth0's verdict).
    InvalidCredentials = 0,
    // Sign-up rejected: the email already has an account.
    AccountExists = 1,
    // The request to Auth0 failed in transit (offline / DNS / TLS).
    NetworkError = 2,
    // Auth0 is reachable-but-unhealthy or the health check failed.
    ServiceUnavailable = 3,
    // The operation needs an authenticated session and there is none.
    NotAuthenticated = 4,
    // Any other Auth0-reported failure.
    Unknown = 5,
    // Sign-up rejected: the password failed Auth0's strength / history / dictionary
    // policy (invalid_password, PasswordStrengthError and friends).
    WeakPassword = 6,
    // Sign-up rejected: the email address is malformed (signup payload validation).
    InvalidEmail = 7,
    // Throttled: too many attempts / brute-force protection / HTTP 429.
    RateLimited = 8,
    // The account or request was blocked (access_denied / blocked user / bot detection).
    AccessBlocked = 9,
    // Sign-up rejected: the USERNAME is already taken (Auth0 username_exists). Distinct
    // from AccountExists, which is the EMAIL already being registered.
    UsernameExists = 10,
    // Sign-up rejected generically (invalid_signup) — Auth0's enumeration-masked response
    // when it won't say whether the email or username is the duplicate.
    SignupRejected = 11,
};

// Non-sensitive session identity returned by Auth0 after authentication.
// Only auth0_user_id / display_name / email are ever persisted (into
// AccountState); the access token is held in memory by AuthManager and is
// NEVER written through the storage seam.
struct AuthSession {
    std::string auth0_user_id;  // the "sub" claim
    std::string display_name;
    std::string email;
    std::string access_token;
};

// Transient credentials carried to the auth seam and nowhere else.
//
// CONTRACT: an AuthCredentials value flows input -> AuthManager method ->
// AuthBackend (Auth0) and is then discarded. It is never copied into AppState,
// never persisted, never logged. Auth0 owns credential storage; Z04 only
// forwards the user's input across the seam.
struct AuthCredentials {
    std::string email;
    std::string password;
};

// Auth seam — Auth0.
//
// Deployment wires Auth0's embedded SDK behind this interface (the app renders
// its own forms; Auth0 verifies credentials, stores passwords, manages tokens,
// and sends reset emails). Native tests inject a mock returning healthy /
// unhealthy and success / failure. Z04 never branches on which is installed.
class AuthBackend {
public:
    virtual ~AuthBackend() = default;

    // Probe Auth0 reachability (the well-known JWKS endpoint per
    // auth0_config.hpp). Returns true when healthy. The mock returns its
    // configured health. The real impl enforces kAuth0HealthCheckTimeout.
    [[nodiscard]] virtual bool health_check() noexcept = 0;

    [[nodiscard]] virtual std::expected<AuthSession, AuthError> sign_in(
        const AuthCredentials& credentials) = 0;

    // display_name has already passed the form-level username denylist check
    // (Account Creation Flow); Z04 does not re-validate it (see report).
    [[nodiscard]] virtual std::expected<AuthSession, AuthError> sign_up(
        const AuthCredentials& credentials, std::string_view display_name) = 0;

    [[nodiscard]] virtual std::expected<void, AuthError> sign_out() = 0;

    [[nodiscard]] virtual std::expected<void, AuthError> delete_account(
        std::string_view auth0_user_id) = 0;

    // Trigger Auth0's password-reset email. Z04 never sees or sets the new
    // password — Auth0 owns the reset flow end to end.
    [[nodiscard]] virtual std::expected<void, AuthError> change_password(
        std::string_view email) = 0;
};

// Auth-flow orchestrator.
//
// Owns the auth seam plus the in-memory session (token), and coordinates the
// full guest<->logged-in transition: on sign-in / sign-up it establishes the
// account, then either adopts authoritative server state or migrates the
// guest's local state up as the initial account state; on sign-out it drops
// the session but leaves IDBFS intact for guest use; on delete it asks Auth0
// to delete the account and wipes IDBFS back to a fresh guest.
class AuthManager {
public:
    AuthManager(AuthBackend& auth, IdbfsStore& store, Migrator& migrator,
                SyncEngine& sync, SyncBackend& server, Clock& clock) noexcept;

    [[nodiscard]] std::expected<void, AuthError> sign_in(
        const AuthCredentials& credentials);

    [[nodiscard]] std::expected<void, AuthError> sign_up(
        const AuthCredentials& credentials, std::string_view display_name);

    [[nodiscard]] std::expected<void, AuthError> sign_out();

    [[nodiscard]] std::expected<void, AuthError> delete_account();

    [[nodiscard]] std::expected<void, AuthError> change_password();

    // Logged-out password reset: forward an arbitrary email to Auth0's reset flow with NO
    // authenticated-session requirement (the Sign In "Forgot password?" path). change_
    // password() resets the current account; this resets whatever address the user typed.
    // Z04 never sees the new password — Auth0 owns the reset end to end.
    [[nodiscard]] std::expected<void, AuthError> send_password_reset(std::string_view email);

    // Auth0 health check with the success-result caching mandated by
    // auth0_config.hpp (kAuth0HealthCheckCacheTtl). A cached healthy result
    // short-circuits repeated modal-open checks; failures are never cached, so
    // the next attempt re-probes immediately. This is Z04's auth0_health_check
    // export; Z11/Z12/Z14 call it before opening auth-dependent modals.
    [[nodiscard]] bool auth0_health_check();

    // Reconcile the current authenticated account against the server. Used by
    // both the sign-in/up transition and session-start reconciliation:
    // Found -> adopt server state; NotFound -> migrate local up as initial;
    // Failed -> leave local state and let the sync subsystem retry. Drives the
    // SyncEngine's session-start gate: a successful reconcile (adopt, or a
    // brand-new account seeded) opens it; a failed one keeps it closed and
    // schedules a reconcile retry.
    void reconcile_account(std::string_view auth0_user_id);

private:
    // Shared post-authentication path for sign-in and sign-up: stash the
    // in-memory session, pin the local account identity, and reconcile.
    void establish_session(AuthSession session);

    AuthBackend& auth_;
    IdbfsStore& store_;
    Migrator& migrator_;
    SyncEngine& sync_;
    SyncBackend& server_;
    Clock& clock_;

    std::optional<AuthSession> session_;  // in-memory only; never persisted

    // Cached health-check result: the time of the last successful probe.
    std::optional<std::chrono::steady_clock::time_point> health_ok_at_;
};

}  // namespace poker_trainer::persistence
