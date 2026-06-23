#pragma once

#include <cstdint>
#include <string>
#include <string_view>

// Zone 12 — pure auth-form logic (ARCHITECTURE "Sign In modal" / "Sign Up modal" /
// "Account Creation Flow"). The form's decisions with no ImGui and no Auth0 link:
// submit-enable gating, the 8-character password minimum, the inline-error wording for
// each failure, the email mask, and the Sign In <-> Sign Up view mode. Unit-tested.
//
// The `settings` library deliberately does NOT link Zone 04 persistence (its auth flows
// arrive through boot-injected callbacks). So this layer speaks in a Zone-12-local
// AuthOutcome enum, NOT persistence::AuthError; boot translates one to the other when it
// wires the seams. Keeping the boundary here means a credential failure's USER-FACING
// COPY lives in Zone 12 (the UI), while Zone 04 only reports the categorized outcome.

namespace poker_trainer::settings {

// Which view the (single, content-swapping) auth modal is showing.
enum class AuthMode : std::uint8_t { SignIn = 0, SignUp = 1 };

// Categorized result of a submitted auth operation, mirroring persistence::AuthError
// across the no-persistence-link boundary (boot maps AuthError -> AuthOutcome). Success
// is the happy path; the rest drive either an inline state_fail message or the outage
// banner (service-down, per the Service Outage Banner spec's "distinction" note).
enum class AuthOutcome : std::uint8_t {
    Success = 0,
    InvalidCredentials = 1,  // Sign In: wrong email/password (inline, below password)
    AccountExists = 2,       // Sign Up: email already in use (inline, below email)
    NetworkError = 3,        // service-down -> outage banner, not inline
    ServiceUnavailable = 4,  // service-down -> outage banner, not inline
    NotAuthenticated = 5,    // no session for an op that needs one (defensive)
    Unknown = 6,             // any other Auth0-reported failure (inline, generic)
    WeakPassword = 7,        // Sign Up: password failed the strength policy (below password)
    InvalidEmail = 8,        // Sign Up: malformed email (below email)
    RateLimited = 9,         // too many attempts / 429 (inline, general)
    AccessBlocked = 10,      // account / request blocked (inline, general)
    UsernameExists = 11,     // Sign Up: the username is taken (below username)
    SignupRejected = 12,     // Sign Up: enumeration-masked duplicate (inline, general)
};

// The 8-character password minimum enforced before Sign Up submits (Auth0 also enforces
// its own rules server-side; this is the client-side gate from the spec).
inline constexpr std::size_t kMinPasswordLength = 8;

// Where an inline error attaches in the form (the field the message renders beneath).
enum class AuthField : std::uint8_t { None = 0, Username = 1, Email = 2, Password = 3 };

// How a failed submit should surface: either the outage banner (service-down) or an
// inline state_fail message under `field`. For a banner, `message` is the banner text;
// for inline, it is the text rendered beneath the field. Empty message + None for
// AuthOutcome::Success.
struct AuthErrorDisplay {
    bool use_banner{false};
    AuthField field{AuthField::None};
    std::string_view message{};
};

// The Sign In button is enabled only when both fields contain input (spec). Whitespace-
// only input does not count as input.
[[nodiscard]] bool sign_in_submittable(std::string_view id_or_email,
                                       std::string_view password) noexcept;

// The Sign Up button is enabled only when all three fields are filled, the password meets
// the 8-character minimum, and BOTH consent checkboxes are checked (spec).
[[nodiscard]] bool sign_up_submittable(std::string_view username, std::string_view email,
                                       std::string_view password, bool age_consent,
                                       bool tos_consent) noexcept;

// Lightweight client-side email-format check, run on Sign Up submit before Auth0 (Auth0's
// /dbconnections/signup returns only a generic payload-validation 400 for a malformed
// address — no stable error code — so the reliable place to catch it is here, mirroring the
// denylist's "validate before the form reaches Auth0" rule). Requires exactly one '@' with
// a non-empty local part and a domain that contains an interior '.', and no whitespace.
// Auth0 still does the authoritative validation for anything that passes this.
[[nodiscard]] bool is_valid_email_format(std::string_view email) noexcept;

// Map a failed AuthOutcome to its surface + wording, for the given view. Returns an empty
// (Success-shaped) display for AuthOutcome::Success.
[[nodiscard]] AuthErrorDisplay describe_outcome(AuthOutcome outcome, AuthMode mode) noexcept;

// The denylist rejection message rendered beneath the username field when a Sign Up name
// matches the denylist (spec wording; the support email is a launch placeholder).
inline constexpr std::string_view kDenylistRejectionMessage =
    "This username isn't allowed. Please choose another. If you believe this is an "
    "error, contact support@poker-trainer.com.";

// The Forgot-password confirmation shown after the reset email is requested (spec).
inline constexpr std::string_view kPasswordResetSentMessage =
    "Check your email for reset instructions.";

// Partially mask an email for display: keep the first character of the local part, then
// "***", then the full domain — e.g. "jane@example.com" -> "j***@example.com". A local
// part that is empty masks to "***"; input without an '@' is treated as a bare local
// part. Used by the logged-in Account section and the Change Password confirm.
[[nodiscard]] std::string mask_email(std::string_view email);

}  // namespace poker_trainer::settings
