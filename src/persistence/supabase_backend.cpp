#include "persistence/supabase_backend.hpp"

#include "persistence/idbfs.hpp"
#include "persistence/migration.hpp"
#include "persistence/persistence_schema.hpp"
#include "persistence/sync.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace poker_trainer::persistence {

// ============================================================================
// Pure boundary contract — native + wasm. Tested directly (base64 round-trip,
// request-body / RLS-header shaping, GET-response parsing, status translation).
// ============================================================================

namespace {

constexpr std::string_view kBase64Alphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

[[nodiscard]] int base64_value(char c) noexcept {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

}  // namespace

std::string base64_encode(std::span<const std::uint8_t> data) {
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    std::size_t i = 0;
    while (i + 3 <= data.size()) {
        const std::uint32_t n = (static_cast<std::uint32_t>(data[i]) << 16) |
                                (static_cast<std::uint32_t>(data[i + 1]) << 8) |
                                static_cast<std::uint32_t>(data[i + 2]);
        out.push_back(kBase64Alphabet[(n >> 18) & 0x3Fu]);
        out.push_back(kBase64Alphabet[(n >> 12) & 0x3Fu]);
        out.push_back(kBase64Alphabet[(n >> 6) & 0x3Fu]);
        out.push_back(kBase64Alphabet[n & 0x3Fu]);
        i += 3;
    }
    const std::size_t rem = data.size() - i;
    if (rem == 1) {
        const std::uint32_t n = static_cast<std::uint32_t>(data[i]) << 16;
        out.push_back(kBase64Alphabet[(n >> 18) & 0x3Fu]);
        out.push_back(kBase64Alphabet[(n >> 12) & 0x3Fu]);
        out.push_back('=');
        out.push_back('=');
    } else if (rem == 2) {
        const std::uint32_t n = (static_cast<std::uint32_t>(data[i]) << 16) |
                                (static_cast<std::uint32_t>(data[i + 1]) << 8);
        out.push_back(kBase64Alphabet[(n >> 18) & 0x3Fu]);
        out.push_back(kBase64Alphabet[(n >> 12) & 0x3Fu]);
        out.push_back(kBase64Alphabet[(n >> 6) & 0x3Fu]);
        out.push_back('=');
    }
    return out;
}

std::optional<std::vector<std::uint8_t>> base64_decode(std::string_view text) {
    if (text.size() % 4 != 0) {
        return std::nullopt;
    }
    if (text.empty()) {
        return std::vector<std::uint8_t>{};
    }
    std::size_t pad = 0;
    if (text[text.size() - 1] == '=') ++pad;
    if (text[text.size() - 2] == '=') ++pad;

    std::vector<std::uint8_t> out;
    out.reserve((text.size() / 4) * 3);
    for (std::size_t i = 0; i < text.size(); i += 4) {
        const bool last = (i + 4 >= text.size());
        const char c2 = text[i + 2];
        const char c3 = text[i + 3];
        const int a = base64_value(text[i]);
        const int b = base64_value(text[i + 1]);
        const int c = (last && c2 == '=') ? 0 : base64_value(c2);
        const int d = (last && c3 == '=') ? 0 : base64_value(c3);
        if (a < 0 || b < 0 || c < 0 || d < 0) {
            return std::nullopt;
        }
        const std::uint32_t n = (static_cast<std::uint32_t>(a) << 18) |
                                (static_cast<std::uint32_t>(b) << 12) |
                                (static_cast<std::uint32_t>(c) << 6) |
                                static_cast<std::uint32_t>(d);
        out.push_back(static_cast<std::uint8_t>((n >> 16) & 0xFFu));
        if (!(last && pad >= 2)) {
            out.push_back(static_cast<std::uint8_t>((n >> 8) & 0xFFu));
        }
        if (!(last && pad >= 1)) {
            out.push_back(static_cast<std::uint8_t>(n & 0xFFu));
        }
    }
    return out;
}

std::string json_escape(std::string_view value) {
    static constexpr std::string_view kHex = "0123456789abcdef";
    std::string out;
    out.reserve(value.size() + 2);
    for (const char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20u) {
                    const auto byte = static_cast<unsigned char>(c);
                    out += "\\u00";
                    out.push_back(kHex[(byte >> 4) & 0x0Fu]);
                    out.push_back(kHex[byte & 0x0Fu]);
                } else {
                    out.push_back(c);
                }
        }
    }
    return out;
}

