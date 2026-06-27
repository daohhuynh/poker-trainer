#include "settings/account_modal.hpp"

#include "settings/auth_form.hpp"
#include "settings/settings_modal.hpp"
#include "settings/username_denylist.hpp"

#include "modal/auth_modals.hpp"
#include "modal/confirm_modal.hpp"
#include "modal/modals.hpp"

#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"

#include "theme/theme.hpp"
#include "theme/theme_tokens.hpp"

#include <array>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "bridge/focus_registry.hpp"

// Zone 12 — the Sign In / Sign Up modal body (rendered into Zone 11's auth_modals shell
// via the generic content-provider seam) plus the Account-section action helpers. The
// pure form decisions live in auth_form.hpp / username_denylist.hpp (unit-tested); this
// translation unit is the ImGui render glue + the deferred-dispatch wiring, browser-
// verified (CLAUDE.md §9). It mirrors settings_modal.cpp's idioms — the focus-reconcile
// substrate for text fields, a per-element FocusRegistry, click-to-snap focus, and
// deferred close/submit so no registry-clearing close runs inside a dispatch closure.

namespace poker_trainer::settings {

namespace {

// ----- auth-modal field focus ids (the form fields; the X close is modal::kAuthShellClose) -----
constexpr backbone::FocusableId kAuthIdField = backbone::make_focusable_id("auth.id");
constexpr backbone::FocusableId kAuthUsername = backbone::make_focusable_id("auth.username");
constexpr backbone::FocusableId kAuthEmail = backbone::make_focusable_id("auth.email");
constexpr backbone::FocusableId kAuthPassword = backbone::make_focusable_id("auth.password");
constexpr backbone::FocusableId kAuthShowPw = backbone::make_focusable_id("auth.showpw");
constexpr backbone::FocusableId kAuthForgotLink = backbone::make_focusable_id("auth.forgot_link");
constexpr backbone::FocusableId kAuthSubmit = backbone::make_focusable_id("auth.submit");
constexpr backbone::FocusableId kAuthSwapLink = backbone::make_focusable_id("auth.swap");
constexpr backbone::FocusableId kAuthAgeConsent = backbone::make_focusable_id("auth.age");
constexpr backbone::FocusableId kAuthTosConsent = backbone::make_focusable_id("auth.tos");
constexpr backbone::FocusableId kAuthForgotEmail = backbone::make_focusable_id("auth.forgot_email");
constexpr backbone::FocusableId kAuthForgotSend = backbone::make_focusable_id("auth.forgot_send");
constexpr backbone::FocusableId kAuthForgotBack = backbone::make_focusable_id("auth.forgot_back");

// ----- focus orders, one per layout (X close last in each, per spec, wrapping) -----
constexpr std::array<backbone::FocusableId, 7> kSignInFocus{
    kAuthIdField, kAuthPassword, kAuthShowPw, kAuthForgotLink,
    kAuthSubmit, kAuthSwapLink, modal::kAuthShellClose};
constexpr std::array<backbone::FocusableId, 9> kSignUpFocus{
    kAuthUsername, kAuthEmail, kAuthPassword, kAuthShowPw, kAuthAgeConsent,
    kAuthTosConsent, kAuthSubmit, kAuthSwapLink, modal::kAuthShellClose};
constexpr std::array<backbone::FocusableId, 4> kForgotFocus{
    kAuthForgotEmail, kAuthForgotSend, kAuthForgotBack, modal::kAuthShellClose};

[[nodiscard]] ImU32 token_u32(theme::ColorToken t) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(t));
}

void focus_on_click(backbone::FocusableId id) {
    backbone::activate_keyboard_mode();
    backbone::snap_focus_to(id);
}

[[nodiscard]] std::string_view buf_view(const char* buf) { return std::string_view{buf}; }

// ----- widgets (draw + focus ring + click snap), mirroring settings_modal.cpp -----

