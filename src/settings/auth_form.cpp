#include "settings/auth_form.hpp"

#include <cctype>
#include <string>
#include <string_view>

namespace poker_trainer::settings {

namespace {

// True when `s` holds at least one non-whitespace character (spec: a field "contains
// input"). A field of only spaces is not input.
[[nodiscard]] bool has_input(std::string_view s) noexcept {
    for (const char c : s) {
        if (std::isspace(static_cast<unsigned char>(c)) == 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

bool sign_in_submittable(std::string_view id_or_email, std::string_view password) noexcept {
    return has_input(id_or_email) && has_input(password);
}

bool sign_up_submittable(std::string_view username, std::string_view email,
                         std::string_view password, bool age_consent,
                         bool tos_consent) noexcept {
    return has_input(username) && has_input(email) && has_input(password) &&
           password.size() >= kMinPasswordLength && age_consent && tos_consent;
}

bool is_valid_email_format(std::string_view email) noexcept {
    const std::size_t at = email.find('@');
    if (at == std::string_view::npos || at == 0) {
        return false;  // missing '@', or empty local part
    }
    if (email.find('@', at + 1) != std::string_view::npos) {
        return false;  // more than one '@'
    }
    const std::string_view domain = email.substr(at + 1);
    const std::size_t dot = domain.find('.');
    if (dot == std::string_view::npos || dot == 0 || dot + 1 == domain.size()) {
        return false;  // domain has no interior '.' (no "x.y")
    }
    for (const char c : email) {
        if (std::isspace(static_cast<unsigned char>(c)) != 0) {
            return false;  // no whitespace anywhere
        }
    }
    return true;
}

AuthErrorDisplay describe_outcome(AuthOutcome outcome, AuthMode mode) noexcept {
    switch (outcome) {
        case AuthOutcome::Success:
            return AuthErrorDisplay{};
        case AuthOutcome::InvalidCredentials:
            // Sign In only (Sign Up never reports this); inline below the password.
            return AuthErrorDisplay{.use_banner = false,
                                    .field = AuthField::Password,
                                    .message = "Incorrect email or password."};
        case AuthOutcome::AccountExists:
            // Sign Up only; the EMAIL is taken — inline below the email field.
            return AuthErrorDisplay{.use_banner = false,
                                    .field = AuthField::Email,
                                    .message = "An account with that email already exists."};
        case AuthOutcome::UsernameExists:
            // Sign Up only; the USERNAME is taken — inline below the username field.
            return AuthErrorDisplay{
                .use_banner = false,
                .field = AuthField::Username,
                .message = "That username is already taken. Please choose another."};
        case AuthOutcome::SignupRejected:
            // Auth0 won't say whether the email or username is the duplicate (enumeration
            // prevention); a neutral general message that blames neither field.
            return AuthErrorDisplay{
                .use_banner = false,
                .field = AuthField::None,
                .message = "Could not create the account. That email or username may "
                           "already be in use."};
        case AuthOutcome::WeakPassword:
            // Sign Up only; inline below the password field.
            return AuthErrorDisplay{
                .use_banner = false,
                .field = AuthField::Password,
                .message = "Password is too weak. Use at least 8 characters with a mix of "
                           "letters, numbers, and symbols."};
        case AuthOutcome::InvalidEmail:
            // Sign Up only; inline below the email field.
            return AuthErrorDisplay{.use_banner = false,
                                    .field = AuthField::Email,
                                    .message = "Enter a valid email address."};
        case AuthOutcome::RateLimited:
            // Not field-specific; a general inline message (not the banner — the user can
            // proceed once the throttle clears, it is not a service outage).
            return AuthErrorDisplay{
                .use_banner = false,
                .field = AuthField::None,
                .message = "Too many attempts. Please wait a moment and try again."};
        case AuthOutcome::AccessBlocked:
            return AuthErrorDisplay{
                .use_banner = false,
                .field = AuthField::None,
                .message = "This account or request has been blocked. Contact support if "
                           "this continues."};
        case AuthOutcome::NetworkError:
        case AuthOutcome::ServiceUnavailable:
            // Service-down: the outage banner, not an inline message (Service Outage
            // Banner spec — "distinction from credential failures"). Wording matches the
            // banner triggers section, by source view.
            return AuthErrorDisplay{
                .use_banner = true,
                .field = AuthField::None,
                .message = mode == AuthMode::SignIn
                               ? "Sign in temporarily unavailable. Please try again later."
                               : "Sign up temporarily unavailable. Please try again later."};
        case AuthOutcome::NotAuthenticated:
        case AuthOutcome::Unknown:
            break;
    }
    // Any other Auth0-reported failure (Unknown / NotAuthenticated): a general inline
    // message rendered at the form level (field = None).
    return AuthErrorDisplay{.use_banner = false,
                            .field = AuthField::None,
                            .message = "Something went wrong. Please try again."};
}

std::string mask_email(std::string_view email) {
    const std::size_t at = email.find('@');
    const std::string_view local = at == std::string_view::npos ? email : email.substr(0, at);
    const std::string_view domain = at == std::string_view::npos ? std::string_view{} : email.substr(at);

    std::string out;
    if (local.empty()) {
        out += "***";
    } else {
        out.push_back(local.front());
        out += "***";
    }
    out += domain;
    return out;
}

}  // namespace poker_trainer::settings
