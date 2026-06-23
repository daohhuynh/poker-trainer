#include "settings/auth_form.hpp"

#include <gtest/gtest.h>

namespace pt = poker_trainer::settings;

// ----- Sign In submit gating: both fields must contain input -----

TEST(SignInSubmittable, BothFieldsFilledEnables) {
    EXPECT_TRUE(pt::sign_in_submittable("jane@example.com", "hunter2!"));
}

TEST(SignInSubmittable, EmptyEitherFieldDisables) {
    EXPECT_FALSE(pt::sign_in_submittable("", "hunter2!"));
    EXPECT_FALSE(pt::sign_in_submittable("jane@example.com", ""));
    EXPECT_FALSE(pt::sign_in_submittable("", ""));
}

TEST(SignInSubmittable, WhitespaceOnlyIsNotInput) {
    EXPECT_FALSE(pt::sign_in_submittable("   ", "hunter2!"));
    EXPECT_FALSE(pt::sign_in_submittable("jane@example.com", "  "));
}

// ----- Sign Up submit gating: 3 fields + 8-char password + both consents -----

TEST(SignUpSubmittable, AllConditionsMetEnables) {
    EXPECT_TRUE(pt::sign_up_submittable("RiverRat", "r@example.com", "password", true, true));
}

TEST(SignUpSubmittable, PasswordBelowMinimumDisables) {
    // 7 chars: below the 8-char minimum.
    EXPECT_FALSE(pt::sign_up_submittable("RiverRat", "r@example.com", "short77", true, true));
}

TEST(SignUpSubmittable, ExactlyEightCharPasswordEnables) {
    EXPECT_TRUE(pt::sign_up_submittable("RiverRat", "r@example.com", "12345678", true, true));
}

TEST(SignUpSubmittable, MissingEitherConsentDisables) {
    EXPECT_FALSE(pt::sign_up_submittable("RiverRat", "r@example.com", "password", false, true));
    EXPECT_FALSE(pt::sign_up_submittable("RiverRat", "r@example.com", "password", true, false));
}

TEST(SignUpSubmittable, EmptyAnyFieldDisables) {
    EXPECT_FALSE(pt::sign_up_submittable("", "r@example.com", "password", true, true));
    EXPECT_FALSE(pt::sign_up_submittable("RiverRat", "", "password", true, true));
    EXPECT_FALSE(pt::sign_up_submittable("RiverRat", "r@example.com", "", true, true));
}

// ----- Client-side email-format check (Sign Up, before Auth0) -----

TEST(IsValidEmailFormat, AcceptsWellFormed) {
    EXPECT_TRUE(pt::is_valid_email_format("jane@example.com"));
    EXPECT_TRUE(pt::is_valid_email_format("a.b+tag@sub.example.co"));
}

TEST(IsValidEmailFormat, RejectsMissingAtSign) {
    EXPECT_FALSE(pt::is_valid_email_format("daohuyn"));  // the reported case
}

TEST(IsValidEmailFormat, RejectsEmptyLocalOrDomain) {
    EXPECT_FALSE(pt::is_valid_email_format("@example.com"));
    EXPECT_FALSE(pt::is_valid_email_format("jane@"));
}

TEST(IsValidEmailFormat, RejectsDomainWithoutInteriorDot) {
    EXPECT_FALSE(pt::is_valid_email_format("jane@localhost"));
    EXPECT_FALSE(pt::is_valid_email_format("jane@.com"));
    EXPECT_FALSE(pt::is_valid_email_format("jane@example."));
}

TEST(IsValidEmailFormat, RejectsMultipleAtSigns) {
    EXPECT_FALSE(pt::is_valid_email_format("a@b@example.com"));
}

TEST(IsValidEmailFormat, RejectsWhitespaceAndEmpty) {
    EXPECT_FALSE(pt::is_valid_email_format("jane @example.com"));
    EXPECT_FALSE(pt::is_valid_email_format(""));
}

// ----- Error outcome mapping: inline vs banner, field placement -----

TEST(DescribeOutcome, SuccessHasNoDisplay) {
    const pt::AuthErrorDisplay d = pt::describe_outcome(pt::AuthOutcome::Success, pt::AuthMode::SignIn);
    EXPECT_FALSE(d.use_banner);
    EXPECT_EQ(d.field, pt::AuthField::None);
    EXPECT_TRUE(d.message.empty());
}

TEST(DescribeOutcome, InvalidCredentialsIsInlineUnderPassword) {
    const pt::AuthErrorDisplay d =
        pt::describe_outcome(pt::AuthOutcome::InvalidCredentials, pt::AuthMode::SignIn);
    EXPECT_FALSE(d.use_banner);
    EXPECT_EQ(d.field, pt::AuthField::Password);
    EXPECT_FALSE(d.message.empty());
}

