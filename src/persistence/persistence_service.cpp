#include "persistence/persistence_service.hpp"

#include "persistence/persistence_schema.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace poker_trainer::persistence {

PersistenceService::PersistenceService(StorageBackend& storage,
                                       AuthBackend& auth, SyncBackend& server,
                                       Clock& clock) noexcept
    : store_(storage),
      sync_(server, clock),
      migrator_(server),
      auth_(auth, store_, migrator_, server, clock) {}

AppState PersistenceService::load_state() { return store_.load_state(); }

void PersistenceService::save_state(const AppState& state) {
    // IDBFS writes immediately; the server sync runs in the background for
    // logged-in users (guests have no server-side account state).
    store_.save_state(state);
    if (state.account.is_authenticated) {
        sync_.record_state_change(state.account.auth0_user_id, state);
    }
}

const AppState& PersistenceService::state() const noexcept {
    return store_.state();
}

void PersistenceService::reconcile_on_session_start() {
    if (!store_.state().account.is_authenticated) {
        return;  // guest: local IDBFS is authoritative, nothing to reconcile
    }
    const std::string user_id = store_.state().account.auth0_user_id;
    auth_.reconcile_account(user_id);
}

std::expected<void, AuthError> PersistenceService::sign_in(
    const AuthCredentials& credentials) {
    return auth_.sign_in(credentials);
}

std::expected<void, AuthError> PersistenceService::sign_up(
    const AuthCredentials& credentials, std::string_view display_name) {
    return auth_.sign_up(credentials, display_name);
}

std::expected<void, AuthError> PersistenceService::sign_out() {
    return auth_.sign_out();
}

std::expected<void, AuthError> PersistenceService::delete_account() {
    return auth_.delete_account();
}

std::expected<void, AuthError> PersistenceService::change_password() {
    return auth_.change_password();
}

bool PersistenceService::auth0_health_check() {
    return auth_.auth0_health_check();
}

bool PersistenceService::migrate_guest_to_account() {
    if (!store_.state().account.is_authenticated) {
        return false;
    }
    const std::string user_id = store_.state().account.auth0_user_id;
    return migrator_.migrate(store_.state(), user_id);
}

void PersistenceService::pump_sync() {
    if (!store_.state().account.is_authenticated) {
        return;
    }
    const std::string user_id = store_.state().account.auth0_user_id;
    sync_.pump(user_id);
}

bool PersistenceService::has_seen_tutorial_prompt() const noexcept {
    return store_.has_seen_tutorial_prompt();
}

void PersistenceService::mark_tutorial_prompt_seen() {
    store_.mark_tutorial_prompt_seen();
}

bool PersistenceService::has_completed_tutorial() const noexcept {
    return store_.has_completed_tutorial();
}

void PersistenceService::mark_tutorial_completed() {
    store_.mark_tutorial_completed();
}

}  // namespace poker_trainer::persistence
