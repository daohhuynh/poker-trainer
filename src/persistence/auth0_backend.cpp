#include "persistence/auth0_backend.hpp"

#include "persistence/auth.hpp"

namespace poker_trainer::persistence {

// Native + wasm: the testable boundary contract. The JS bridge promises to return exactly
// these codes; this is the single place they become AuthError.
AuthError auth0_code_to_error(int code) noexcept {
    switch (static_cast<Auth0BridgeCode>(code)) {
        case Auth0BridgeCode::InvalidCredentials:
            return AuthError::InvalidCredentials;
        case Auth0BridgeCode::NetworkError:
            return AuthError::NetworkError;
        case Auth0BridgeCode::ServiceUnavailable:
            return AuthError::ServiceUnavailable;
        case Auth0BridgeCode::AccountExists:
            return AuthError::AccountExists;
        case Auth0BridgeCode::WeakPassword:
            return AuthError::WeakPassword;
        case Auth0BridgeCode::InvalidEmail:
            return AuthError::InvalidEmail;
        case Auth0BridgeCode::RateLimited:
            return AuthError::RateLimited;
        case Auth0BridgeCode::AccessBlocked:
            return AuthError::AccessBlocked;
        case Auth0BridgeCode::UsernameExists:
            return AuthError::UsernameExists;
        case Auth0BridgeCode::SignupRejected:
            return AuthError::SignupRejected;
        case Auth0BridgeCode::Success:
        case Auth0BridgeCode::Unknown:
            break;
    }
    return AuthError::Unknown;
}

}  // namespace poker_trainer::persistence

#ifdef __EMSCRIPTEN__

#include "persistence/auth0_config.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <emscripten/emscripten.h>

// clang-format off
// NOTE: every JS string literal below is DOUBLE-quoted. Single quotes inside an EM_JS body
// are tokenized by the C preprocessor as char constants (-Winvalid-pp-token / -Wmultichar),
// which the -Werror baseline would reject; double quotes are valid C string literals and
// valid JS, and EM_JS escapes them correctly through stringification.

// One-time JS helper setup: a UTF-8-safe JWT-claims decoder and the HTTP-status -> bridge-
// code mapper, installed on globalThis so every operation function can share them.
EM_JS(void, pt_auth0_init, (), {
    if (globalThis.ptAuth0Ready) { return; }
    globalThis.ptAuth0Ready = true;
    globalThis.ptAuth0DecodeJwt = function (t) {
        try {
            var seg = (t || "").split(".")[1];
            if (!seg) { return {}; }
            seg = seg.replace(/-/g, "+").replace(/_/g, "/");
            var pad = seg.length % 4; if (pad) { seg += "=".repeat(4 - pad); }
            var bin = atob(seg);
            var bytes = Uint8Array.from(bin, function (c) { return c.charCodeAt(0); });
            return JSON.parse(new TextDecoder().decode(bytes));
        } catch (e) { return {}; }
    };
    // Map an Auth0 Authentication-API failure to an Auth0BridgeCode. `code` is the JSON
    // `error` (token endpoint) or `code` (dbconnections); `name` is the dbconnections
    // error name (e.g. PasswordStrengthError, BadRequestError); `desc` is the human
    // description/message. Order matters: more specific codes win.
    globalThis.ptAuth0MapStatus = function (status, code, name, desc) {
        code = code || ""; name = name || ""; desc = desc || "";
        if (status === 0) { return 2; }                                  // network
        if (status === 429 || code === "too_many_attempts" ||
            code === "too_many_requests") { return 8; }                  // rate limited
        if (status >= 500) { return 3; }                                 // service
        if (code === "invalid_password" || name.indexOf("Password") === 0) { return 6; }  // weak pw
        if (code === "username_exists") { return 10; }                   // username taken
        if (code === "user_exists") { return 4; }                        // email already registered
        if (code === "invalid_signup") { return 11; }                    // enumeration-masked dup
        if (code === "invalid_grant" || code === "wrong_credentials" ||
            code === "mfa_required" || status === 401 || status === 403) { return 1; }  // bad creds
        if (code === "access_denied" || code === "unauthorized" ||
            code === "unauthorized_client" || code === "blocked" ||
            code === "user_blocked" || code === "extensibility_error") { return 9; }  // blocked
        if (name === "BadRequestError" && /email/i.test(desc)) { return 7; }  // invalid email
        if (/email/i.test(desc) && /(valid|format)/i.test(desc)) { return 7; }  // invalid email
        if (code === "invalid_request" && /email/i.test(desc)) { return 7; }  // invalid email
        return 5;                                                        // unknown
    };
    // Stash a token-endpoint response as the live session. Decodes identity from the
    // id_token and keeps BOTH tokens: `token` (access token) for any audience-gated API,
    // and `id_token` — the bearer Supabase's Auth0 third-party integration validates (the
    // `role` claim must ride the id_token; Auth0 strips it from access tokens). A returned
    // refresh_token is persisted (rotation-aware) so the session survives a reload.
    globalThis.ptAuth0StashSession = function (d) {
        var c = globalThis.ptAuth0DecodeJwt(d.id_token || "");
        globalThis.ptAuth0Session = {
            sub: c.sub || "",
            name: c.name || c.nickname || c.preferred_username ||
                  (c.email ? String(c.email).split("@")[0] : "") || "",
            email: c.email || "",
            token: d.access_token || "",
            id_token: d.id_token || ""
        };
        try {
            if (d.refresh_token) { localStorage.setItem("pt.auth0.rt", d.refresh_token); }
        } catch (e) {}
    };
    // Drop the live session and the durable refresh token (sign-out / delete).
    globalThis.ptAuth0ClearSession = function () {
        globalThis.ptAuth0Session = null;
        try { localStorage.removeItem("pt.auth0.rt"); } catch (e) {}
    };
});

