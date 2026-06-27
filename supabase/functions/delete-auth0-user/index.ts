// supabase/functions/delete-auth0-user/index.ts
//
// Deletes the CALLER'S OWN Auth0 user record. All authority comes from the caller's
// Auth0 id_token: the function verifies it against the tenant JWKS and acts only on the
// verified `sub`. It trusts no privileged input from the request body (the client sends
// an empty `{}`), holds no service-role key, and never accepts a sub as a parameter.
//
// Pairs with the C++ client call in src/persistence/auth.cpp / supabase_backend.cpp:
//   POST <SUPABASE_URL>/functions/v1/delete-auth0-user
//   headers: apikey: <anon>, Authorization: Bearer <Auth0 id_token>, Content-Type: application/json
//   body:    {}
//   client treats any non-2xx as a (non-blocking) failure.
//
// IMPORTANT — deploy with JWT verification OFF:
//   supabase functions deploy delete-auth0-user --no-verify-jwt
// The Authorization header carries an AUTH0 id_token, not a Supabase JWT, so the
// platform's default Supabase-JWT gate would reject every call before this code runs.
// We do our own Auth0 verification below; the anon `apikey` still gates invocation.
//
// Required Supabase secrets (already set; never hardcode):
//   AUTH0_DOMAIN, AUTH0_MGMT_CLIENT_ID, AUTH0_MGMT_CLIENT_SECRET
// Optional hardening (no-op when unset):
//   AUTH0_ISSUER          — override the expected issuer (custom-domain tenants)
//   AUTH0_SPA_CLIENT_ID   — when set, also require it as the id_token audience

import { createRemoteJWKSet, jwtVerify } from "npm:jose@5";

const AUTH0_DOMAIN = Deno.env.get("AUTH0_DOMAIN") ?? "";
const AUTH0_MGMT_CLIENT_ID = Deno.env.get("AUTH0_MGMT_CLIENT_ID") ?? "";
const AUTH0_MGMT_CLIENT_SECRET = Deno.env.get("AUTH0_MGMT_CLIENT_SECRET") ?? "";
const AUTH0_ISSUER = Deno.env.get("AUTH0_ISSUER") ?? `https://${AUTH0_DOMAIN}/`;
const AUTH0_SPA_CLIENT_ID = Deno.env.get("AUTH0_SPA_CLIENT_ID") ?? "";

// Cached across warm invocations; jose fetches + caches the signing keys by `kid`.
const JWKS = AUTH0_DOMAIN
  ? createRemoteJWKSet(new URL(`https://${AUTH0_DOMAIN}/.well-known/jwks.json`))
  : null;

function corsHeaders(origin: string | null): Record<string, string> {
  // Auth is the bearer token, not the origin, so reflecting the caller's origin (or "*")
  // is safe; a forged origin still cannot present a valid id_token.
  return {
    "Access-Control-Allow-Origin": origin ?? "*",
    "Access-Control-Allow-Methods": "POST, OPTIONS",
    "Access-Control-Allow-Headers": "authorization, apikey, content-type",
    "Access-Control-Max-Age": "86400",
    "Vary": "Origin",
  };
}

function json(body: unknown, status: number, origin: string | null): Response {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "Content-Type": "application/json", ...corsHeaders(origin) },
  });
}

Deno.serve(async (req: Request): Promise<Response> => {
  const origin = req.headers.get("Origin");

  // CORS preflight (the client sends custom apikey/Authorization headers).
  if (req.method === "OPTIONS") {
    return new Response(null, { status: 204, headers: corsHeaders(origin) });
  }
  if (req.method !== "POST") {
    return json({ error: "method_not_allowed" }, 405, origin);
  }
  if (!AUTH0_DOMAIN || !AUTH0_MGMT_CLIENT_ID || !AUTH0_MGMT_CLIENT_SECRET || !JWKS) {
    return json({ error: "server_misconfigured" }, 500, origin);
  }

  // 1. Verify the caller's Auth0 id_token and extract the trusted `sub`.
  const authHeader = req.headers.get("Authorization") ?? "";
  const match = authHeader.match(/^Bearer\s+(.+)$/i);
  if (!match) {
    return json({ error: "missing_bearer" }, 401, origin);
  }
  let sub: string;
  try {
    const opts: { issuer: string; audience?: string } = { issuer: AUTH0_ISSUER };
    if (AUTH0_SPA_CLIENT_ID) {
      opts.audience = AUTH0_SPA_CLIENT_ID;
    }
    // jwtVerify validates the RS256 signature against the JWKS and enforces exp / nbf /
    // iss (and aud when provided); it throws on any failure.
    const { payload } = await jwtVerify(match[1], JWKS, opts);
    if (typeof payload.sub !== "string" || payload.sub.length === 0) {
      return json({ error: "invalid_token_sub" }, 401, origin);
    }
    sub = payload.sub;
  } catch (_err) {
    return json({ error: "invalid_token" }, 401, origin);
  }

  // 2. Obtain an Auth0 Management API token via client-credentials (M2M app).
  let mgmtToken: string;
  try {
    const tokenResp = await fetch(`https://${AUTH0_DOMAIN}/oauth/token`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        grant_type: "client_credentials",
        client_id: AUTH0_MGMT_CLIENT_ID,
        client_secret: AUTH0_MGMT_CLIENT_SECRET,
        audience: `https://${AUTH0_DOMAIN}/api/v2/`,
      }),
    });
    if (!tokenResp.ok) {
      return json({ error: "mgmt_token_failed", status: tokenResp.status }, 502, origin);
    }
    const tokenBody = await tokenResp.json();
    if (typeof tokenBody.access_token !== "string") {
      return json({ error: "mgmt_token_missing" }, 502, origin);
    }
    mgmtToken = tokenBody.access_token;
  } catch (_err) {
    return json({ error: "mgmt_token_error" }, 502, origin);
  }

  // 3. Delete the verified user (Auth0 returns 204 No Content on success).
  try {
    const delResp = await fetch(
      `https://${AUTH0_DOMAIN}/api/v2/users/${encodeURIComponent(sub)}`,
      { method: "DELETE", headers: { Authorization: `Bearer ${mgmtToken}` } },
    );
    // 204 is success; a missing user (404) is treated as already-deleted (idempotent).
    if (delResp.status !== 204 && delResp.status !== 404 && !delResp.ok) {
      return json({ error: "delete_failed", status: delResp.status }, 502, origin);
    }
  } catch (_err) {
    return json({ error: "delete_error" }, 502, origin);
  }

  // 4. Success — the client only checks the status code.
  return json({ ok: true }, 200, origin);
});
