#pragma once

#include "persistence/auth.hpp"

#include <expected>
#include <optional>
#include <string_view>

// Zone 04 — the PRODUCTION Auth0 AuthBackend (embedded login). Implements the same
// AuthBackend seam the tests' MockAuthBackend implements, bridging C++ to the browser
// via Emscripten. Per the spec the app's own modals collect credentials and pass them
// here; Auth0's hosted UI is used ONLY for the password-reset page (the reset link in
// the email). The JS bridge (auth0_backend.cpp) talks to the Auth0 Authentication API
// directly — Resource-Owner-Password-Realm for login, /dbconnections for signup and the
// reset email — so there is no redirect that would destroy the wasm app's in-memory
// state, and no external SDK or host-page glue is required (it is self-contained in the
// wasm bundle). Value-returning operations use a synchronous XMLHttpRequest so the
// synchronous AuthBackend contract holds without Asyncify; fire-and-forget operations
// (reset email, sign-out) use async fetch.
//
// The Auth0Backend class itself is compiled only under __EMSCRIPTEN__ (it is pure DOM
// glue). The bridge-code -> AuthError mapping below is the testable boundary contract and
// compiles natively too.

namespace poker_trainer::persistence {

// The integer result codes the JS bridge returns across the EM_JS boundary. Keeping them
// as a named contract (rather than magic ints) is what makes the boundary unit-testable:
// the JS side promises to return exactly these, and auth0_code_to_error maps them back to
// the categorized AuthError the form layer renders.
enum class Auth0BridgeCode : int {
    Success = 0,
    InvalidCredentials = 1,  // wrong email/password (token: invalid_grant / 401 / 403)
    NetworkError = 2,        // request never completed (offline / DNS / TLS / XHR throw)
    ServiceUnavailable = 3,  // Auth0 reachable-but-unhealthy (5xx / health fail)
    AccountExists = 4,       // signup: user_exists / username_exists / invalid_signup
    Unknown = 5,             // any other Auth0-reported failure
    WeakPassword = 6,        // signup: invalid_password / Password*Error
    InvalidEmail = 7,        // signup: malformed email (payload validation)
    RateLimited = 8,         // HTTP 429 / too_many_attempts / too_many_requests
    AccessBlocked = 9,       // access_denied / unauthorized / blocked user / bot detection
    UsernameExists = 10,     // signup: username_exists (the USERNAME is taken)
    SignupRejected = 11,     // signup: invalid_signup (enumeration-masked email-or-username)
};

// Map a bridge code to a categorized AuthError. Code 0 (Success) has no error; callers
// must check success before translating, so Success folds to Unknown defensively.
[[nodiscard]] AuthError auth0_code_to_error(int code) noexcept;

#ifdef __EMSCRIPTEN__

// Production Auth0 backend. Constructed once at boot; holds no per-call state (the JS
// side stashes the active session on the Emscripten Module). Embeds no client secret and
// no Management API token — only the already-public SPA client id (from auth0_config.hpp).
class Auth0Backend final : public AuthBackend {
public:
    Auth0Backend() noexcept;

    [[nodiscard]] bool health_check() noexcept override;

    [[nodiscard]] std::expected<AuthSession, AuthError> sign_in(
        const AuthCredentials& credentials) override;

    // Stay-signed-in: redeem a persisted Auth0 refresh token (synchronous
    // refresh_token grant) for fresh tokens and re-derive the identity from the
    // new id_token. std::nullopt when no refresh token is stored or the silent
    // refresh failed (expired / revoked -> the token is dropped; transient ->
    // kept for the next load). No credentials, no user interaction.
    [[nodiscard]] std::optional<AuthSession> restore_session() override;

    [[nodiscard]] std::expected<AuthSession, AuthError> sign_up(
        const AuthCredentials& credentials, std::string_view display_name) override;

    [[nodiscard]] std::expected<void, AuthError> sign_out() override;

    [[nodiscard]] std::expected<void, AuthError> delete_account(
        std::string_view auth0_user_id) override;

    [[nodiscard]] std::expected<void, AuthError> change_password(
        std::string_view email) override;
};

#endif  // __EMSCRIPTEN__

}  // namespace poker_trainer::persistence
