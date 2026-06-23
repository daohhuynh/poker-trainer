#include "persistence/auth0_backend.hpp"

#include "persistence/auth.hpp"

#include <gtest/gtest.h>

namespace pt = poker_trainer::persistence;

// The bridge-code -> AuthError boundary contract (the JS side promises to return exactly
// these codes). The Auth0Backend class itself is browser-only; this mapping is pure.

TEST(Auth0CodeToError, InvalidCredentials) {
    EXPECT_EQ(pt::auth0_code_to_error(static_cast<int>(pt::Auth0BridgeCode::InvalidCredentials)),
              pt::AuthError::InvalidCredentials);
}

TEST(Auth0CodeToError, NetworkError) {
    EXPECT_EQ(pt::auth0_code_to_error(static_cast<int>(pt::Auth0BridgeCode::NetworkError)),
              pt::AuthError::NetworkError);
}

TEST(Auth0CodeToError, ServiceUnavailable) {
    EXPECT_EQ(pt::auth0_code_to_error(static_cast<int>(pt::Auth0BridgeCode::ServiceUnavailable)),
              pt::AuthError::ServiceUnavailable);
}

TEST(Auth0CodeToError, AccountExists) {
    EXPECT_EQ(pt::auth0_code_to_error(static_cast<int>(pt::Auth0BridgeCode::AccountExists)),
              pt::AuthError::AccountExists);
}

TEST(Auth0CodeToError, WeakPassword) {
    EXPECT_EQ(pt::auth0_code_to_error(static_cast<int>(pt::Auth0BridgeCode::WeakPassword)),
              pt::AuthError::WeakPassword);
}

TEST(Auth0CodeToError, InvalidEmail) {
    EXPECT_EQ(pt::auth0_code_to_error(static_cast<int>(pt::Auth0BridgeCode::InvalidEmail)),
              pt::AuthError::InvalidEmail);
}

TEST(Auth0CodeToError, RateLimited) {
    EXPECT_EQ(pt::auth0_code_to_error(static_cast<int>(pt::Auth0BridgeCode::RateLimited)),
              pt::AuthError::RateLimited);
}

TEST(Auth0CodeToError, AccessBlocked) {
    EXPECT_EQ(pt::auth0_code_to_error(static_cast<int>(pt::Auth0BridgeCode::AccessBlocked)),
              pt::AuthError::AccessBlocked);
}

TEST(Auth0CodeToError, UsernameExistsIsDistinctFromAccountExists) {
    EXPECT_EQ(pt::auth0_code_to_error(static_cast<int>(pt::Auth0BridgeCode::UsernameExists)),
              pt::AuthError::UsernameExists);
    // Must NOT collapse into AccountExists (the reported bug).
    EXPECT_NE(pt::auth0_code_to_error(static_cast<int>(pt::Auth0BridgeCode::UsernameExists)),
              pt::AuthError::AccountExists);
}

TEST(Auth0CodeToError, SignupRejected) {
    EXPECT_EQ(pt::auth0_code_to_error(static_cast<int>(pt::Auth0BridgeCode::SignupRejected)),
              pt::AuthError::SignupRejected);
}

TEST(Auth0CodeToError, UnknownCodeFoldsToUnknown) {
    EXPECT_EQ(pt::auth0_code_to_error(static_cast<int>(pt::Auth0BridgeCode::Unknown)),
              pt::AuthError::Unknown);
}

TEST(Auth0CodeToError, SuccessFoldsToUnknownDefensively) {
    // Code 0 is success and carries no error; callers translate only the failure case, so a
    // defensive translation never yields a bogus categorized failure.
    EXPECT_EQ(pt::auth0_code_to_error(static_cast<int>(pt::Auth0BridgeCode::Success)),
              pt::AuthError::Unknown);
}

TEST(Auth0CodeToError, OutOfRangeCodeFoldsToUnknown) {
    EXPECT_EQ(pt::auth0_code_to_error(99), pt::AuthError::Unknown);
    EXPECT_EQ(pt::auth0_code_to_error(-1), pt::AuthError::Unknown);
}
