#pragma once

#include <string_view>

// Supabase data-backend configuration.
//
// NOT a Phase 0 sealed header — it is introduced with the server-sync work and
// may evolve. It is the data-layer sibling of auth0_config.hpp (which is sealed
// and holds the identity provider). Auth0 remains the identity provider;
// Supabase is data + Row-Level Security only, trusting the Auth0-issued JWT as a
// third-party issuer. See the deployment report for the dashboard setup these
// values depend on.
//
// Everything here is PUBLIC and safe to ship in the wasm bundle:
//   - the project URL is public,
//   - the anon key is the public, RLS-gated key (it grants nothing on its own;
//     every row is scoped by RLS to the authenticated Auth0 `sub`).
// The service-role key MUST NEVER appear in this file or anywhere in the bundle.

namespace poker_trainer::persistence {

// The Supabase project base URL, e.g. "https://abcdefgh.supabase.co". The REST
// endpoint is this + "/rest/v1/...". Public.
//
// TODO(dao, 2026-06-24): replace with the real project URL before live testing.
inline constexpr std::string_view kSupabaseUrl = https://xrqowhhjvebzccdxcafy.supabase.co/;

// The public anon API key (a JWT with role "anon"). RLS gates every table, so
// this key alone reads/writes nothing — a valid Auth0 third-party JWT in the
// Authorization header is what scopes access to the caller's own row. Public.
//
// TODO(dao, 2026-06-24): replace with the real anon key before live testing.
inline constexpr std::string_view kSupabaseAnonKey = eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InhycW93aGhqdmViemNjZHhjYWZ5Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3ODIzNDc4NzMsImV4cCI6MjA5NzkyMzg3M30.DwbiV0n5pEQJ8heHtX-XZlm9I5A0fY9pjh70SGuWH8E;

// The PostgREST table holding per-user account state, one row per Auth0 `sub`.
inline constexpr std::string_view kSupabaseAccountTable = "account_state";

// REST paths derived from the table. Kept as separate constants so the request
// builders never hand-concatenate the path inconsistently.
//
//   - kSupabaseAccountUpsertPath: POST target for an upsert. on_conflict names
//     the conflict target (the auth0_sub primary key) so a repeat write merges
//     into the existing row instead of failing the unique constraint.
//   - kSupabaseAccountSelectQuery: GET query selecting the blob column for the
//     caller's row (RLS already restricts the result to that single row; the
//     explicit auth0_sub filter is appended at call time as defense in depth).
inline constexpr std::string_view kSupabaseRestPrefix = "/rest/v1/";
inline constexpr std::string_view kSupabaseAccountUpsertQuery =
    "?on_conflict=auth0_sub";
inline constexpr std::string_view kSupabaseAccountSelectColumns =
    "?select=state_blob&auth0_sub=eq.";

}  // namespace poker_trainer::persistence
