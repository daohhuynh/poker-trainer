#pragma once

#include "persistence/persistence_schema.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace poker_trainer::persistence {

class SyncBackend;

// The exact payload uploaded when a guest creates an account. Per Module 7
// "Account creation migration", precisely three pieces of state migrate to
// the server as the initial account state — and nothing else (no shuffle
// pool, settings, history, account, or tutorial fields).
struct AccountMigrationState {
    // Current Spendable Tomatoes balance.
    std::uint64_t spendable{0};

    // Lifetime Tomatoes total.
    std::uint64_t lifetime{0};

    // Full list of tracks unlocked via Shop purchases.
    std::vector<std::uint8_t> unlocked_track_ids;

    [[nodiscard]] bool operator==(const AccountMigrationState&) const = default;
};

// Guest -> account migrator.
//
// Builds the three-field initial account state from the guest's local AppState
// and uploads it through the sync seam. After a successful upload the server
// is authoritative for those values; the user's wallet and lifetime totals
// continue accumulating from their pre-account values (the local AppState is
// not reset — the migration seeds the server, it does not clear the client).
class Migrator {
public:
    explicit Migrator(SyncBackend& server) noexcept;

    // Extract exactly Spendable Tomatoes, Lifetime Tomatoes, and the
    // unlocked-tracks list from the local state. Pure and order-preserving;
    // the unlocked list is copied verbatim (already a sorted vector per the
    // schema contract).
    [[nodiscard]] static AccountMigrationState build_migration_state(
        const AppState& local_state);

    // Upload the initial account state for the given Auth0 user. Returns true
    // on success.
    [[nodiscard]] bool migrate(const AppState& local_state,
                               std::string_view auth0_user_id);

private:
    SyncBackend& server_;
};

}  // namespace poker_trainer::persistence