std::string url_encode_component(std::string_view value) {
    static constexpr std::string_view kHex = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size());
    for (const char c : value) {
        const bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                                (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                                c == '.' || c == '~';
        if (unreserved) {
            out.push_back(c);
        } else {
            const auto byte = static_cast<unsigned char>(c);
            out.push_back('%');
            out.push_back(kHex[(byte >> 4) & 0x0Fu]);
            out.push_back(kHex[byte & 0x0Fu]);
        }
    }
    return out;
}

std::string build_account_upsert_body(std::string_view auth0_sub,
                                      std::string_view state_blob_b64,
                                      std::uint64_t lifetime,
                                      std::string_view display_name, bool opted_in) {
    std::string body = "{\"auth0_sub\":\"";
    body += json_escape(auth0_sub);
    body += "\",\"state_blob\":\"";
    body += json_escape(state_blob_b64);
    body += "\",\"lifetime_tomatoes\":";
    body += std::to_string(lifetime);
    body += ",\"display_name\":\"";
    body += json_escape(display_name);
    body += "\",\"opted_in\":";
    body += opted_in ? "true" : "false";
    body += "}";
    return body;
}

std::string build_push_body(std::string_view auth0_sub, const AppState& state,
                            bool opted_in) {
    const std::vector<std::uint8_t> bytes = serialize_app_state(state);
    return build_account_upsert_body(auth0_sub, base64_encode(bytes),
                                     state.tomatoes.lifetime,
                                     state.account.display_name, opted_in);
}

std::string build_initial_body(std::string_view auth0_sub,
                               const AccountMigrationState& initial,
                               std::string_view display_name, bool opted_in) {
    // Exactly the three migration fields become a minimal AppState; every other
    // field stays default so the never-seeded server row holds only what Module 7
    // permits to migrate.
    AppState seed{};
    seed.tomatoes.spendable = initial.spendable;
    seed.tomatoes.lifetime = initial.lifetime;
    seed.music_library.unlocked_track_ids = initial.unlocked_track_ids;
    const std::vector<std::uint8_t> bytes = serialize_app_state(seed);
    return build_account_upsert_body(auth0_sub, base64_encode(bytes),
                                     initial.lifetime, display_name, opted_in);
}

FetchOutcome supabase_fetch_outcome(int http_status, bool row_present) noexcept {
    if (http_status < 200 || http_status >= 300) {
        return FetchOutcome::Failed;
    }
    return row_present ? FetchOutcome::Found : FetchOutcome::NotFound;
}

bool supabase_write_ok(int http_status) noexcept {
    return http_status >= 200 && http_status < 300;
}

std::optional<std::string> extract_json_string_field(std::string_view json,
                                                     std::string_view field) {
    std::string needle = "\"";
    needle += field;
    needle += "\"";
    const std::size_t key = json.find(needle);
    if (key == std::string_view::npos) {
        return std::nullopt;
    }
    std::size_t p = key + needle.size();
    while (p < json.size() &&
           (json[p] == ' ' || json[p] == '\t' || json[p] == ':')) {
        ++p;
    }
    if (p >= json.size() || json[p] != '"') {
        return std::nullopt;
    }
    ++p;  // step over the opening quote
    std::string out;
    while (p < json.size() && json[p] != '"') {
        if (json[p] == '\\' && p + 1 < json.size()) {
            const char e = json[p + 1];
            switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                // The only string field we parse is state_blob, which is base64
                // and carries no escapes; the table above covers PostgREST's
                // escaping of any incidental display-name echo defensively.
                default: out.push_back(e); break;
            }
            p += 2;
        } else {
            out.push_back(json[p]);
            ++p;
        }
    }
    if (p >= json.size()) {
        return std::nullopt;  // unterminated string
    }
    return out;
}

FetchResult parse_fetch_response(int http_status,
                                 std::string_view response_body) {
    if (!supabase_write_ok(http_status)) {
        return FetchResult{FetchOutcome::Failed, AppState{}};
    }
    const std::optional<std::string> blob_b64 =
        extract_json_string_field(response_body, "state_blob");
    if (!blob_b64.has_value()) {
        // 2xx with no row (PostgREST returns "[]") — a never-seeded account.
        return FetchResult{FetchOutcome::NotFound, AppState{}};
    }
    const std::optional<std::vector<std::uint8_t>> bytes =
        base64_decode(*blob_b64);
    if (!bytes.has_value()) {
        // Present but undecodable: don't adopt garbage, and don't NotFound
        // (which would migrate local state over real server data). Treat as a
        // transient failure so the reconcile retries.
        return FetchResult{FetchOutcome::Failed, AppState{}};
    }
    const DeserializeResult parsed = deserialize_app_state(*bytes);
    if (parsed.result != SchemaValidationResult::Ok) {
        return FetchResult{FetchOutcome::Failed, AppState{}};
    }
    return FetchResult{FetchOutcome::Found, parsed.state};
}