bool auth_text(backbone::FocusableId id, const char* imgui_id, char* buf, std::size_t len,
               const bridge::FocusReconcile& rec, ImU32 ring, float width = -1.0f,
               ImGuiInputTextFlags flags = 0) {
    bridge::grab_keyboard_if_target(rec, id);
    ImGui::SetNextItemWidth(width);
    const bool edited = ImGui::InputText(imgui_id, buf, len, flags);
    if (ImGui::IsItemClicked()) {
        focus_on_click(id);
    }
    bridge::draw_focus_ring(id, ring);
    return edited;
}

bool auth_button(backbone::FocusableId id, const char* label, ImU32 ring, bool enabled,
                 ImVec2 size = ImVec2{0.0f, 0.0f}) {
    if (!enabled) {
        ImGui::BeginDisabled();
    }
    const bool clicked = ImGui::Button(label, size);
    if (!enabled) {
        ImGui::EndDisabled();
    }
    if (ImGui::IsItemClicked()) {
        focus_on_click(id);
    }
    bridge::draw_focus_ring(id, ring);
    return clicked;
}

bool auth_checkbox(backbone::FocusableId id, const char* label, bool& value, ImU32 ring) {
    const bool changed = ImGui::Checkbox(label, &value);
    if (ImGui::IsItemClicked()) {
        focus_on_click(id);
    }
    bridge::draw_focus_ring(id, ring);
    return changed;
}

// A text link: colored text over an invisible button (no hardcoded colors; the color is a
// theme token). `id` is the focus id; `imgui_id` is the ImGui widget id; `label` is shown.
bool auth_link(backbone::FocusableId id, const char* imgui_id, const char* label,
               theme::ColorToken color, ImU32 ring) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size = ImGui::CalcTextSize(label);
    const bool clicked = ImGui::InvisibleButton(imgui_id, size);
    if (ImGui::IsItemClicked()) {
        focus_on_click(id);
    }
    ImGui::GetWindowDrawList()->AddText(pos, token_u32(color), label);
    bridge::draw_focus_ring(id, ring);
    return clicked;
}

// Like auth_link but draws a persistent underline when `highlighted` is true (the
// arrow-key positional highlight for Category A grouped stops). Also underlines on hover.
bool auth_link_highlighted(backbone::FocusableId id, const char* imgui_id, const char* label,
                           theme::ColorToken color, ImU32 ring, bool highlighted) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size = ImGui::CalcTextSize(label);
    const bool clicked = ImGui::InvisibleButton(imgui_id, size);
    if (ImGui::IsItemClicked()) {
        focus_on_click(id);
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText(pos, token_u32(color), label);
    if (highlighted || ImGui::IsItemHovered()) {
        dl->AddLine(ImVec2{pos.x, pos.y + size.y}, ImVec2{pos.x + size.x, pos.y + size.y},
                    token_u32(color), 1.0f);
    }
    bridge::draw_focus_ring(id, ring);
    return clicked;
}

// A simple procedural "person" silhouette (head + shoulders). No person AssetId exists in
// the sealed asset_paths.hpp, so the header glyph is drawn (precedent: the Z11 cluster
// glyphs). `c` is the glyph-box center; `r` half the box size.
void draw_person_glyph(ImVec2 c, float r, ImU32 col) {
    constexpr float kPi = 3.14159265358979323846f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddCircleFilled(ImVec2{c.x, c.y - r * 0.35f}, r * 0.40f, col, 16);
    dl->PathClear();
    dl->PathArcTo(ImVec2{c.x, c.y + r * 0.55f}, r * 0.62f, kPi, kPi * 2.0f, 16);
    dl->PathFillConvex(col);
}

[[nodiscard]] std::span<const backbone::FocusableId> auth_focus_list(const AccountModalState& a) {
    if (a.forgot_open) {
        return std::span<const backbone::FocusableId>(kForgotFocus);
    }
    return modal::auth_mode() == modal::AuthMode::SignUp
               ? std::span<const backbone::FocusableId>(kSignUpFocus)
               : std::span<const backbone::FocusableId>(kSignInFocus);
}

