#pragma once

#include <cstdint>

// Zone 11 — Auth (Sign In / Sign Up) modal shell.
//
// A SINGLE modal (kAuthModalId) whose content swaps in place between Sign In and Sign Up
// (ARCHITECTURE "Modal Popup Contents by Source"). Zone 11 owns the compact centered
// window + the X close + dismissal (render_auth_modal); Zone 12 supplies the form body,
// focus list, and key dispatch through the generic content-provider seam (Zone 11 must
// not depend on Zone 04 auth flows / Zone 12). The Account section (Z12) and the Tutorial
// Complete screen (Z14) open these AFTER an Auth0 health check — a failed check triggers
// the outage banner instead of opening.
//
// The current "view" (Sign In vs Sign Up) lives here as the single source of truth: the
// open functions set it; Zone 12 reads auth_mode() to render the right form and writes it
// via set_auth_mode() on the in-place swap link.

namespace poker_trainer::modal {

// Which view the open auth modal is showing.
enum class AuthMode : std::uint8_t { SignIn = 0, SignUp = 1 };

// Canonical compact size for the auth modal (viewport fractions). Distinct from the
// cluster-modal size; the auth form is a centered compact panel. Tunable in the visual
// pass (the spec fixes "same dimensions" for both views but not the exact size).
inline constexpr float kAuthModalWidthFrac = 0.34f;
inline constexpr float kAuthModalHeightFrac = 0.62f;

// Open the auth modal in the Sign In / Sign Up view. (Callers gate these behind the Auth0
// health check; see Zone 12's account_open_sign_in / account_open_sign_up.)
void open_sign_in_modal();
void open_sign_up_modal();

// The view the open auth modal is showing / set it (Zone 12's content-swap link).
[[nodiscard]] AuthMode auth_mode();
void set_auth_mode(AuthMode mode);

// Render entry, dispatched by render_modal_overlay when kAuthModalId is topmost: the
// compact centered window + X close + click-outside, with the registered content
// provider's body drawn inside. A no-op when no provider is registered (a build without
// Zone 12); Escape still closes via the ModalLayer handler.
void render_auth_modal();

}  // namespace poker_trainer::modal