namespace {

// Extract the first JSON non-negative integer value for `field` from `json` (a shallow
// "field":<digits> scan). Sufficient for the rank / lifetime_tomatoes counts in the
// leaderboard RPC rows (no negatives, no decimals). std::nullopt when absent.
[[nodiscard]] std::optional<std::uint64_t> extract_json_uint_field(
    std::string_view json, std::string_view field) {
    std::string needle = "\"";
    needle += field;
    needle += "\"";
    const std::size_t key = json.find(needle);
    if (key == std::string_view::npos) {
        return std::nullopt;
    }
    std::size_t p = key + needle.size();
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t' || json[p] == ':')) {
        ++p;
    }
    std::uint64_t value = 0;
    bool any = false;
    while (p < json.size() && json[p] >= '0' && json[p] <= '9') {
        value = value * 10u + static_cast<std::uint64_t>(json[p] - '0');
        any = true;
        ++p;
    }
    return any ? std::optional<std::uint64_t>{value} : std::nullopt;
}

// Index just past the JSON object that begins at `open` ('{'), tracking brace depth
// and ignoring braces inside strings. Returns body.size() if unterminated.
[[nodiscard]] std::size_t json_object_end(std::string_view body, std::size_t open) {
    std::size_t depth = 0;
    bool in_string = false;
    bool escaped = false;
    for (std::size_t i = open; i < body.size(); ++i) {
        const char c = body[i];
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
        } else if (c == '{') {
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0) {
                return i + 1;
            }
        }
    }
    return body.size();
}

}  // namespace

LeaderboardFetchResult parse_leaderboard_response(int http_status,
                                                  std::string_view response_body) {
    LeaderboardFetchResult result{};
    if (!supabase_write_ok(http_status)) {
        return result;  // ok stays false: the UI shows the error + Retry state
    }
    result.ok = true;
    std::size_t i = 0;
    while (i < response_body.size()) {
        if (response_body[i] != '{') {
            ++i;
            continue;
        }
        const std::size_t end = json_object_end(response_body, i);
        const std::string_view obj = response_body.substr(i, end - i);
        const std::optional<std::string> name =
            extract_json_string_field(obj, "display_name");
        const std::optional<std::uint64_t> rank = extract_json_uint_field(obj, "rank");
        const std::optional<std::uint64_t> lifetime =
            extract_json_uint_field(obj, "lifetime_tomatoes");
        if (name.has_value() && rank.has_value() && lifetime.has_value()) {
            result.entries.push_back(LeaderboardEntry{static_cast<std::uint32_t>(*rank),
                                                      *name, *lifetime});
        }
        i = end;
    }
    return result;
}

}  // namespace poker_trainer::persistence

// ============================================================================
// Browser glue — wasm only. Synchronous XMLHttpRequest so the synchronous
// SyncBackend contract holds without Asyncify (the same pattern auth0_backend
// uses). The bearer is the Auth0 ID token read live off globalThis; the anon key
// + project URL are public config. No service-role key anywhere.
// ============================================================================

#ifdef __EMSCRIPTEN__

#include "persistence/supabase_config.hpp"

#include <emscripten/emscripten.h>

// clang-format off
// Every JS string literal below is DOUBLE-quoted: single quotes inside an EM_JS body are
// tokenized by the C preprocessor as char constants (-Winvalid-pp-token / -Wmultichar),
// which the -Werror baseline rejects. (Same rule as auth0_backend.cpp.)

// The bearer attached below is the Auth0 ID token — not the access token — which is what
// Supabase's Auth0 third-party integration validates (Auth0 strips the non-namespaced
// `role` claim from access tokens, so it must ride the id_token). It is read live off the
// session the Auth0 backend stashes on globalThis; it is empty before sign-in.

