#pragma once

// Test doubles for Zone 04's three seams plus the injected clock, and a few
// shared fixtures helpers. Production swaps these for the real IDBFS / Auth0 /
// server backends with no change to Z04 logic.

#include "persistence/auth.hpp"
#include "persistence/clock.hpp"
#include "persistence/idbfs.hpp"
#include "persistence/migration.hpp"
#include "persistence/persistence_schema.hpp"
#include "persistence/sync.hpp"

#include "engine/scenario_id.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace poker_trainer::persistence::test {

// In-memory storage seam. Keeps the single current blob plus a full write log
// so tests can assert exactly what was — and was not — persisted.
class MemoryStorage final : public StorageBackend {
public:
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read()
        const override {
        return blob_;
    }

    void write(std::span<const std::uint8_t> bytes) override {
        blob_ = std::vector<std::uint8_t>(bytes.begin(), bytes.end());
        write_log_.push_back(*blob_);
    }

    void clear() override {
        blob_.reset();
        cleared_ = true;
    }

    [[nodiscard]] const std::optional<std::vector<std::uint8_t>>& blob()
        const noexcept {
        return blob_;
    }
    [[nodiscard]] const std::vector<std::vector<std::uint8_t>>& writes()
        const noexcept {
        return write_log_;
    }
    [[nodiscard]] bool cleared() const noexcept { return cleared_; }

    // Inject a raw blob directly (used to simulate a corrupt store).
    void set_raw(std::vector<std::uint8_t> raw) { blob_ = std::move(raw); }

private:
    std::optional<std::vector<std::uint8_t>> blob_;
    std::vector<std::vector<std::uint8_t>> write_log_;
    bool cleared_{false};
};

// Controllable monotonic clock — no real waits anywhere.
class ManualClock final : public Clock {
public:
    [[nodiscard]] std::chrono::steady_clock::time_point now()
        const noexcept override {
        return now_;
    }

    void advance(std::chrono::nanoseconds delta) noexcept { now_ += delta; }

private:
    std::chrono::steady_clock::time_point now_{};
};

// Auth0 seam mock: configurable health and success/failure per operation,
// recording the inputs it received so tests can prove credentials are
// forwarded across the seam (and, elsewhere, never persisted).
class MockAuthBackend final : public AuthBackend {
public:
    bool healthy{true};
    int health_calls{0};

    AuthSession session{"sub-123", "Alice", "alice@example.com", "token-abc"};
    bool sign_in_ok{true};
    bool sign_up_ok{true};
    bool sign_out_ok{true};
    bool delete_ok{true};
    bool change_password_ok{true};
    // Stay-signed-in: off by default (most fixtures are guests / interactive
    // sign-in). A test that exercises restore sets restore_ok = true; the
    // restored identity is `session`.
    bool restore_ok{false};
    int restore_calls{0};
    AuthError error{AuthError::Unknown};

    std::vector<std::string> passwords_seen;
    std::vector<std::string> display_names_seen;
    std::string change_password_email;
    std::string deleted_user_id;

    [[nodiscard]] bool health_check() noexcept override {
        ++health_calls;
        return healthy;
    }

    [[nodiscard]] std::expected<AuthSession, AuthError> sign_in(
        const AuthCredentials& credentials) override {
        passwords_seen.push_back(credentials.password);
        if (!sign_in_ok) {
            return std::unexpected(error);
        }
        return session;
    }

    [[nodiscard]] std::optional<AuthSession> restore_session() override {
        ++restore_calls;
        if (!restore_ok) {
            return std::nullopt;
        }
        return session;
    }

    [[nodiscard]] std::expected<AuthSession, AuthError> sign_up(
        const AuthCredentials& credentials,
        std::string_view display_name) override {
        passwords_seen.push_back(credentials.password);
        display_names_seen.emplace_back(display_name);
        if (!sign_up_ok) {
            return std::unexpected(error);
        }
        return session;
    }

    [[nodiscard]] std::expected<void, AuthError> sign_out() override {
        if (!sign_out_ok) {
            return std::unexpected(error);
        }
        return {};
    }

    [[nodiscard]] std::expected<void, AuthError> delete_account(
        std::string_view auth0_user_id) override {
        deleted_user_id = std::string(auth0_user_id);
        if (!delete_ok) {
            return std::unexpected(error);
        }
        return {};
    }

    [[nodiscard]] std::expected<void, AuthError> change_password(
        std::string_view email) override {
        change_password_email = std::string(email);
        if (!change_password_ok) {
            return std::unexpected(error);
        }
        return {};
    }
};

// Server seam mock: programmable fetch outcome, push success, and upload
// success, recording every call (and its ordered payload) for assertions.
class MockSyncBackend final : public SyncBackend {
public:
    FetchResult fetch_result{FetchOutcome::Failed, AppState{}};
    int fetch_calls{0};

    bool push_ok{true};
    struct PushRecord {
        std::string user;
        std::vector<AppState> writes;
        bool accepted{false};
    };
    std::vector<PushRecord> pushes;

    bool upload_ok{true};
    struct UploadRecord {
        std::string user;
        AccountMigrationState payload;
    };
    std::vector<UploadRecord> uploads;

