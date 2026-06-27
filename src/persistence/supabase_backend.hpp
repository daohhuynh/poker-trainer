#pragma once

#include "persistence/migration.hpp"
#include "persistence/persistence_schema.hpp"
#include "persistence/sync.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Zone 04 — the PRODUCTION Supabase SyncBackend. Implements the same SyncBackend
// seam the tests' MockSyncBackend implements, bridging C++ to Supabase's PostgREST
// over authenticated HTTP. Auth0 stays the identity provider; Supabase is data +
// Row-Level Security only, trusting the Auth0-issued JWT as a third-party issuer.
//
// Token model (verified against Supabase's Auth0 third-party-auth guide): the
// bearer sent to Supabase is the Auth0 ID TOKEN, not the access token. Supabase
// reads a `role` claim of "authenticated" to assign the Postgres role, and Auth0
// strips non-namespaced custom claims from ACCESS tokens — so the role claim must
// ride the ID token, and the ID token is what Supabase validates. The browser glue
// reads the live ID token off globalThis.ptAuth0Session (stashed by the Auth0
// backend at sign-in / silent-restore); no token is plumbed through C++ or
// persisted. RLS scopes every row to (auth.jwt()->>'sub'), i.e. the Auth0 sub.
//
// As with auth0_backend.hpp, the DOM glue (the SupabaseSyncBackend class) compiles
// only under __EMSCRIPTEN__. Everything below it — base64, the JSON request-body
// builders, the HTTP-status -> FetchOutcome translation, and the GET-response
// parser — is the testable boundary contract and compiles natively too.

namespace poker_trainer::persistence {

// --- Encoding ---

// Standard-alphabet base64 (RFC 4648) with "=" padding. The serialized AppState
// is binary; base64 lets it travel as a JSON string in the state_blob column.
[[nodiscard]] std::string base64_encode(std::span<const std::uint8_t> data);

// Inverse of base64_encode. std::nullopt when the input is not valid base64
// (wrong length, illegal character) so a corrupt server blob never decodes to
// silent garbage.
[[nodiscard]] std::optional<std::vector<std::uint8_t>> base64_decode(
    std::string_view text);

// --- Request shaping (pure; RLS-token shaping is asserted against these) ---

// Escape a string for embedding inside a JSON string literal (", \, and control
// characters). The Auth0 sub ("auth0|abc") and a user-chosen display name both
// flow through here before reaching the request body.
[[nodiscard]] std::string json_escape(std::string_view value);

// Percent-encode a string for use as a URL query-component value (RFC 3986
// unreserved set passes through; everything else becomes %XX). The Auth0 sub
// carries a "|" that must be encoded in the RLS filter / delete query.
[[nodiscard]] std::string url_encode_component(std::string_view value);

// Build the JSON object body for an account-state upsert. Exactly the columns the
// account_state row carries: the Auth0 sub (RLS key), the base64 state blob, the
// denormalized lifetime-tomatoes (the leaderboard metric), the display name, and the
// denormalized leaderboard opt-in flag (the queryable column the leaderboard RPC
// filters on; the flag itself lives in the opaque settings blob, so it is denormalized
// out here by the caller, which reads it live — see push / upload_initial).
[[nodiscard]] std::string build_account_upsert_body(
    std::string_view auth0_sub, std::string_view state_blob_b64,
    std::uint64_t lifetime, std::string_view display_name, bool opted_in);

// Build the upsert body for a full state push: the newest snapshot serialized +
// base64'd into state_blob, with lifetime + display name + opt-in denormalized out.
// opted_in is supplied by the caller (read live off the session) since it is not a
// field on AppState.
[[nodiscard]] std::string build_push_body(std::string_view auth0_sub,
                                          const AppState& state, bool opted_in);

// Build the upsert body for the guest->account seed. Exactly the three migration
// fields become a minimal AppState (spendable / lifetime / unlocked tracks; every
// other field default), serialized into state_blob. The display name and opt-in flag
// are not part of the migration payload, so they are supplied by the caller (read from
// the live Auth0 session / settings).
[[nodiscard]] std::string build_initial_body(
    std::string_view auth0_sub, const AccountMigrationState& initial,
    std::string_view display_name, bool opted_in);

// --- Response translation (pure) ---

// Translate an HTTP status from a GET into a fetch outcome, given whether the
// response carried a row. 2xx + row -> Found; 2xx + no row -> NotFound; anything
// else (0 network, 401/403 RLS/auth, 5xx) -> Failed (keep local, retry).
[[nodiscard]] FetchOutcome supabase_fetch_outcome(int http_status,
                                                  bool row_present) noexcept;

// True when a write (upsert / delete) HTTP status indicates durable acceptance
// (any 2xx). Everything else is a failure the SyncEngine retries with backoff.
[[nodiscard]] bool supabase_write_ok(int http_status) noexcept;

// Extract the first JSON string value for `field` from `json` (a shallow scan for
// "field":"...."). Returns std::nullopt when the field is absent. Sufficient for
// the PostgREST array-of-one-object the select returns; the base64 value contains
// no quote/backslash, so the scan to the closing quote is unambiguous.
[[nodiscard]] std::optional<std::string> extract_json_string_field(
    std::string_view json, std::string_view field);

// Parse a GET response into a FetchResult: non-2xx -> Failed; 2xx with no
// state_blob -> NotFound (never-seeded account); 2xx with a decodable blob ->
// Found + the reconstructed AppState; 2xx with a present-but-undecodable blob ->
// Failed (do not adopt garbage and do not migrate over real server data).
[[nodiscard]] FetchResult parse_fetch_response(int http_status,
                                               std::string_view response_body);

// Parse the leaderboard_top_100() RPC response into ranked entries. The body is a
// PostgREST JSON array of {rank, display_name, lifetime_tomatoes} objects, ordered by
// rank. Non-2xx -> not ok (the UI shows the error/Retry state); 2xx -> ok with one
// entry per well-formed object (objects missing a required field are skipped). Pure
// and native-testable (the leaderboard fetch/parse is the tested boundary; the HTTP
// round-trip is browser-verified).
[[nodiscard]] LeaderboardFetchResult parse_leaderboard_response(
    int http_status, std::string_view response_body);

#ifdef __EMSCRIPTEN__

// Production Supabase backend. Constructed once at boot. Holds no per-call state:
// the bearer (Auth0 ID token) is read live off globalThis at each call, and the
// anon key + project URL come from supabase_config.hpp. Embeds no service-role
// key — only the public anon key.
class SupabaseSyncBackend final : public SyncBackend {
public:
    [[nodiscard]] FetchResult fetch(std::string_view auth0_user_id) override;

    [[nodiscard]] bool push(
        std::string_view auth0_user_id,
        std::span<const AppState> ordered_writes) override;

    [[nodiscard]] bool upload_initial(
        std::string_view auth0_user_id,
        const AccountMigrationState& initial) override;

    [[nodiscard]] bool delete_account_state(
        std::string_view auth0_user_id) override;

    [[nodiscard]] LeaderboardFetchResult fetch_leaderboard() override;

    [[nodiscard]] bool delete_auth0_user() override;
};

#endif  // __EMSCRIPTEN__

}  // namespace poker_trainer::persistence