void populate_auth_registry(AccountModalState& a) {
    if (a.focus_registry == nullptr) {
        return;
    }
    bridge::FocusRegistry& reg = *a.focus_registry;
    reg.clear();

    if (a.forgot_open) {
        reg.register_element(kAuthForgotEmail, bridge::FocusableEntry{.is_text_field = true});
        // Send does not close the modal or change context, so its activate runs inline.
        reg.register_element(kAuthForgotSend, bridge::FocusableEntry{.activate = [&a] {
                                 if (a.seams.reset_password) {
                                     a.seams.reset_password(buf_view(a.forgot_email_buf.data()));
                                 }
                                 a.forgot_sent = true;
                             }});
        reg.register_element(kAuthForgotBack, bridge::FocusableEntry{.activate = [&a] {
                                 a.request_relayout = true;
                                 a.pending_layout = AccountModalState::Layout::SignIn;
                             }});
    } else if (modal::auth_mode() == modal::AuthMode::SignUp) {
        reg.register_element(kAuthUsername, bridge::FocusableEntry{.is_text_field = true});
        reg.register_element(kAuthEmail, bridge::FocusableEntry{.is_text_field = true});
        reg.register_element(kAuthPassword, bridge::FocusableEntry{.is_text_field = true});
        reg.register_element(kAuthShowPw, bridge::FocusableEntry{.activate = [&a] {
                                 a.show_password = !a.show_password;
                             }});
        reg.register_element(kAuthAgeConsent, bridge::FocusableEntry{.activate = [&a] {
                                 a.consent_age = !a.consent_age;
                             }});
        reg.register_element(kAuthTosConsent, bridge::FocusableEntry{.activate = [&a] {
                                 a.consent_tos = !a.consent_tos;
                             }});
        reg.register_element(kAuthSubmit,
                             bridge::FocusableEntry{.activate = [&a] { a.request_submit = true; }});
        reg.register_element(kAuthSwapLink, bridge::FocusableEntry{.activate = [&a] {
                                 a.request_relayout = true;
                                 a.pending_layout = AccountModalState::Layout::SignIn;
                             }});
    } else {  // Sign In
        reg.register_element(kAuthIdField, bridge::FocusableEntry{.is_text_field = true});
        reg.register_element(kAuthPassword, bridge::FocusableEntry{.is_text_field = true});
        reg.register_element(kAuthShowPw, bridge::FocusableEntry{.activate = [&a] {
                                 a.show_password = !a.show_password;
                             }});
        reg.register_element(kAuthForgotLink, bridge::FocusableEntry{.activate = [&a] {
                                 a.request_relayout = true;
                                 a.pending_layout = AccountModalState::Layout::Forgot;
                             }});
        reg.register_element(kAuthSubmit,
                             bridge::FocusableEntry{.activate = [&a] { a.request_submit = true; }});
        reg.register_element(kAuthSwapLink, bridge::FocusableEntry{.activate = [&a] {
                                 a.request_relayout = true;
                                 a.pending_layout = AccountModalState::Layout::SignUp;
                             }});
    }
    reg.register_element(modal::kAuthShellClose,
                         bridge::FocusableEntry{.activate = [&a] { a.request_close = true; }});
}

// Map an auth-call result to its UI surface: close on success, banner on service-down,
// inline state_fail otherwise.
void handle_outcome(AccountModalState& a, AuthOutcome r, AuthMode mode) {
    if (r == AuthOutcome::Success) {
        modal::close_modal();  // the Account section re-reads the snapshot next frame
        return;
    }
    const AuthErrorDisplay d = describe_outcome(r, mode);
    if (d.use_banner) {
        modal::trigger_outage_banner(d.message);
    } else {
        a.error_field = d.field;
        a.error_message = std::string{d.message};
    }
}

