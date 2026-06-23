#pragma once

#include "persistence/auth.hpp"      // persistence::AuthError
#include "settings/auth_form.hpp"    // settings::AuthOutcome

// Zone 05 glue — the auth-result boundary translation. The Z12 Account seams speak in
// settings::AuthOutcome (the UI's vocabulary); Z04 speaks in persistence::AuthError. Boot
// wires the seams to PersistenceService and translates here. Kept as a header-only inline
// function (rather than buried in the wasm-only boot.cpp) so the mapping is unit-tested.

namespace poker_trainer::bridge {

// Map a Z04 AuthError to the Z12 AuthOutcome the form layer renders. (Success is not an
// error and has no AuthError; callers translate only the failure case.)
[[nodiscard]] constexpr settings::AuthOutcome to_auth_outcome(
    persistence::AuthError error) noexcept {
    switch (error) {
        case persistence::AuthError::InvalidCredentials:
            return settings::AuthOutcome::InvalidCredentials;
        case persistence::AuthError::AccountExists:
            return settings::AuthOutcome::AccountExists;
        case persistence::AuthError::NetworkError:
            return settings::AuthOutcome::NetworkError;
        case persistence::AuthError::ServiceUnavailable:
            return settings::AuthOutcome::ServiceUnavailable;
        case persistence::AuthError::NotAuthenticated:
            return settings::AuthOutcome::NotAuthenticated;
        case persistence::AuthError::WeakPassword:
            return settings::AuthOutcome::WeakPassword;
        case persistence::AuthError::InvalidEmail:
            return settings::AuthOutcome::InvalidEmail;
        case persistence::AuthError::RateLimited:
            return settings::AuthOutcome::RateLimited;
        case persistence::AuthError::AccessBlocked:
            return settings::AuthOutcome::AccessBlocked;
        case persistence::AuthError::UsernameExists:
            return settings::AuthOutcome::UsernameExists;
        case persistence::AuthError::SignupRejected:
            return settings::AuthOutcome::SignupRejected;
        case persistence::AuthError::Unknown:
            break;
    }
    return settings::AuthOutcome::Unknown;
}

}  // namespace poker_trainer::bridge