// Sync upsert (POST with Prefer: resolution=merge-duplicates). Returns the HTTP status, or
// 0 if the request never completed (offline / DNS / TLS / XHR throw).
EM_JS(int, pt_supabase_upsert, (const char* url, const char* apikey, const char* body), {
    try {
        var xhr = new XMLHttpRequest();
        xhr.open("POST", UTF8ToString(url), false);
        xhr.setRequestHeader("apikey", UTF8ToString(apikey));
        var s = globalThis.ptAuth0Session || {};
        var idt = (typeof s.id_token === "string") ? s.id_token : "";
        if (idt) { xhr.setRequestHeader("Authorization", "Bearer " + idt); }
        xhr.setRequestHeader("Content-Type", "application/json");
        xhr.setRequestHeader("Prefer", "resolution=merge-duplicates,return=minimal");
        xhr.send(UTF8ToString(body));
        return xhr.status;
    } catch (e) { return 0; }
});

// Sync GET. Writes the response text into the caller's buffer (truncating to maxlen) and
// returns the HTTP status.
EM_JS(int, pt_supabase_get, (const char* url, const char* apikey, char* out, int maxlen), {
    try {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", UTF8ToString(url), false);
        xhr.setRequestHeader("apikey", UTF8ToString(apikey));
        var s = globalThis.ptAuth0Session || {};
        var idt = (typeof s.id_token === "string") ? s.id_token : "";
        if (idt) { xhr.setRequestHeader("Authorization", "Bearer " + idt); }
        xhr.setRequestHeader("Accept", "application/json");
        xhr.send();
        stringToUTF8(xhr.responseText || "", out, maxlen);
        return xhr.status;
    } catch (e) { return 0; }
});

// Sync DELETE of the caller's own row (RLS scopes it; the auth0_sub filter is belt-and-
// suspenders). Returns the HTTP status.
EM_JS(int, pt_supabase_delete, (const char* url, const char* apikey), {
    try {
        var xhr = new XMLHttpRequest();
        xhr.open("DELETE", UTF8ToString(url), false);
        xhr.setRequestHeader("apikey", UTF8ToString(apikey));
        var s = globalThis.ptAuth0Session || {};
        var idt = (typeof s.id_token === "string") ? s.id_token : "";
        if (idt) { xhr.setRequestHeader("Authorization", "Bearer " + idt); }
        xhr.send();
        return xhr.status;
    } catch (e) { return 0; }
});

// Sync POST of a JSON body (PostgREST RPC or an Edge Function). Writes the response text
// into the caller's buffer when out is non-null, and returns the HTTP status. Used for the
// leaderboard RPC (reads the response) and the delete-auth0-user Edge Function (status
// only — out is null). The bearer is the live Auth0 id_token, same as every call above.
EM_JS(int, pt_supabase_post_json,
      (const char* url, const char* apikey, const char* body, char* out, int maxlen), {
    try {
        var xhr = new XMLHttpRequest();
        xhr.open("POST", UTF8ToString(url), false);
        xhr.setRequestHeader("apikey", UTF8ToString(apikey));
        var s = globalThis.ptAuth0Session || {};
        var idt = (typeof s.id_token === "string") ? s.id_token : "";
        if (idt) { xhr.setRequestHeader("Authorization", "Bearer " + idt); }
        xhr.setRequestHeader("Content-Type", "application/json");
        xhr.setRequestHeader("Accept", "application/json");
        xhr.send(UTF8ToString(body));
        if (out) { stringToUTF8(xhr.responseText || "", out, maxlen); }
        return xhr.status;
    } catch (e) { return 0; }
});

// Read the live leaderboard opt-in flag, stashed on globalThis by the bridge whenever the
// setting changes (boot.cpp set_leaderboard_opt_in_stash). It is not a field on AppState
// (it lives in the opaque settings blob), so the push denormalizes it out via this read —
// the same globalThis-handoff the id_token and display name already use. Returns 1/0.
EM_JS(int, pt_read_leaderboard_opt_in, (), {
    return (globalThis.ptLeaderboardOptIn === true) ? 1 : 0;
});

// Copy the live session display name into a C++ buffer (the migration payload omits it, so
// the initial seed reads it here).
EM_JS(void, pt_supabase_session_name, (char* out, int maxlen), {
    var s = globalThis.ptAuth0Session || {};
    var v = (typeof s.name === "string") ? s.name : "";
    stringToUTF8(v, out, maxlen);
});

// clang-format on