// Submit the active form. Denylist runs client-side BEFORE the Auth0 seam (enforcement
// order). A null seam reports ServiceUnavailable so submit never fabricates success while
// the production Auth0 backend is unbuilt.
void do_submit(AccountModalState& a) {
    a.error_field = AuthField::None;
    a.error_message.clear();

    if (modal::auth_mode() == modal::AuthMode::SignUp) {
        if (!sign_up_submittable(buf_view(a.username_buf.data()), buf_view(a.email_buf.data()),
                                 buf_view(a.password_buf.data()), a.consent_age, a.consent_tos)) {
            return;  // the button is disabled in this state; defensive
        }
        if (is_username_denylisted(buf_view(a.username_buf.data()))) {
            a.error_field = AuthField::Username;
            a.error_message = std::string{kDenylistRejectionMessage};
            return;  // never reaches Auth0
        }
        if (!is_valid_email_format(buf_view(a.email_buf.data()))) {
            // Caught client-side: Auth0's signup returns only a generic 400 for a malformed
            // email, so the specific message is reliable only here.
            a.error_field = AuthField::Email;
            a.error_message = "Enter a valid email address.";
            return;  // never reaches Auth0
        }
        const AuthOutcome r =
            a.seams.sign_up ? a.seams.sign_up(buf_view(a.username_buf.data()),
                                              buf_view(a.email_buf.data()),
                                              buf_view(a.password_buf.data()))
                            : AuthOutcome::ServiceUnavailable;
        handle_outcome(a, r, AuthMode::SignUp);
    } else {
        if (!sign_in_submittable(buf_view(a.id_or_email_buf.data()),
                                 buf_view(a.password_buf.data()))) {
            return;
        }
        const AuthOutcome r =
            a.seams.sign_in ? a.seams.sign_in(buf_view(a.id_or_email_buf.data()),
                                              buf_view(a.password_buf.data()))
                            : AuthOutcome::ServiceUnavailable;
        handle_outcome(a, r, AuthMode::SignIn);
    }
}

// Switch the in-place content (Sign In <-> Sign Up <-> Forgot): rebuild the registry and
// re-push the focus context (net-zero on the modal's single pushed context).
void do_relayout(AccountModalState& a, AccountModalState::Layout target) {
    a.error_field = AuthField::None;
    a.error_message.clear();
    a.tos_highlight = TosHighlight::None;
    switch (target) {
        case AccountModalState::Layout::SignIn:
            modal::set_auth_mode(modal::AuthMode::SignIn);
            a.forgot_open = false;
            break;
        case AccountModalState::Layout::SignUp:
            modal::set_auth_mode(modal::AuthMode::SignUp);
            a.forgot_open = false;
            break;
        case AccountModalState::Layout::Forgot:
            a.forgot_open = true;
            a.forgot_sent = false;
            // Prefill the identifier the user already typed (often their email).
            a.forgot_email_buf = {};
            {
                const std::string_view src = buf_view(a.id_or_email_buf.data());
                const std::size_t n = std::min(src.size(), a.forgot_email_buf.size() - 1);
                for (std::size_t i = 0; i < n; ++i) {
                    a.forgot_email_buf[i] = src[i];
                }
            }
            break;
    }
    populate_auth_registry(a);
    backbone::pop_focus_context();
    const std::span<const backbone::FocusableId> list = auth_focus_list(a);
    backbone::push_focus_context(list, list.empty() ? backbone::kNoFocus : list.front(),
                                 "modal.auth");
    a.last_synced_focus = backbone::kNoFocus;
}

void render_field_error(const AccountModalState& a, AuthField field) {
    if (a.error_field == field && !a.error_message.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::StateFail));
        ImGui::TextWrapped("%s", a.error_message.c_str());
        ImGui::PopStyleColor();
    }
}

