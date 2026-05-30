#include "persistence/migration.hpp"

#include "persistence/sync.hpp"

#include <string_view>

namespace poker_trainer::persistence {

Migrator::Migrator(SyncBackend& server) noexcept : server_(server) {}

AccountMigrationState Migrator::build_migration_state(
    const AppState& local_state) {
    // Exactly the three Module 7 migration fields — Spendable Tomatoes,
    // Lifetime Tomatoes, and the unlocked-tracks list. Shuffle pool, settings,
    // history, account, and tutorial state are deliberately excluded.
    AccountMigrationState initial{};
    initial.spendable = local_state.tomatoes.spendable;
    initial.lifetime = local_state.tomatoes.lifetime;
    initial.unlocked_track_ids = local_state.music_library.unlocked_track_ids;
    return initial;
}

bool Migrator::migrate(const AppState& local_state,
                       std::string_view auth0_user_id) {
    const AccountMigrationState initial = build_migration_state(local_state);
    return server_.upload_initial(auth0_user_id, initial);
}

}  // namespace poker_trainer::persistence