namespace poker_trainer::persistence {

namespace {

// The GET response is a one-row PostgREST array holding the base64 state blob; 256 KiB is
// far above the worst case (a bounded 256-entry history + the small settings blob).
constexpr int kFetchBufferBytes = 256 * 1024;

// The leaderboard RPC returns up to 100 rows of {rank, display_name (<=32), lifetime};
// 64 KiB is far above the worst case.
constexpr int kLeaderboardBufferBytes = 64 * 1024;

[[nodiscard]] std::string anon_key() { return std::string{kSupabaseAnonKey}; }

[[nodiscard]] std::string account_base() {
    return std::string{kSupabaseUrl} + std::string{kSupabaseRestPrefix} +
           std::string{kSupabaseAccountTable};
}

[[nodiscard]] std::string read_session_name() {
    std::string buf(512, '\0');
    pt_supabase_session_name(buf.data(), static_cast<int>(buf.size()));
    buf.resize(std::char_traits<char>::length(buf.c_str()));
    return buf;
}

}  // namespace

FetchResult SupabaseSyncBackend::fetch(std::string_view auth0_user_id) {
    const std::string url = account_base() +
                            std::string{kSupabaseAccountSelectColumns} +
                            url_encode_component(auth0_user_id);
    std::string buf(static_cast<std::size_t>(kFetchBufferBytes), '\0');
    const int status =
        pt_supabase_get(url.c_str(), anon_key().c_str(), buf.data(),
                        kFetchBufferBytes);
    buf.resize(std::char_traits<char>::length(buf.c_str()));
    return parse_fetch_response(status, buf);
}

bool SupabaseSyncBackend::push(std::string_view auth0_user_id,
                               std::span<const AppState> ordered_writes) {
    if (ordered_writes.empty()) {
        return true;  // nothing to push is trivially accepted
    }
    // Each queued write is a full snapshot; the newest one is the authoritative
    // server state, so a single upsert of the last element durably reflects the
    // whole ordered batch.
    const AppState& newest = ordered_writes.back();
    const std::string url =
        account_base() + std::string{kSupabaseAccountUpsertQuery};
    const bool opted_in = pt_read_leaderboard_opt_in() != 0;
    const std::string body = build_push_body(auth0_user_id, newest, opted_in);
    return supabase_write_ok(
        pt_supabase_upsert(url.c_str(), anon_key().c_str(), body.c_str()));
}

bool SupabaseSyncBackend::upload_initial(std::string_view auth0_user_id,
                                         const AccountMigrationState& initial) {
    const std::string url =
        account_base() + std::string{kSupabaseAccountUpsertQuery};
    const bool opted_in = pt_read_leaderboard_opt_in() != 0;
    const std::string body =
        build_initial_body(auth0_user_id, initial, read_session_name(), opted_in);
    return supabase_write_ok(
        pt_supabase_upsert(url.c_str(), anon_key().c_str(), body.c_str()));
}

bool SupabaseSyncBackend::delete_account_state(std::string_view auth0_user_id) {
    const std::string url = account_base() + "?auth0_sub=eq." +
                            url_encode_component(auth0_user_id);
    return supabase_write_ok(
        pt_supabase_delete(url.c_str(), anon_key().c_str()));
}

LeaderboardFetchResult SupabaseSyncBackend::fetch_leaderboard() {
    const std::string url =
        std::string{kSupabaseUrl} + std::string{kSupabaseLeaderboardRpcPath};
    std::string buf(static_cast<std::size_t>(kLeaderboardBufferBytes), '\0');
    // The RPC takes no arguments; an empty JSON object is the PostgREST RPC body.
    const int status = pt_supabase_post_json(url.c_str(), anon_key().c_str(), "{}",
                                             buf.data(), kLeaderboardBufferBytes);
    buf.resize(std::char_traits<char>::length(buf.c_str()));
    return parse_leaderboard_response(status, buf);
}

bool SupabaseSyncBackend::delete_auth0_user() {
    // The Edge Function derives the caller's `sub` from the verified bearer; the client
    // sends only the bearer (attached inside pt_supabase_post_json) and an empty body,
    // and ignores the response beyond its status.
    const std::string url = std::string{kSupabaseUrl} +
                            std::string{kSupabaseFunctionsPrefix} +
                            std::string{kSupabaseDeleteAuth0UserFn};
    const int status =
        pt_supabase_post_json(url.c_str(), anon_key().c_str(), "{}", nullptr, 0);
    return supabase_write_ok(status);
}

}  // namespace poker_trainer::persistence

#endif  // __EMSCRIPTEN__
