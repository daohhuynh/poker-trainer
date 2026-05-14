#pragma once

#include <chrono>
#include <string_view>

namespace poker_trainer::persistence {

// The Auth0 tenant domain. Set at compile time so the auth bridge
// never has to query for it. Tenant migrations require a rebuild.
inline constexpr std::string_view kAuth0Domain =
    "poker-trainer.us.auth0.com";

// The Auth0 client ID for the trainer's Single Page Application. Tied
// to the Auth0 tenant above. Public — Auth0 client IDs are not secrets
// in SPA flows; the security model relies on the redirect URI allowlist
// configured in the Auth0 dashboard.
inline constexpr std::string_view kAuth0ClientId =
    "REPLACE_WITH_ACTUAL_CLIENT_ID";

// The redirect URI Auth0 sends the user back to after authentication.
// Must exactly match an entry in the Auth0 application's "Allowed
// Callback URLs" setting. The production value is the CDN deployment
// URL; for local development, a separate Auth0 application with a
// localhost callback may be used (out of scope for Phase 0).
inline constexpr std::string_view kAuth0RedirectUri =
    "https://app.poker-trainer.com/callback";

// The audience identifier for the trainer's backend API. Auth0 issues
// access tokens with this audience claim so the backend can validate
// they were minted for this application. The leaderboard backend
// validates the audience claim before accepting any request.
inline constexpr std::string_view kAuth0Audience =
    "https://api.poker-trainer.com";

// The scopes requested at sign-in. "openid" enables OIDC, "profile"
// gives access to user display name, "email" gives access to email
// address. No other scopes are requested; the trainer does not need
// elevated permissions.
inline constexpr std::string_view kAuth0Scopes = "openid profile email";

// The URL hit by the Auth0 health check. Returns 200 when Auth0 is
// reachable and operational. The check is fired before opening any
// modal that depends on Auth0 (Sign In, Sign Up, Forgot Password,
// Delete Account). On failure, the Service Outage Banner is triggered
// instead of opening the modal.
//
// Using the well-known JWKS endpoint because it is publicly cacheable
// and a fast 200 response indicates the tenant is reachable.
inline constexpr std::string_view kAuth0HealthCheckUrl =
    "https://poker-trainer.us.auth0.com/.well-known/jwks.json";

// The timeout for the health check request. If the health check has
// not returned within this duration, the check is considered failed
// and the outage banner is triggered.
inline constexpr std::chrono::milliseconds kAuth0HealthCheckTimeout{3000};

// The duration the health check result is cached. After a successful
// health check, subsequent auth-dependent modal opens within this
// window skip the check and proceed directly to opening the modal.
// This avoids hammering the health check endpoint when the user
// opens multiple auth modals in quick succession.
inline constexpr std::chrono::seconds kAuth0HealthCheckCacheTtl{30};

}  // namespace poker_trainer::persistence