// Sync ROPG (password-realm grant) login. On success stashes the session on globalThis and
// returns 0; otherwise returns a bridge code. The `aud` (audience) is the registered Auth0
// API identifier: requesting it makes Auth0 mint a JWT access token (a custom backend can
// validate the audience), AND the openid+offline_access scope yields the id_token + refresh
// token the trainer needs for Supabase RLS and stay-signed-in. NOTE: with `aud` set, sign-in
// FAILS until the Auth0 API is registered (Auth0 returns "service not found") — see report.
EM_JS(int, pt_auth0_sign_in, (const char* url, const char* cid, const char* scope,
                              const char* realm, const char* aud,
                              const char* user, const char* pw), {
    try {
        var body = new URLSearchParams();
        body.set("grant_type", "http://auth0.com/oauth/grant-type/password-realm");
        body.set("client_id", UTF8ToString(cid));
        body.set("realm", UTF8ToString(realm));
        body.set("scope", UTF8ToString(scope));
        var a = UTF8ToString(aud);
        if (a) { body.set("audience", a); }
        body.set("username", UTF8ToString(user));
        body.set("password", UTF8ToString(pw));
        var xhr = new XMLHttpRequest();
        xhr.open("POST", UTF8ToString(url), false);
        xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
        xhr.send(body.toString());
        if (xhr.status >= 200 && xhr.status < 300) {
            globalThis.ptAuth0StashSession(JSON.parse(xhr.responseText));
            return 0;
        }
        var code = "", nm = "", desc = "";
        try {
            var j = JSON.parse(xhr.responseText);
            code = j.error || j.code || ""; nm = j.name || "";
            desc = j.error_description || j.description || j.message || "";
        } catch (e) {}
        return globalThis.ptAuth0MapStatus(xhr.status, code, nm, desc);
    } catch (e) { return 2; }
});

