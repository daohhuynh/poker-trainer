#include "persistence/migration.hpp"

#include "persistence/sync.hpp"

#include <string_view>

namespace poker_trainer::persistence {

Migrator::Migrator(SyncBackend& server) noexcept : server_(server) {}

AccountMigrationState Migrator::build_migration_state(
    const AppState& local_state) {
    // The three Module 7 migration value fields — Spendable Tomatoes, Lifetime
    // Tomatoes, and the unlocked-tracks list — plus the account display name, which
    // seeds the server row's display_name column. Shuffle pool, settings, history,
    // and tutorial state are deliberately excluded. display_name is read from the
    // persisted account (the chosen username, pinned at sign-up) — the SAME source
    // the regular push uses — so the initial upload never seeds the email-derived
    // id_token name claim.
    AccountMigrationState initial{};
    initial.spendable = local_state.tomatoes.spendable;
    initial.lifetime = local_state.tomatoes.lifetime;
    initial.unlocked_track_ids = local_state.music_library.unlocked_track_ids;
    initial.display_name = local_state.account.display_name;
    return initial;
}

bool Migrator::migrate(const AppState& local_state,
                       std::string_view auth0_user_id) {
    const AccountMigrationState initial = build_migration_state(local_state);
    return server_.upload_initial(auth0_user_id, initial);
}

}  // namespace poker_trainer::persistence