// Password field + show/hide toggle on one row.
void render_password_row(AccountModalState& a, const bridge::FocusReconcile& rec, ImU32 ring) {
    ImGui::TextUnformatted("Password");
    const float toggle_w = ImGui::CalcTextSize("Hide").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    const float field_w = ImGui::GetContentRegionAvail().x - toggle_w - ImGui::GetStyle().ItemSpacing.x;
    const ImGuiInputTextFlags flags = a.show_password ? 0 : ImGuiInputTextFlags_Password;
    auth_text(kAuthPassword, "##auth_pw", a.password_buf.data(), a.password_buf.size(), rec, ring,
              field_w, flags);
    ImGui::SameLine();
    if (auth_button(kAuthShowPw, a.show_password ? "Hide" : "Show", ring, true)) {
        a.show_password = !a.show_password;
    }
}

void render_sign_in(AccountModalState& a, const bridge::FocusReconcile& rec, ImU32 ring) {
    ImGui::TextUnformatted("Email or username");
    auth_text(kAuthIdField, "##auth_id", a.id_or_email_buf.data(), a.id_or_email_buf.size(), rec, ring);

    render_password_row(a, rec, ring);
    render_field_error(a, AuthField::Password);

    if (auth_link(kAuthForgotLink, "##forgot", "Forgot password?", theme::ColorToken::TextSecondary,
                  ring)) {
        do_relayout(a, AccountModalState::Layout::Forgot);
    }

    render_field_error(a, AuthField::None);  // general (rate-limit / blocked / unknown)

    ImGui::Spacing();
    const bool enabled =
        sign_in_submittable(buf_view(a.id_or_email_buf.data()), buf_view(a.password_buf.data()));
    if (auth_button(kAuthSubmit, "Sign In", ring, enabled, ImVec2{-1.0f, 0.0f})) {
        do_submit(a);
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Don't have an account?");
    ImGui::SameLine();
    if (auth_link(kAuthSwapLink, "##swap_up", "Sign Up", theme::ColorToken::AccentPrimary, ring)) {
        do_relayout(a, AccountModalState::Layout::SignUp);
    }
}

void render_sign_up(AccountModalState& a, const bridge::FocusReconcile& rec, ImU32 ring) {
    ImGui::TextUnformatted("Username");
    auth_text(kAuthUsername, "##auth_user", a.username_buf.data(), a.username_buf.size(), rec, ring);
    render_field_error(a, AuthField::Username);

    ImGui::TextUnformatted("Email");
    auth_text(kAuthEmail, "##auth_email", a.email_buf.data(), a.email_buf.size(), rec, ring);
    render_field_error(a, AuthField::Email);

    render_password_row(a, rec, ring);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::TextSecondary));
    ImGui::TextUnformatted("Must be at least 8 characters");
    ImGui::PopStyleColor();
    render_field_error(a, AuthField::Password);

    ImGui::Spacing();
    auth_checkbox(kAuthAgeConsent, "I am 13 years of age or older", a.consent_age, ring);

    // ToS consent with inline document links (mouse-clickable; the focus list follows the
    // spec, which does not list the inline links as Tab stops — the docs are also reachable
    // from the Settings Legal section for keyboard users). Arrow keys on this grouped stop
    // move a positional highlight (Left/Up = Terms, Right/Down = Privacy); Enter/Space then
    // opens the highlighted doc. Underline indicates the highlighted side when focused.
    const bool tos_focused =
        backbone::is_keyboard_mode_active() &&
        backbone::get_focused_element() == kAuthTosConsent;
    const bool terms_hl = tos_focused && a.tos_highlight == TosHighlight::Terms;
    const bool privacy_hl = tos_focused && a.tos_highlight == TosHighlight::Privacy;

    auth_checkbox(kAuthTosConsent, "I agree to the", a.consent_tos, ring);
    ImGui::SameLine();
    if (auth_link_highlighted(kAuthTosConsent /*shares ring target*/, "##tos_link",
                              "Terms of Service", theme::ColorToken::AccentPrimary, ring,
                              terms_hl) &&
        a.seams.open_legal) {
        a.seams.open_legal(LegalDoc::Terms);
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("and");
    ImGui::SameLine();
    if (auth_link_highlighted(kAuthTosConsent, "##privacy_link", "Privacy Policy",
                              theme::ColorToken::AccentPrimary, ring, privacy_hl) &&
        a.seams.open_legal) {
        a.seams.open_legal(LegalDoc::Privacy);
    }

    render_field_error(a, AuthField::None);  // general (rate-limit / blocked / unknown)

    ImGui::Spacing();
    const bool enabled =
        sign_up_submittable(buf_view(a.username_buf.data()), buf_view(a.email_buf.data()),
                            buf_view(a.password_buf.data()), a.consent_age, a.consent_tos);
    if (auth_button(kAuthSubmit, "Sign Up", ring, enabled, ImVec2{-1.0f, 0.0f})) {
        do_submit(a);
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Already have an account?");
    ImGui::SameLine();
    if (auth_link(kAuthSwapLink, "##swap_in", "Sign In", theme::ColorToken::AccentPrimary, ring)) {
        do_relayout(a, AccountModalState::Layout::SignIn);
    }
}

void render_forgot(AccountModalState& a, const bridge::FocusReconcile& rec, ImU32 ring) {
    ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::TextSecondary));
    ImGui::TextWrapped("Enter your email and we'll send a reset link.");
    ImGui::PopStyleColor();

    if (a.forgot_sent) {
        ImGui::BeginDisabled();
    }
    auth_text(kAuthForgotEmail, "##forgot_email", a.forgot_email_buf.data(),
              a.forgot_email_buf.size(), rec, ring);
    const bool can_send = !a.forgot_sent && !buf_view(a.forgot_email_buf.data()).empty();
    if (auth_button(kAuthForgotSend, "Send reset link", ring, can_send, ImVec2{-1.0f, 0.0f})) {
        if (a.seams.reset_password) {
            a.seams.reset_password(buf_view(a.forgot_email_buf.data()));
        }
        a.forgot_sent = true;
    }
    if (a.forgot_sent) {
        ImGui::EndDisabled();
        ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::AccentPrimary));
        ImGui::TextWrapped("%s", std::string{kPasswordResetSentMessage}.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    if (auth_link(kAuthForgotBack, "##forgot_back", "Back to sign in",
                  theme::ColorToken::TextSecondary, ring)) {
        do_relayout(a, AccountModalState::Layout::SignIn);
    }
}

