#pragma once

#include "settings/auth_form.hpp"

#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"

#include "bridge/focus_registry.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

// Zone 12 — the Account section's auth flows: the Sign In / Sign Up modal content
// (rendered into Zone 11's auth_modals shell via the generic content-provider seam),
// the boot-injected Zone 04 seams, and the action helpers the Settings Account section
// (settings_modal.cpp) calls. ARCHITECTURE "Account Settings" / "Account Creation Flow"
// / the Sign In + Sign Up modal contents.
//
// The `settings` library does NOT link Zone 04 persistence (its auth flows arrive
// through boot-injected callbacks, exactly as Part 1's persist / apply_audio do). So the
// seams below speak in Zone-12 types (AuthOutcome, AccountSnapshot); boot translates
// to/from persistence at the wiring point. With every seam null the page still renders
// and degrades to no-ops — this is the state before a production Auth0 backend is built.

namespace poker_trainer::settings {

struct SettingsModalState;  // settings_modal.hpp (the Account section host)

// Read-only view of the persisted account identity (persistence::AccountState), surfaced
// without a persistence link. Guest by default (every field empty / false).
struct AccountSnapshot {
    bool is_authenticated{false};
    std::string display_name;
    std::string email;
};

// Spendable + Lifetime tomatoes, for the View Profile panel (one of the three sanctioned
// tomato-display surfaces — Shop, Profile, Leaderboard).
struct WalletSnapshot {
    std::uint64_t spendable{0};
    std::uint64_t lifetime{0};
};

// Which legal document a consent inline-link opens.
enum class LegalDoc : std::uint8_t { Terms = 0, Privacy = 1 };

// The Zone-04 / Zone-05 injected seams, wired once at boot. All optional; a null seam is
// a graceful no-op (health_check null is treated as healthy so the modal still opens for
// visual verification before the backend exists).
struct AccountSeams {
    std::function<AccountSnapshot()> account{};
    std::function<WalletSnapshot()> wallet{};
    std::function<bool()> health_check{};
    std::function<AuthOutcome(std::string_view id_or_email, std::string_view password)> sign_in{};
    std::function<AuthOutcome(std::string_view username, std::string_view email,
                              std::string_view password)>
        sign_up{};
    std::function<void()> sign_out{};
    std::function<void()> delete_account{};
    std::function<void()> change_password{};                 // logged-in reset email
    std::function<void(std::string_view email)> reset_password{};  // logged-out forgot
    std::function<void(LegalDoc)> open_legal{};
};

// Which link in the ToS/Privacy consent row is arrow-highlighted (Category A positional
// toggle). None = no highlight set; Enter/Space in this state toggles the checkbox.
// Terms/Privacy = the respective doc link is highlighted; Enter/Space opens it.
enum class TosHighlight : std::uint8_t { None, Terms, Privacy };

// Per-open state for the auth (Sign In / Sign Up) modal. Owns its OWN focus registry,
// never the app-root or the Settings one (the shared-registry-clobber rule).
struct AccountModalState {
    AccountSeams seams{};

    bridge::FocusRegistry own_registry{};
    bridge::FocusRegistry* focus_registry{nullptr};
    backbone::FocusableId last_synced_focus{backbone::kNoFocus};

    // Form field buffers (no heap during typing).
    std::array<char, 128> id_or_email_buf{};   // Sign In identifier (email OR username)
    std::array<char, 64> username_buf{};       // Sign Up display name
    std::array<char, 128> email_buf{};         // Sign Up email
    std::array<char, 64> password_buf{};
    bool show_password{false};
    bool consent_age{false};
    bool consent_tos{false};

    // Arrow highlight for the ToS/Privacy consent grouped stop (Category A positional toggle).
    TosHighlight tos_highlight{TosHighlight::None};

    // Inline state_fail error attached under one field, set on a failed submit / denylist.
    AuthField error_field{AuthField::None};
    std::string error_message{};

    // Forgot-password inline sub-view (Sign In only).
    bool forgot_open{false};
    bool forgot_sent{false};
    std::array<char, 128> forgot_email_buf{};

    // Deferred actions (keyboard activate closures raise these; the dispatch handler runs
    // them AFTER dispatch_focus_key returns, so no registry-clearing close runs mid-closure
    // — mirrors Part 1's request_close).
    bool request_close{false};
    bool request_submit{false};
    bool request_relayout{false};
    enum class Layout : std::uint8_t { SignIn, SignUp, Forgot };
    Layout pending_layout{Layout::SignIn};
};

// ----- Boot wiring -----

// Register the auth-modal content provider with Zone 11 (kAuthModalId) and wire the
// consent inline-links to the Settings legal-doc modal. Call once at boot after the
// seams are filled and after install_settings_content. `settings` is captured so the
// ToS / Privacy links can reuse the existing legal-doc sub-modal.
void install_account_content(AccountModalState& account, SettingsModalState& settings);

// ----- Action helpers, called by the Settings Account section (settings_modal.cpp) -----

// Current identity (guest default when the seam is unwired).
[[nodiscard]] AccountSnapshot account_snapshot(const AccountModalState& a);
[[nodiscard]] WalletSnapshot account_wallet(const AccountModalState& a);

// Health-check-gated modal opens (ARCHITECTURE Account Settings). On a failed check the
// outage banner is triggered instead of opening the modal. A null health_check seam is
// treated as healthy (so the modal opens for visual verification pre-backend).
void account_open_sign_in(AccountModalState& a);
void account_open_sign_up(AccountModalState& a);

void account_sign_out(AccountModalState& a);

// Open the red (state_fail) delete-account confirmation; Yes calls the delete seam.
void account_confirm_delete(AccountModalState& a);

// Open the "we'll email a reset link" confirmation; Yes triggers the reset email.
void account_confirm_change_password(AccountModalState& a);

}  // namespace poker_trainer::settings