// Sync stay-signed-in: redeem the persisted refresh token (refresh_token grant) for fresh
// tokens and re-stash the session. Returns 0 on success; 1 = no durable token (no network
// hit); 2 = the token is dead (4xx invalid_grant — dropped so we never retry it); 3 =
// transient failure (network / 5xx — the token is kept for the next load). The openid scope
// yields a fresh id_token; offline_access keeps a (rotated) refresh token alive.
EM_JS(int, pt_auth0_restore, (const char* url, const char* cid, const char* scope,
                              const char* aud), {
    var rt = "";
    try { rt = localStorage.getItem("pt.auth0.rt") || ""; } catch (e) { rt = ""; }
    if (!rt) { return 1; }
    try {
        var body = new URLSearchParams();
        body.set("grant_type", "refresh_token");
        body.set("client_id", UTF8ToString(cid));
        body.set("refresh_token", rt);
        body.set("scope", UTF8ToString(scope));
        var a = UTF8ToString(aud);
        if (a) { body.set("audience", a); }
        var xhr = new XMLHttpRequest();
        xhr.open("POST", UTF8ToString(url), false);
        xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
        xhr.send(body.toString());
        if (xhr.status >= 200 && xhr.status < 300) {
            globalThis.ptAuth0StashSession(JSON.parse(xhr.responseText));
            return 0;
        }
        if (xhr.status >= 400 && xhr.status < 500) {
            try { localStorage.removeItem("pt.auth0.rt"); } catch (e) {}
            return 2;
        }
        return 3;
    } catch (e) { return 3; }
});

// Sync database signup (creates the user). The caller logs in afterward to get tokens.
EM_JS(int, pt_auth0_signup, (const char* url, const char* cid, const char* conn,
                             const char* email, const char* user, const char* pw), {
    try {
        var payload = {
            client_id: UTF8ToString(cid),
            email: UTF8ToString(email),
            password: UTF8ToString(pw),
            connection: UTF8ToString(conn)
        };
        var u = UTF8ToString(user);
        if (u) { payload.username = u; }
        var xhr = new XMLHttpRequest();
        xhr.open("POST", UTF8ToString(url), false);
        xhr.setRequestHeader("Content-Type", "application/json");
        xhr.send(JSON.stringify(payload));
        if (xhr.status >= 200 && xhr.status < 300) { return 0; }
        var code = "", nm = "", desc = "";
        try {
            var j = JSON.parse(xhr.responseText);
            code = j.code || j.error || ""; nm = j.name || "";
            desc = j.description || j.message || j.error_description || "";
        } catch (e) {}
        return globalThis.ptAuth0MapStatus(xhr.status, code, nm, desc);
    } catch (e) { return 2; }
});

// Copy a stashed-session string field into a C++ buffer (truncating to maxlen).
EM_JS(void, pt_auth0_session_field, (const char* key, char* out, int maxlen), {
    var s = globalThis.ptAuth0Session || {};
    var v = s[UTF8ToString(key)];
    if (typeof v !== "string") { v = ""; }
    stringToUTF8(v, out, maxlen);
});

// Async, fire-and-forget reset email (Auth0 sends it; the link goes to Auth0's hosted
// reset page — the only hosted UI the app uses). Drives both authed Change Password and
// the logged-out Forgot Password.
EM_JS(void, pt_auth0_change_password, (const char* url, const char* cid, const char* conn,
                                       const char* email), {
    try {
        fetch(UTF8ToString(url), {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
                client_id: UTF8ToString(cid),
                email: UTF8ToString(email),
                connection: UTF8ToString(conn)
            })
        }).catch(function () {});
    } catch (e) {}
});

EM_JS(void, pt_auth0_sign_out, (), {
    globalThis.ptAuth0ClearSession();
});

// Delete is stubbed: the privileged Auth0 user-record deletion needs the Management API,
// which a SPA cannot safely hold. The local IDBFS wipe + sign-out are real (done by the
// AuthManager / here); the server-side account is NOT removed.
EM_JS(void, pt_auth0_delete_stub, (), {
    if (typeof console !== "undefined") {
        console.warn("[pt] Auth0 user-record deletion requires the Management API and is " +
                     "stubbed: the local IDBFS wipe + Supabase row delete + sign-out are " +
                     "performed, but the server-side Auth0 account is NOT deleted.");
    }
    globalThis.ptAuth0ClearSession();
});

// Sync health probe (GET JWKS). 1 = reachable.
EM_JS(int, pt_auth0_health, (const char* url), {
    try {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", UTF8ToString(url), false);
        xhr.send();
        return (xhr.status >= 200 && xhr.status < 300) ? 1 : 0;
    } catch (e) { return 0; }
});

// clang-format on