    bool delete_ok{true};
    std::vector<std::string> deletes;  // user ids whose server row was deleted

    // delete-auth0-user Edge Function recorder. Programmable success; counts calls so
    // the delete flow can be asserted to invoke it (and to proceed when it fails).
    bool delete_auth0_ok{true};
    int delete_auth0_calls{0};

    [[nodiscard]] FetchResult fetch(std::string_view auth0_user_id) override {
        ++fetch_calls;
        static_cast<void>(auth0_user_id);
        return fetch_result;
    }

    [[nodiscard]] bool push(
        std::string_view auth0_user_id,
        std::span<const AppState> ordered_writes) override {
        pushes.push_back(PushRecord{
            std::string(auth0_user_id),
            std::vector<AppState>(ordered_writes.begin(), ordered_writes.end()),
            push_ok});
        return push_ok;
    }

    [[nodiscard]] bool upload_initial(
        std::string_view auth0_user_id,
        const AccountMigrationState& initial) override {
        uploads.push_back(UploadRecord{std::string(auth0_user_id), initial});
        return upload_ok;
    }

    [[nodiscard]] bool delete_account_state(
        std::string_view auth0_user_id) override {
        deletes.emplace_back(auth0_user_id);
        return delete_ok;
    }

    [[nodiscard]] bool delete_auth0_user() override {
        ++delete_auth0_calls;
        return delete_auth0_ok;
    }
};

// Field-by-field AppState equality (the contract struct defines none). Used to
// assert serialization losslessness and reconciliation adoption.
[[nodiscard]] inline bool app_states_equal(const AppState& a,
                                           const AppState& b) {
    if (a.schema_version != b.schema_version) return false;
    if (a.settings_blob != b.settings_blob) return false;
    if (a.tomatoes.spendable != b.tomatoes.spendable) return false;
    if (a.tomatoes.lifetime != b.tomatoes.lifetime) return false;
    if (a.music_library.unlocked_track_ids !=
        b.music_library.unlocked_track_ids)
        return false;
    if (a.music_library.active_pool_track_ids !=
        b.music_library.active_pool_track_ids)
        return false;
    if (a.scenario_history.size() != b.scenario_history.size()) return false;
    for (std::size_t i = 0; i < a.scenario_history.size(); ++i) {
        const ScenarioHistoryEntry& x = a.scenario_history[i];
        const ScenarioHistoryEntry& y = b.scenario_history[i];
        if (!(x.scenario_id == y.scenario_id && x.passed == y.passed &&
              x.elapsed_ms == y.elapsed_ms &&
              x.completed_at == y.completed_at))
            return false;
    }
    if (a.account.is_authenticated != b.account.is_authenticated) return false;
    if (a.account.auth0_user_id != b.account.auth0_user_id) return false;
    if (a.account.display_name != b.account.display_name) return false;
    if (a.account.email != b.account.email) return false;
    if (a.tutorial.has_seen_tutorial_prompt !=
        b.tutorial.has_seen_tutorial_prompt)
        return false;
    if (a.tutorial.has_completed_tutorial != b.tutorial.has_completed_tutorial)
        return false;
    return true;
}

// A fully-populated AppState exercising every field (high-bit bytes, max-ish
// integers, multiple history entries, non-empty account, set flags).
[[nodiscard]] inline AppState make_populated_state() {
    AppState state{};
    state.schema_version = kCurrentSchemaVersion;
    state.settings_blob = {0x00, 0x01, 0x7F, 0x80, 0xFF, 0x42, 0x00};
    state.tomatoes.spendable = 12345;
    state.tomatoes.lifetime = 9876543210ULL;
    state.music_library.unlocked_track_ids = {1, 3, 5, 7, 11};
    state.music_library.active_pool_track_ids = {0, 3, 7};
    for (std::uint32_t i = 0; i < 3; ++i) {
        ScenarioHistoryEntry entry{};
        entry.scenario_id =
            engine::ScenarioId{1000ULL + static_cast<std::uint64_t>(i)};
        entry.passed = (i % 2u == 0u);
        entry.elapsed_ms = 5000u + i;
        entry.completed_at = std::chrono::steady_clock::time_point(
            std::chrono::nanoseconds(123456789LL + static_cast<long long>(i)));
        state.scenario_history.push_back(entry);
    }
    state.account.is_authenticated = true;
    state.account.auth0_user_id = "auth0|abc123";
    state.account.display_name = "TestUser";
    state.account.email = "test@example.com";
    state.tutorial.has_seen_tutorial_prompt = true;
    state.tutorial.has_completed_tutorial = false;
    return state;
}

// True if haystack contains needle as a raw byte subsequence.
[[nodiscard]] inline bool bytes_contain(std::span<const std::uint8_t> haystack,
                                        std::string_view needle) {
    if (needle.empty()) {
        return true;
    }
    return std::search(haystack.begin(), haystack.end(), needle.begin(),
                       needle.end(),
                       [](std::uint8_t lhs, char rhs) {
                           return lhs == static_cast<std::uint8_t>(rhs);
                       }) != haystack.end();
}

}  // namespace poker_trainer::persistence::test