// The content provider's render_body. render_auth_modal (Zone 11) provides the compact
// centered window + the X close; this draws the person-glyph header and the active form.
void render_auth_body(AccountModalState& a) {
    const ImU32 ring = token_u32(theme::ColorToken::BorderFocus);
    const bridge::FocusReconcile rec =
        a.focus_registry != nullptr
            ? bridge::begin_focus_reconcile(*a.focus_registry, a.last_synced_focus)
            : bridge::FocusReconcile{};

    const float h = ImGui::GetTextLineHeight();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    draw_person_glyph(ImVec2{p.x + h * 0.5f, p.y + h * 0.5f}, h * 0.5f,
                      token_u32(theme::ColorToken::TextPrimary));
    ImGui::Dummy(ImVec2{h, h});
    ImGui::SameLine();
    const char* title =
        a.forgot_open ? "Reset password"
                      : (modal::auth_mode() == modal::AuthMode::SignUp ? "Sign Up" : "Sign In");
    ImGui::TextUnformatted(title);
    // The X close (Zone 11, top-right) is ~1.6 line-heights tall and overlaps this header
    // row; pad the header band so the separator below clears the X box instead of cutting
    // into it. Body content shifts down with the separator, preserving the form's spacing.
    ImGui::Dummy(ImVec2{0.0f, h * 0.8f});
    ImGui::Separator();

    if (a.forgot_open) {
        render_forgot(a, rec, ring);
    } else if (modal::auth_mode() == modal::AuthMode::SignUp) {
        render_sign_up(a, rec, ring);
    } else {
        render_sign_in(a, rec, ring);
    }

    a.last_synced_focus = bridge::active_focus_or_none();
}