namespace poker_trainer::persistence {

namespace {

// The database connection name. Not in the sealed auth0_config.hpp; this is Auth0's
// default DB connection name. If the tenant uses a different one, update here (and the
// dashboard "Default Directory" / connection settings — see the report).
constexpr std::string_view kConnection = "poker-trainer-db";

[[nodiscard]] std::string base_url() {
    return "https://" + std::string{kAuth0Domain};
}

[[nodiscard]] std::string read_session_field(const char* key, int cap) {
    std::string buf(static_cast<std::size_t>(cap), '\0');
    pt_auth0_session_field(key, buf.data(), cap);
    buf.resize(std::char_traits<char>::length(buf.c_str()));
    return buf;
}

[[nodiscard]] AuthSession read_stashed_session() {
    AuthSession s{};
    s.auth0_user_id = read_session_field("sub", 512);
    s.display_name = read_session_field("name", 512);
    s.email = read_session_field("email", 512);
    s.access_token = read_session_field("token", 4096);  // JWTs run long
    return s;
}

// openid (identity) + offline_access (refresh token for stay-signed-in). kAuth0Scopes is the
// sealed base set; offline_access is composed here so the sealed header is untouched. The
// audience makes the access token a JWT (see pt_auth0_sign_in).
[[nodiscard]] std::string token_scope() {
    return std::string{kAuth0Scopes} + " offline_access";
}

}  // namespace

Auth0Backend::Auth0Backend() noexcept { pt_auth0_init(); }

bool Auth0Backend::health_check() noexcept {
    const std::string url = base_url() + "/.well-known/jwks.json";
    return pt_auth0_health(url.c_str()) != 0;
}

std::expected<AuthSession, AuthError> Auth0Backend::sign_in(const AuthCredentials& credentials) {
    const std::string url = base_url() + "/oauth/token";
    const int code = pt_auth0_sign_in(
        url.c_str(), std::string{kAuth0ClientId}.c_str(), token_scope().c_str(),
        std::string{kConnection}.c_str(), std::string{kAuth0Audience}.c_str(),
        credentials.email.c_str(), credentials.password.c_str());
    if (code != 0) {
        return std::unexpected(auth0_code_to_error(code));
    }
    return read_stashed_session();
}

std::optional<AuthSession> Auth0Backend::restore_session() {
    const std::string url = base_url() + "/oauth/token";
    const int code = pt_auth0_restore(url.c_str(), std::string{kAuth0ClientId}.c_str(),
                                      token_scope().c_str(), std::string{kAuth0Audience}.c_str());
    if (code != 0) {
        return std::nullopt;  // no durable token, or the silent refresh failed
    }
    return read_stashed_session();
}

std::expected<AuthSession, AuthError> Auth0Backend::sign_up(const AuthCredentials& credentials,
                                                            std::string_view display_name) {
    const std::string url = base_url() + "/dbconnections/signup";
    const int code = pt_auth0_signup(url.c_str(), std::string{kAuth0ClientId}.c_str(),
                                     std::string{kConnection}.c_str(), credentials.email.c_str(),
                                     std::string{display_name}.c_str(), credentials.password.c_str());
    if (code != 0) {
        return std::unexpected(auth0_code_to_error(code));
    }
    // Signup created the user but returns no tokens; log in to establish the session.
    std::expected<AuthSession, AuthError> session = sign_in(credentials);
    if (session.has_value()) {
        // Pin the display name to the username the user chose (the id_token's name claim
        // may default to the email for a brand-new DB user).
        session->display_name = std::string{display_name};
    }
    return session;
}

std::expected<void, AuthError> Auth0Backend::sign_out() {
    pt_auth0_sign_out();
    return {};
}

std::expected<void, AuthError> Auth0Backend::delete_account(std::string_view /*auth0_user_id*/) {
    // Stubbed server-side (Management API); the AuthManager performs the real local wipe.
    pt_auth0_delete_stub();
    return {};
}

std::expected<void, AuthError> Auth0Backend::change_password(std::string_view email) {
    const std::string url = base_url() + "/dbconnections/change_password";
    pt_auth0_change_password(url.c_str(), std::string{kAuth0ClientId}.c_str(),
                             std::string{kConnection}.c_str(), std::string{email}.c_str());
    return {};
}

}  // namespace poker_trainer::persistence

#endif  // __EMSCRIPTEN__
