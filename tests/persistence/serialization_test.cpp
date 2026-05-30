// AppState serialization round-trip and corruption-detection tests.

#include "persistence/idbfs.hpp"

#include "persistence/persistence_schema.hpp"

#include "persistence_mocks.hpp"

#include <cstdint>
#include <span>
#include <vector>

#include <gtest/gtest.h>

namespace pt = poker_trainer::persistence;

namespace {

// Mirrors the on-disk FNV-1a so a tampered blob can be given a *valid*
// checksum — required to reach the version-validation branches rather than
// tripping the checksum guard first.
std::uint64_t reference_fnv1a(std::span<const std::uint8_t> data) {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    for (const std::uint8_t byte : data) {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

void rewrite_trailing_checksum(std::vector<std::uint8_t>& blob) {
    const std::uint64_t checksum = reference_fnv1a(
        std::span<const std::uint8_t>(blob.data(), blob.size() - 8));
    for (std::size_t i = 0; i < 8; ++i) {
        blob[blob.size() - 8 + i] =
            static_cast<std::uint8_t>((checksum >> (8u * i)) & 0xFFu);
    }
}

void append_u32(std::vector<std::uint8_t>& blob, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) {
        blob.push_back(static_cast<std::uint8_t>((value >> (8u * i)) & 0xFFu));
    }
}

void append_u64(std::vector<std::uint8_t>& blob, std::uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) {
        blob.push_back(static_cast<std::uint8_t>((value >> (8u * i)) & 0xFFu));
    }
}

}  // namespace

TEST(Serialization, RoundTripPopulatedIsLossless) {
    const pt::AppState original = pt::test::make_populated_state();
    const std::vector<std::uint8_t> blob = pt::serialize_app_state(original);
    const pt::DeserializeResult parsed = pt::deserialize_app_state(blob);

    EXPECT_EQ(parsed.result, pt::SchemaValidationResult::Ok);
    EXPECT_TRUE(pt::test::app_states_equal(parsed.state, original));
}

TEST(Serialization, RoundTripDefaultIsLossless) {
    const pt::AppState original{};
    const std::vector<std::uint8_t> blob = pt::serialize_app_state(original);
    const pt::DeserializeResult parsed = pt::deserialize_app_state(blob);

    EXPECT_EQ(parsed.result, pt::SchemaValidationResult::Ok);
    EXPECT_TRUE(pt::test::app_states_equal(parsed.state, original));
}

TEST(Serialization, WritesCurrentSchemaVersion) {
    pt::AppState original{};
    original.schema_version = 999;  // writer must ignore this and emit current
    const std::vector<std::uint8_t> blob = pt::serialize_app_state(original);
    const pt::DeserializeResult parsed = pt::deserialize_app_state(blob);

    EXPECT_EQ(parsed.result, pt::SchemaValidationResult::Ok);
    EXPECT_EQ(parsed.state.schema_version, pt::kCurrentSchemaVersion);
}

TEST(Serialization, DetectsTruncation) {
    std::vector<std::uint8_t> blob =
        pt::serialize_app_state(pt::test::make_populated_state());
    blob.resize(blob.size() - 5);  // drop part of the checksum/payload
    EXPECT_EQ(pt::deserialize_app_state(blob).result,
              pt::SchemaValidationResult::Corrupt);
}

TEST(Serialization, DetectsPayloadBitFlip) {
    std::vector<std::uint8_t> blob =
        pt::serialize_app_state(pt::test::make_populated_state());
    blob[20] ^= 0xFF;  // flip a payload byte; checksum no longer matches
    EXPECT_EQ(pt::deserialize_app_state(blob).result,
              pt::SchemaValidationResult::Corrupt);
}

TEST(Serialization, DetectsBadMagic) {
    std::vector<std::uint8_t> blob =
        pt::serialize_app_state(pt::test::make_populated_state());
    blob[0] = 0x00;  // corrupt the magic tag
    EXPECT_EQ(pt::deserialize_app_state(blob).result,
              pt::SchemaValidationResult::Corrupt);
}

TEST(Serialization, DetectsUndersizedBlob) {
    const std::vector<std::uint8_t> blob{0x50, 0x54, 0x41};  // shorter than hdr
    EXPECT_EQ(pt::deserialize_app_state(blob).result,
              pt::SchemaValidationResult::Corrupt);
}

TEST(Serialization, DetectsEmptyBlob) {
    const std::vector<std::uint8_t> blob{};
    EXPECT_EQ(pt::deserialize_app_state(blob).result,
              pt::SchemaValidationResult::Corrupt);
}

TEST(Serialization, RejectsUnsupportedVersionWithValidChecksum) {
    std::vector<std::uint8_t> blob =
        pt::serialize_app_state(pt::test::make_populated_state());
    // Patch the version field (offset 4) to an out-of-range value, then fix
    // the checksum so the blob is structurally valid but version-rejected.
    blob[4] = 0xE7;  // 999 little-endian
    blob[5] = 0x03;
    blob[6] = 0x00;
    blob[7] = 0x00;
    rewrite_trailing_checksum(blob);
    EXPECT_EQ(pt::deserialize_app_state(blob).result,
              pt::SchemaValidationResult::Unsupported);
}

TEST(Serialization, RejectsOversizeHistoryCount) {
    // A version-valid, checksum-valid blob whose declared history count exceeds
    // kMaxScenarioHistoryEntries must be rejected as Corrupt, never trusted
    // into a runaway allocation. Build the minimal body by hand: empty
    // settings, zero tomatoes, empty track lists, then an absurd history count.
    std::vector<std::uint8_t> blob{0x50, 0x54, 0x41, 0x53};  // magic 'PTAS'
    append_u32(blob, pt::kCurrentSchemaVersion);
    append_u32(blob, 0);            // settings_blob length
    append_u64(blob, 0);            // tomatoes.spendable
    append_u64(blob, 0);            // tomatoes.lifetime
    append_u32(blob, 0);            // unlocked_track_ids length
    append_u32(blob, 0);            // active_pool_track_ids length
    append_u32(blob, 0xFFFFFFFFu);  // history count — far past the bound
    append_u64(blob, 0);            // checksum placeholder
    rewrite_trailing_checksum(blob);

    EXPECT_EQ(pt::deserialize_app_state(blob).result,
              pt::SchemaValidationResult::Corrupt);
}
