#include "persistence/idbfs.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace poker_trainer::persistence {

namespace {

// On-disk blob layout (little-endian throughout for determinism across the
// native test host and the wasm deployment target):
//
//   [0..3]   magic 'P','T','A','S'
//   [4..7]   u32 schema version
//   [8..N)   field payload (see write_body / read_body)
//   [N..N+8) u64 FNV-1a checksum over bytes [0..N)
//
// The checksum lets the load path distinguish a structurally intact blob from
// a truncated / corrupted one (SchemaValidationResult::Corrupt).

constexpr std::array<std::uint8_t, 4> kMagic{
    std::uint8_t{'P'}, std::uint8_t{'T'}, std::uint8_t{'A'}, std::uint8_t{'S'}};

constexpr std::size_t kHeaderSize = kMagic.size() + sizeof(std::uint32_t);
constexpr std::size_t kChecksumSize = sizeof(std::uint64_t);

constexpr std::uint64_t kFnvOffsetBasis = 0xcbf29ce484222325ULL;
constexpr std::uint64_t kFnvPrime = 0x100000001b3ULL;

[[nodiscard]] std::uint64_t fnv1a(std::span<const std::uint8_t> data) noexcept {
    std::uint64_t hash = kFnvOffsetBasis;
    for (const std::uint8_t byte : data) {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= kFnvPrime;
    }
    return hash;
}

void put_u8(std::vector<std::uint8_t>& out, std::uint8_t value) {
    out.push_back(value);
}

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (unsigned i = 0; i < sizeof(value); ++i) {
        out.push_back(static_cast<std::uint8_t>((value >> (8u * i)) & 0xFFu));
    }
}

void put_u64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (unsigned i = 0; i < sizeof(value); ++i) {
        out.push_back(static_cast<std::uint8_t>((value >> (8u * i)) & 0xFFu));
    }
}

void put_i64(std::vector<std::uint8_t>& out, std::int64_t value) {
    put_u64(out, std::bit_cast<std::uint64_t>(value));
}

void put_bool(std::vector<std::uint8_t>& out, bool value) {
    put_u8(out, value ? std::uint8_t{1} : std::uint8_t{0});
}

void put_bytes(std::vector<std::uint8_t>& out,
               std::span<const std::uint8_t> bytes) {
    put_u32(out, static_cast<std::uint32_t>(bytes.size()));
    out.insert(out.end(), bytes.begin(), bytes.end());
}

void put_string(std::vector<std::uint8_t>& out, const std::string& value) {
    put_u32(out, static_cast<std::uint32_t>(value.size()));
    for (const char c : value) {
        out.push_back(static_cast<std::uint8_t>(c));
    }
}

// Bounds-checked sequential reader. Any read past the end latches the reader
// into a failed state and yields zero/empty values; callers check ok() once at
// the end rather than after every field.
class ByteReader {
public:
    explicit ByteReader(std::span<const std::uint8_t> data) noexcept
        : data_(data) {}

    [[nodiscard]] std::uint8_t u8() noexcept {
        if (!need(1)) {
            return 0;
        }
        return data_[pos_++];
    }

    [[nodiscard]] std::uint32_t u32() noexcept {
        if (!need(sizeof(std::uint32_t))) {
            return 0;
        }
        std::uint32_t value = 0;
        for (unsigned i = 0; i < sizeof(std::uint32_t); ++i) {
            value |= static_cast<std::uint32_t>(data_[pos_ + i]) << (8u * i);
        }
        pos_ += sizeof(std::uint32_t);
        return value;
    }

    [[nodiscard]] std::uint64_t u64() noexcept {
        if (!need(sizeof(std::uint64_t))) {
            return 0;
        }
        std::uint64_t value = 0;
        for (unsigned i = 0; i < sizeof(std::uint64_t); ++i) {
            value |= static_cast<std::uint64_t>(data_[pos_ + i]) << (8u * i);
        }
        pos_ += sizeof(std::uint64_t);
        return value;
    }

    [[nodiscard]] std::int64_t i64() noexcept {
        return std::bit_cast<std::int64_t>(u64());
    }

    [[nodiscard]] bool boolean() noexcept { return u8() != 0; }

