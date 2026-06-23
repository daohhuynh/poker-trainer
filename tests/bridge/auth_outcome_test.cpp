#include "bridge/auth_outcome.hpp"

#include "persistence/auth.hpp"
#include "settings/auth_form.hpp"

#include <gtest/gtest.h>

namespace br = poker_trainer::bridge;
namespace pt = poker_trainer::persistence;
namespace st = poker_trainer::settings;

// The AuthError -> AuthOutcome boundary translation boot uses to feed the Z12 form layer.

TEST(ToAuthOutcome, InvalidCredentials) {
    EXPECT_EQ(br::to_auth_outcome(pt::AuthError::InvalidCredentials),
              st::AuthOutcome::InvalidCredentials);
}

TEST(ToAuthOutcome, AccountExists) {
    EXPECT_EQ(br::to_auth_outcome(pt::AuthError::AccountExists), st::AuthOutcome::AccountExists);
}

TEST(ToAuthOutcome, NetworkErrorMapsToBannerCategory) {
    // NetworkError + ServiceUnavailable are the service-down outcomes the form turns into
    // the outage banner.
    EXPECT_EQ(br::to_auth_outcome(pt::AuthError::NetworkError), st::AuthOutcome::NetworkError);
    EXPECT_EQ(br::to_auth_outcome(pt::AuthError::ServiceUnavailable),
              st::AuthOutcome::ServiceUnavailable);
}

TEST(ToAuthOutcome, NotAuthenticated) {
    EXPECT_EQ(br::to_auth_outcome(pt::AuthError::NotAuthenticated),
              st::AuthOutcome::NotAuthenticated);
}

TEST(ToAuthOutcome, WeakPassword) {
    EXPECT_EQ(br::to_auth_outcome(pt::AuthError::WeakPassword), st::AuthOutcome::WeakPassword);
}

TEST(ToAuthOutcome, InvalidEmail) {
    EXPECT_EQ(br::to_auth_outcome(pt::AuthError::InvalidEmail), st::AuthOutcome::InvalidEmail);
}

TEST(ToAuthOutcome, RateLimited) {
    EXPECT_EQ(br::to_auth_outcome(pt::AuthError::RateLimited), st::AuthOutcome::RateLimited);
}

TEST(ToAuthOutcome, AccessBlocked) {
    EXPECT_EQ(br::to_auth_outcome(pt::AuthError::AccessBlocked), st::AuthOutcome::AccessBlocked);
}

TEST(ToAuthOutcome, UsernameExists) {
    EXPECT_EQ(br::to_auth_outcome(pt::AuthError::UsernameExists), st::AuthOutcome::UsernameExists);
}

TEST(ToAuthOutcome, SignupRejected) {
    EXPECT_EQ(br::to_auth_outcome(pt::AuthError::SignupRejected), st::AuthOutcome::SignupRejected);
}

TEST(ToAuthOutcome, Unknown) {
    EXPECT_EQ(br::to_auth_outcome(pt::AuthError::Unknown), st::AuthOutcome::Unknown);
}
