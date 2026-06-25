// Supabase SyncBackend boundary tests — the pure, browser-free contract: base64
// round-trip, RLS request-body / URL shaping, HTTP-status -> FetchOutcome
// translation, and the GET-response -> FetchResult reconcile parse. The DOM glue
// (XHR + the Auth0 id_token bearer) is browser-only and is Dao's live manual
// verification; everything asserted here compiles and runs natively.

#include "persistence/supabase_backend.hpp"

#include "persistence/idbfs.hpp"
#include "persistence/migration.hpp"
#include "persistence/persistence_schema.hpp"
#include "persistence/sync.hpp"

#include "persistence_mocks.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace pt = poker_trainer::persistence;

namespace {

std::vector<std::uint8_t> bytes_of(std::initializer_list<int> values) {
    std::vector<std::uint8_t> out;
    for (const int v : values) {
        out.push_back(static_cast<std::uint8_t>(v));
    }
    return out;
}

}  // namespace

// --- base64 ---

TEST(SupabaseBase64, KnownRfcVectors) {
    EXPECT_EQ(pt::base64_encode(bytes_of({'M', 'a', 'n'})), "TWFu");
    EXPECT_EQ(pt::base64_encode(bytes_of({'M', 'a'})), "TWE=");
    EXPECT_EQ(pt::base64_encode(bytes_of({'M'})), "TQ==");
    EXPECT_TRUE(pt::base64_encode(std::vector<std::uint8_t>{}).empty());
}

TEST(SupabaseBase64, RoundTripsEveryTailLength) {
    for (std::size_t n = 0; n <= 9; ++n) {
        std::vector<std::uint8_t> data;
        for (std::size_t i = 0; i < n; ++i) {
            data.push_back(static_cast<std::uint8_t>((i * 37u + 5u) & 0xFFu));
        }
        const std::string encoded = pt::base64_encode(data);
        const std::optional<std::vector<std::uint8_t>> decoded =
            pt::base64_decode(encoded);
        ASSERT_TRUE(decoded.has_value()) << "n=" << n;
        EXPECT_EQ(*decoded, data) << "n=" << n;
    }
}

TEST(SupabaseBase64, RoundTripsHighBitBytes) {
    const std::vector<std::uint8_t> data =
        bytes_of({0x00, 0xFF, 0x80, 0x7F, 0x01, 0xFE, 0xAA});
    const std::optional<std::vector<std::uint8_t>> decoded =
        pt::base64_decode(pt::base64_encode(data));
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, data);
}

TEST(SupabaseBase64, DecodeRejectsBadLengthAndIllegalChars) {
    EXPECT_FALSE(pt::base64_decode("TWF").has_value());      // length % 4 != 0
    EXPECT_FALSE(pt::base64_decode("TW*u").has_value());     // illegal char
    EXPECT_FALSE(pt::base64_decode("####").has_value());     // all illegal
}

// --- request shaping (RLS key + columns) ---

TEST(SupabaseShaping, UrlEncodesAuth0SubPipe) {
    // The Auth0 sub carries a "|" that must be percent-encoded in the RLS filter.
    EXPECT_EQ(pt::url_encode_component("auth0|abc123"), "auth0%7Cabc123");
    EXPECT_EQ(pt::url_encode_component("google-oauth2|9.~_-"),
              "google-oauth2%7C9.~_-");
}

TEST(SupabaseShaping, JsonEscapeHandlesQuotesBackslashControl) {
    EXPECT_EQ(pt::json_escape("a\"b\\c"), "a\\\"b\\\\c");
    EXPECT_EQ(pt::json_escape(std::string_view("x\ny", 3)), "x\\ny");
}

TEST(SupabaseShaping, UpsertBodyCarriesEveryColumn) {
    const std::string body =
        pt::build_account_upsert_body("auth0|u1", "QkxPQg==", 4242, "Alice");
    EXPECT_NE(body.find("\"auth0_sub\":\"auth0|u1\""), std::string::npos);
    EXPECT_NE(body.find("\"state_blob\":\"QkxPQg==\""), std::string::npos);
    EXPECT_NE(body.find("\"lifetime_tomatoes\":4242"), std::string::npos);
    EXPECT_NE(body.find("\"display_name\":\"Alice\""), std::string::npos);
}

TEST(SupabaseShaping, UpsertBodyEscapesDisplayName) {
    const std::string body =
        pt::build_account_upsert_body("auth0|u1", "Qg==", 1, "A\"B");
    EXPECT_NE(body.find("\"display_name\":\"A\\\"B\""), std::string::npos);
}

TEST(SupabaseShaping, PushBodyEmbedsTheFullSerializedState) {
    // The push body carries the whole AppState (base64 in state_blob); decoding it
    // back must reproduce the snapshot exactly — that is what crosses browsers.
    const pt::AppState state = pt::test::make_populated_state();
    const std::string body = pt::build_push_body("auth0|u1", state);

    const std::optional<std::string> blob =
        pt::extract_json_string_field(body, "state_blob");
    ASSERT_TRUE(blob.has_value());
    const std::optional<std::vector<std::uint8_t>> raw = pt::base64_decode(*blob);
    ASSERT_TRUE(raw.has_value());
    const pt::DeserializeResult parsed = pt::deserialize_app_state(*raw);
    ASSERT_EQ(parsed.result, pt::SchemaValidationResult::Ok);
    EXPECT_TRUE(pt::test::app_states_equal(parsed.state, state));

    // The denormalized leaderboard column matches the blob's lifetime total.
    EXPECT_NE(body.find("\"lifetime_tomatoes\":" +
                        std::to_string(state.tomatoes.lifetime)),
              std::string::npos);
}