    [[nodiscard]] std::vector<std::uint8_t> bytes() {
        const std::uint32_t len = u32();
        if (!need(len)) {
            return {};
        }
        const std::span<const std::uint8_t> chunk = data_.subspan(pos_, len);
        std::vector<std::uint8_t> result(chunk.begin(), chunk.end());
        pos_ += len;
        return result;
    }

    [[nodiscard]] std::string string() {
        const std::uint32_t len = u32();
        if (!need(len)) {
            return {};
        }
        std::string result;
        result.reserve(len);
        for (std::uint32_t i = 0; i < len; ++i) {
            result.push_back(static_cast<char>(data_[pos_ + i]));
        }
        pos_ += len;
        return result;
    }

    [[nodiscard]] bool ok() const noexcept { return ok_; }
    [[nodiscard]] bool fully_consumed() const noexcept {
        return ok_ && pos_ == data_.size();
    }

private:
    [[nodiscard]] bool need(std::size_t count) noexcept {
        if (!ok_ || pos_ + count > data_.size()) {
            ok_ = false;
            return false;
        }
        return true;
    }

    std::span<const std::uint8_t> data_;
    std::size_t pos_{0};
    bool ok_{true};
};

void write_body(std::vector<std::uint8_t>& out, const AppState& state) {
    put_bytes(out, state.settings_blob);

    put_u64(out, state.tomatoes.spendable);
    put_u64(out, state.tomatoes.lifetime);

    put_bytes(out, state.music_library.unlocked_track_ids);
    put_bytes(out, state.music_library.active_pool_track_ids);

    put_u32(out, static_cast<std::uint32_t>(state.scenario_history.size()));
    for (const ScenarioHistoryEntry& entry : state.scenario_history) {
        put_u64(out, entry.scenario_id.value);
        put_bool(out, entry.passed);
        put_u32(out, entry.elapsed_ms);
        put_i64(out, static_cast<std::int64_t>(
                         entry.completed_at.time_since_epoch().count()));
    }

    put_bool(out, state.account.is_authenticated);
    put_string(out, state.account.auth0_user_id);
    put_string(out, state.account.display_name);
    put_string(out, state.account.email);

    put_bool(out, state.tutorial.has_seen_tutorial_prompt);
    put_bool(out, state.tutorial.has_completed_tutorial);
}

// Reconstruct the field payload. Returns true only when every field read
// inside the body succeeded and the body was fully consumed.
[[nodiscard]] bool read_body(ByteReader& reader, AppState& out) {
    out.settings_blob = reader.bytes();

    out.tomatoes.spendable = reader.u64();
    out.tomatoes.lifetime = reader.u64();

    out.music_library.unlocked_track_ids = reader.bytes();
    out.music_library.active_pool_track_ids = reader.bytes();

    const std::uint32_t history_count = reader.u32();
    out.scenario_history.clear();
    // Guard the count against the declared bound before reserving, so a
    // corrupt length can never drive a runaway allocation.
    if (history_count > kMaxScenarioHistoryEntries) {
        return false;
    }
    out.scenario_history.reserve(history_count);
    for (std::uint32_t i = 0; i < history_count; ++i) {
        ScenarioHistoryEntry entry{};
        entry.scenario_id.value = reader.u64();
        entry.passed = reader.boolean();
        entry.elapsed_ms = reader.u32();
        const std::int64_t rep = reader.i64();
        entry.completed_at = std::chrono::steady_clock::time_point(
            std::chrono::steady_clock::duration(
                static_cast<std::chrono::steady_clock::duration::rep>(rep)));
        out.scenario_history.push_back(std::move(entry));
    }

    out.account.is_authenticated = reader.boolean();
    out.account.auth0_user_id = reader.string();
    out.account.display_name = reader.string();
    out.account.email = reader.string();

    out.tutorial.has_seen_tutorial_prompt = reader.boolean();
    out.tutorial.has_completed_tutorial = reader.boolean();

    return reader.fully_consumed();
}

}  // namespace

std::vector<std::uint8_t> serialize_app_state(const AppState& state) {
    std::vector<std::uint8_t> out;
    out.insert(out.end(), kMagic.begin(), kMagic.end());
    put_u32(out, kCurrentSchemaVersion);
    write_body(out, state);
    put_u64(out, fnv1a(out));
    return out;
}

