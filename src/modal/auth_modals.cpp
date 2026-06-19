#include "modal/auth_modals.hpp"

// SEAM: see auth_modals.hpp. These are inert until Z04 (Auth0 client + health
// check), Z12 (Account section), and Z14 (Tutorial Complete) wire their triggers.
// Kept as no-ops rather than opening an empty shell so nothing presents a
// non-functional auth form.

namespace poker_trainer::modal {

void open_sign_in_modal() {
    // SEAM(Z04/Z12): build the Sign In shell + Auth0 flow. On health-check failure
    // the caller invokes trigger_outage_banner (already available).
}

void open_sign_up_modal() {
    // SEAM(Z04/Z12/Z14): build the Sign Up shell + Auth0 flow.
}

}  // namespace poker_trainer::modal