TEST(SupabaseShaping, InitialBodyCarriesExactlyTheThreeMigrationFields) {
    const pt::AccountMigrationState initial{120, 500, {1, 3, 5}};
    const std::string body =
        pt::build_initial_body("auth0|new", initial, "Bob");

    const std::optional<std::string> blob =
        pt::extract_json_string_field(body, "state_blob");
    ASSERT_TRUE(blob.has_value());
    const std::optional<std::vector<std::uint8_t>> raw = pt::base64_decode(*blob);
    ASSERT_TRUE(raw.has_value());
    const pt::DeserializeResult parsed = pt::deserialize_app_state(*raw);
    ASSERT_EQ(parsed.result, pt::SchemaValidationResult::Ok);

    // Exactly the three Module 7 fields; everything else default.
    EXPECT_EQ(parsed.state.tomatoes.spendable, 120u);
    EXPECT_EQ(parsed.state.tomatoes.lifetime, 500u);
    EXPECT_EQ(parsed.state.music_library.unlocked_track_ids,
              (std::vector<std::uint8_t>{1, 3, 5}));
    EXPECT_FALSE(parsed.state.account.is_authenticated);
    EXPECT_TRUE(parsed.state.scenario_history.empty());
    EXPECT_NE(body.find("\"lifetime_tomatoes\":500"), std::string::npos);
    EXPECT_NE(body.find("\"display_name\":\"Bob\""), std::string::npos);
}

// --- response translation ---

TEST(SupabaseTranslate, FetchOutcomeFromStatusAndRowPresence) {
    EXPECT_EQ(pt::supabase_fetch_outcome(200, true), pt::FetchOutcome::Found);
    EXPECT_EQ(pt::supabase_fetch_outcome(200, false), pt::FetchOutcome::NotFound);
    EXPECT_EQ(pt::supabase_fetch_outcome(0, true), pt::FetchOutcome::Failed);
    EXPECT_EQ(pt::supabase_fetch_outcome(401, true), pt::FetchOutcome::Failed);
    EXPECT_EQ(pt::supabase_fetch_outcome(403, false), pt::FetchOutcome::Failed);
    EXPECT_EQ(pt::supabase_fetch_outcome(500, true), pt::FetchOutcome::Failed);
}

TEST(SupabaseTranslate, WriteOkOnlyForTwoXx) {
    EXPECT_TRUE(pt::supabase_write_ok(200));
    EXPECT_TRUE(pt::supabase_write_ok(201));
    EXPECT_TRUE(pt::supabase_write_ok(204));
    EXPECT_FALSE(pt::supabase_write_ok(0));
    EXPECT_FALSE(pt::supabase_write_ok(400));
    EXPECT_FALSE(pt::supabase_write_ok(401));
    EXPECT_FALSE(pt::supabase_write_ok(409));
    EXPECT_FALSE(pt::supabase_write_ok(500));
}

TEST(SupabaseTranslate, ExtractJsonStringField) {
    EXPECT_EQ(pt::extract_json_string_field("[{\"state_blob\":\"ABC==\"}]",
                                            "state_blob"),
              std::optional<std::string>{"ABC=="});
    EXPECT_FALSE(
        pt::extract_json_string_field("[{\"other\":\"x\"}]", "state_blob")
            .has_value());
    EXPECT_FALSE(pt::extract_json_string_field("[]", "state_blob").has_value());
}

TEST(SupabaseTranslate, ParseFetchResponseFoundRoundTrips) {
    // The full reconcile read: a 200 with the PostgREST one-row array reproduces
    // the server's authoritative state for adoption.
    const pt::AppState server_state = pt::test::make_populated_state();
    const std::string blob_b64 =
        pt::base64_encode(pt::serialize_app_state(server_state));
    const std::string response = "[{\"state_blob\":\"" + blob_b64 + "\"}]";

    const pt::FetchResult result = pt::parse_fetch_response(200, response);
    EXPECT_EQ(result.outcome, pt::FetchOutcome::Found);
    EXPECT_TRUE(pt::test::app_states_equal(result.state, server_state));
}

TEST(SupabaseTranslate, ParseFetchResponseEmptyArrayIsNotFound) {
    const pt::FetchResult result = pt::parse_fetch_response(200, "[]");
    EXPECT_EQ(result.outcome, pt::FetchOutcome::NotFound);
}

TEST(SupabaseTranslate, ParseFetchResponseNon2xxIsFailed) {
    EXPECT_EQ(pt::parse_fetch_response(401, "{\"message\":\"JWT\"}").outcome,
              pt::FetchOutcome::Failed);
    EXPECT_EQ(pt::parse_fetch_response(0, "").outcome, pt::FetchOutcome::Failed);
    EXPECT_EQ(pt::parse_fetch_response(500, "").outcome,
              pt::FetchOutcome::Failed);
}

TEST(SupabaseTranslate, ParseFetchResponseUndecodableBlobIsFailed) {
    // A present-but-corrupt blob must NOT NotFound (which would migrate local over
    // real server data) and must NOT adopt garbage.
    const pt::FetchResult result =
        pt::parse_fetch_response(200, "[{\"state_blob\":\"@@@@\"}]");
    EXPECT_EQ(result.outcome, pt::FetchOutcome::Failed);
}