DeserializeResult deserialize_app_state(std::span<const std::uint8_t> bytes) {
    DeserializeResult result{};

    if (bytes.size() < kHeaderSize + kChecksumSize) {
        result.result = SchemaValidationResult::Corrupt;
        return result;
    }

    if (!std::equal(kMagic.begin(), kMagic.end(), bytes.begin())) {
        result.result = SchemaValidationResult::Corrupt;
        return result;
    }

    const std::size_t payload_size = bytes.size() - kChecksumSize;
    const std::uint64_t computed = fnv1a(bytes.first(payload_size));
    ByteReader checksum_reader(bytes.subspan(payload_size, kChecksumSize));
    const std::uint64_t stored = checksum_reader.u64();
    if (computed != stored) {
        result.result = SchemaValidationResult::Corrupt;
        return result;
    }

    ByteReader version_reader(bytes.subspan(kMagic.size(), sizeof(std::uint32_t)));
    const std::uint32_t version = version_reader.u32();

    switch (validate_schema_version(version)) {
        case SchemaValidationResult::Ok: {
            AppState state{};
            ByteReader body_reader(bytes.subspan(
                kHeaderSize, payload_size - kHeaderSize));
            if (!read_body(body_reader, state)) {
                result.result = SchemaValidationResult::Corrupt;
                return result;
            }
            state.schema_version = version;
            result.result = SchemaValidationResult::Ok;
            result.state = std::move(state);
            return result;
        }
        case SchemaValidationResult::NeedsMigration:
            // Unreachable while kMinSupportedSchemaVersion ==
            // kCurrentSchemaVersion (no prior on-disk format exists). When a
            // v2 schema is introduced, register the v1->v2 transform here and
            // parse with the versioned reader before upgrading. Until then a
            // blob claiming an in-range older version cannot be interpreted,
            // so it is reported as such and the caller falls back to fresh.
            result.result = SchemaValidationResult::NeedsMigration;
            return result;
        case SchemaValidationResult::Unsupported:
        case SchemaValidationResult::Corrupt:
            result.result = SchemaValidationResult::Unsupported;
            return result;
    }

    result.result = SchemaValidationResult::Corrupt;
    return result;
}

IdbfsStore::IdbfsStore(StorageBackend& storage) noexcept : storage_(storage) {}

AppState IdbfsStore::load_state() {
    const std::optional<std::vector<std::uint8_t>> blob = storage_.read();
    if (!blob.has_value()) {
        // First launch: nothing persisted yet.
        state_ = AppState{};
        return state_;
    }

    const DeserializeResult parsed = deserialize_app_state(*blob);
    if (parsed.result == SchemaValidationResult::Ok) {
        state_ = parsed.state;
    } else {
        // Corrupt / unsupported / unmigratable blob: fall back to a fresh
        // state rather than bricking the app. A logged-in user repopulates
        // from the server during session-start reconciliation.
        state_ = AppState{};
    }
    return state_;
}

void IdbfsStore::save_state(const AppState& state) {
    state_ = state;
    persist();
}

void IdbfsStore::adopt_server_state(const AppState& server_state) {
    // Server is the source of truth for everything except the local authenticated
    // identity (is_authenticated, auth0_user_id, email), which is pinned by the live
    // Auth0 session. The display_name is the EXCEPTION to that pin: it is the user's
    // chosen username, owned by the server (the public leaderboard identity). The live
    // session's "name" claim is the email, so adopting it would leak the email onto the
    // board; instead take the server's display_name when present.
    AccountState account = state_.account;
    if (!server_state.account.display_name.empty()) {
        account.display_name = server_state.account.display_name;
    }
    state_ = server_state;
    state_.account = account;
    state_.schema_version = kCurrentSchemaVersion;
    persist();
}

void IdbfsStore::update_account(const AccountState& account) {
    state_.account = account;
    persist();
}

void IdbfsStore::wipe() {
    storage_.clear();
    state_ = AppState{};
}

void IdbfsStore::mark_tutorial_prompt_seen() {
    state_.tutorial.has_seen_tutorial_prompt = true;
    persist();
}

void IdbfsStore::mark_tutorial_completed() {
    state_.tutorial.has_completed_tutorial = true;
    persist();
}

void IdbfsStore::persist() {
    const std::vector<std::uint8_t> bytes = serialize_app_state(state_);
    storage_.write(bytes);
}

}  // namespace poker_trainer::persistence