bool dispatch_auth_key(AccountModalState& a, const backbone::KeyEvent& e) {
    if (e.type != backbone::KeyEventType::KeyDown || a.focus_registry == nullptr) {
        return false;
    }
    const bridge::FocusRegistry& reg = *a.focus_registry;
    const backbone::FocusableId focused = bridge::active_focus_or_none();

    // Intra-group keys on the ToS/Privacy consent grouped stop (Category A positional
    // toggle): the row stays a single tab stop. Arrow keys move the highlight absolutely
    // (Left/Up = Terms, Right/Down = Privacy); Enter/Space opens the highlighted doc (or
    // toggles the checkbox if no highlight has been set). Digit 1/2 jump immediately (and
    // set the highlight so Enter follows). Tab falls through to the backbone focus-nav handler.
    if (focused == kAuthTosConsent) {
        if (e.code == backbone::KeyCode::ArrowLeft || e.code == backbone::KeyCode::ArrowUp) {
            a.tos_highlight = TosHighlight::Terms;
            return true;
        }
        if (e.code == backbone::KeyCode::ArrowRight || e.code == backbone::KeyCode::ArrowDown) {
            a.tos_highlight = TosHighlight::Privacy;
            return true;
        }
        if (a.seams.open_legal) {
            if (e.code == backbone::KeyCode::Digit1) {
                a.tos_highlight = TosHighlight::Terms;
                a.seams.open_legal(LegalDoc::Terms);
                return true;
            }
            if (e.code == backbone::KeyCode::Digit2) {
                a.tos_highlight = TosHighlight::Privacy;
                a.seams.open_legal(LegalDoc::Privacy);
                return true;
            }
            // Enter/Space with a highlight: open the highlighted doc instead of toggling the
            // checkbox. Without a highlight, fall through so dispatch_focus_key toggles it.
            const bool activate_key = e.code == backbone::KeyCode::Space ||
                                      e.code == backbone::KeyCode::Enter;
            if (activate_key && a.tos_highlight != TosHighlight::None) {
                a.seams.open_legal(a.tos_highlight == TosHighlight::Terms ? LegalDoc::Terms
                                                                          : LegalDoc::Privacy);
                return true;
            }
        }
    }

    if (reg.is_text_field(focused) &&
        (e.code == backbone::KeyCode::ArrowLeft || e.code == backbone::KeyCode::ArrowRight)) {
        return false;  // text-cursor keys belong to ImGui's InputText
    }

    // Enter submits the SIGN IN form only — equivalent to clicking the Sign In button — when
    // it would be enabled (both fields filled) and a text field is focused (so Enter on a
    // link/button still activates that control). Routed through request_submit so do_submit
    // runs in the deferred block below, never mid-dispatch (it may close + clear the
    // registry). Sign Up / Forgot keep their behavior: Enter on a field does nothing here.
    const bool enter_submit_sign_in =
        e.code == backbone::KeyCode::Enter && !a.forgot_open &&
        modal::auth_mode() == modal::AuthMode::SignIn && reg.is_text_field(focused) &&
        sign_in_submittable(buf_view(a.id_or_email_buf.data()), buf_view(a.password_buf.data()));
    if (enter_submit_sign_in) {
        a.request_submit = true;
    }

    const bool handled = bridge::dispatch_focus_key(reg, focused, e.code);

    // Deferred actions, AFTER dispatch_focus_key returns (no registry-clearing close runs
    // inside the activate closure). At most one fires per key.
    if (a.request_close) {
        a.request_close = false;
        modal::close_modal();
    } else if (a.request_submit) {
        a.request_submit = false;
        do_submit(a);  // may close on success (safe here)
    } else if (a.request_relayout) {
        a.request_relayout = false;
        do_relayout(a, a.pending_layout);
    }
    return handled || enter_submit_sign_in;
}