TEST(DescribeOutcome, AccountExistsIsInlineUnderEmail) {
    const pt::AuthErrorDisplay d =
        pt::describe_outcome(pt::AuthOutcome::AccountExists, pt::AuthMode::SignUp);
    EXPECT_FALSE(d.use_banner);
    EXPECT_EQ(d.field, pt::AuthField::Email);
    EXPECT_FALSE(d.message.empty());
}

TEST(DescribeOutcome, UsernameExistsIsInlineUnderUsernameNotEmail) {
    // The reported bug: a taken username must render under the username field, not email.
    const pt::AuthErrorDisplay d =
        pt::describe_outcome(pt::AuthOutcome::UsernameExists, pt::AuthMode::SignUp);
    EXPECT_FALSE(d.use_banner);
    EXPECT_EQ(d.field, pt::AuthField::Username);
    EXPECT_NE(d.field, pt::AuthField::Email);
    EXPECT_FALSE(d.message.empty());
}

TEST(DescribeOutcome, SignupRejectedIsInlineGeneral) {
    const pt::AuthErrorDisplay d =
        pt::describe_outcome(pt::AuthOutcome::SignupRejected, pt::AuthMode::SignUp);
    EXPECT_FALSE(d.use_banner);
    EXPECT_EQ(d.field, pt::AuthField::None);
    EXPECT_FALSE(d.message.empty());
}

TEST(DescribeOutcome, ServiceDownUsesBannerWithModeSpecificCopy) {
    const pt::AuthErrorDisplay si =
        pt::describe_outcome(pt::AuthOutcome::NetworkError, pt::AuthMode::SignIn);
    EXPECT_TRUE(si.use_banner);
    EXPECT_EQ(si.message, "Sign in temporarily unavailable. Please try again later.");

    const pt::AuthErrorDisplay su =
        pt::describe_outcome(pt::AuthOutcome::ServiceUnavailable, pt::AuthMode::SignUp);
    EXPECT_TRUE(su.use_banner);
    EXPECT_EQ(su.message, "Sign up temporarily unavailable. Please try again later.");
}

TEST(DescribeOutcome, UnknownIsInlineGeneral) {
    const pt::AuthErrorDisplay d = pt::describe_outcome(pt::AuthOutcome::Unknown, pt::AuthMode::SignUp);
    EXPECT_FALSE(d.use_banner);
    EXPECT_EQ(d.field, pt::AuthField::None);
    EXPECT_FALSE(d.message.empty());
}

TEST(DescribeOutcome, WeakPasswordIsInlineUnderPassword) {
    const pt::AuthErrorDisplay d =
        pt::describe_outcome(pt::AuthOutcome::WeakPassword, pt::AuthMode::SignUp);
    EXPECT_FALSE(d.use_banner);
    EXPECT_EQ(d.field, pt::AuthField::Password);
    EXPECT_FALSE(d.message.empty());
}

TEST(DescribeOutcome, InvalidEmailIsInlineUnderEmail) {
    const pt::AuthErrorDisplay d =
        pt::describe_outcome(pt::AuthOutcome::InvalidEmail, pt::AuthMode::SignUp);
    EXPECT_FALSE(d.use_banner);
    EXPECT_EQ(d.field, pt::AuthField::Email);
    EXPECT_FALSE(d.message.empty());
}

TEST(DescribeOutcome, RateLimitedIsInlineGeneralNotBanner) {
    const pt::AuthErrorDisplay d =
        pt::describe_outcome(pt::AuthOutcome::RateLimited, pt::AuthMode::SignIn);
    EXPECT_FALSE(d.use_banner);  // not a service outage; user can retry
    EXPECT_EQ(d.field, pt::AuthField::None);
    EXPECT_FALSE(d.message.empty());
}

TEST(DescribeOutcome, AccessBlockedIsInlineGeneral) {
    const pt::AuthErrorDisplay d =
        pt::describe_outcome(pt::AuthOutcome::AccessBlocked, pt::AuthMode::SignIn);
    EXPECT_FALSE(d.use_banner);
    EXPECT_EQ(d.field, pt::AuthField::None);
    EXPECT_FALSE(d.message.empty());
}

// ----- Email masking -----

TEST(MaskEmail, KeepsFirstCharAndDomain) {
    EXPECT_EQ(pt::mask_email("jane@example.com"), "j***@example.com");
}

TEST(MaskEmail, SingleCharLocalPart) {
    EXPECT_EQ(pt::mask_email("a@example.com"), "a***@example.com");
}

TEST(MaskEmail, EmptyLocalPartMasksToStars) {
    EXPECT_EQ(pt::mask_email("@example.com"), "***@example.com");
}

TEST(MaskEmail, NoAtSignTreatedAsBareLocal) {
    EXPECT_EQ(pt::mask_email("jane"), "j***");
}

TEST(MaskEmail, EmptyInput) {
    EXPECT_EQ(pt::mask_email(""), "***");
}