void on_auth_open(AccountModalState& a) {
    a.id_or_email_buf = {};
    a.username_buf = {};
    a.email_buf = {};
    a.password_buf = {};
    a.forgot_email_buf = {};
    a.show_password = false;
    a.consent_age = false;
    a.consent_tos = false;
    a.tos_highlight = TosHighlight::None;
    a.error_field = AuthField::None;
    a.error_message.clear();
    a.forgot_open = false;
    a.forgot_sent = false;
    a.request_close = false;
    a.request_submit = false;
    a.request_relayout = false;
    a.last_synced_focus = backbone::kNoFocus;
    populate_auth_registry(a);  // reads modal::auth_mode() (set by open_sign_in/up_modal)
}

void on_auth_close(AccountModalState& a) {
    if (a.focus_registry != nullptr) {
        a.focus_registry->clear();
    }
}

modal::ModalContentProvider make_auth_provider(AccountModalState& a) {
    modal::ModalContentProvider p{};
    p.header_name = "";  // render_auth_modal draws no pill header; the body draws the glyph
    p.close_focus = modal::kAuthShellClose;
    p.render_body = [&a] { render_auth_body(a); };
    p.focus_list = [&a] { return auth_focus_list(a); };
    p.initial_focus = backbone::kNoFocus;  // => front() of the mode list (modal_base fallback)
    p.dispatch = [&a](const backbone::KeyEvent& e) { return dispatch_auth_key(a, e); };
    p.on_open = [&a] { on_auth_open(a); };
    p.on_close = [&a] { on_auth_close(a); };
    return p;
}

}  // namespace

// ----- exports -----

AccountSnapshot account_snapshot(const AccountModalState& a) {
    return a.seams.account ? a.seams.account() : AccountSnapshot{};
}

WalletSnapshot account_wallet(const AccountModalState& a) {
    return a.seams.wallet ? a.seams.wallet() : WalletSnapshot{};
}

void account_open_sign_in(AccountModalState& a) {
    const bool healthy = !a.seams.health_check || a.seams.health_check();
    if (healthy) {
        modal::open_sign_in_modal();
    } else {
        modal::trigger_outage_banner("Sign in temporarily unavailable. Please try again later.");
    }
}

void account_open_sign_up(AccountModalState& a) {
    const bool healthy = !a.seams.health_check || a.seams.health_check();
    if (healthy) {
        modal::open_sign_up_modal();
    } else {
        modal::trigger_outage_banner("Sign up temporarily unavailable. Please try again later.");
    }
}

void account_sign_out(AccountModalState& a) {
    if (a.seams.sign_out) {
        a.seams.sign_out();
    }
}

void account_confirm_delete(AccountModalState& a) {
    modal::open_confirm_modal(modal::ConfirmSpec{
        .body = "This will permanently delete your account, leaderboard standing, and all "
                "unlocked tracks. This cannot be undone.",
        .on_yes = [&a] {
            if (a.seams.delete_account) {
                a.seams.delete_account();
            }
        }});
}

void account_confirm_change_password(AccountModalState& a) {
    const std::string masked = mask_email(account_snapshot(a).email);
    modal::open_confirm_modal(modal::ConfirmSpec{
        .body = "We'll email you a password reset link to " + masked + ". Continue?",
        .on_yes = [&a] {
            if (a.seams.change_password) {
                a.seams.change_password();
            }
        }});
}

void install_account_content(AccountModalState& account, SettingsModalState& settings) {
    account.focus_registry = &account.own_registry;

    // The Sign Up consent inline-links reuse Part 1's legal-doc sub-modal (set the doc kind,
    // open kSettingsDocId stacked over the auth modal). Wired here so Zone 12 owns the reuse
    // without a boot edit.
    account.seams.open_legal = [&settings](LegalDoc doc) {
        settings.doc_kind = doc == LegalDoc::Terms ? SettingsModalState::DocKind::Terms
                                                   : SettingsModalState::DocKind::Privacy;
        modal::open_modal(modal::kSettingsDocId);
    };

    modal::register_modal_content(modal::kAuthModalId, make_auth_provider(account));
}

}  // namespace poker_trainer::settings
