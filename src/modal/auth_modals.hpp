#pragma once

// Zone 11 — Auth (Sign In / Sign Up / Forgot Password) modals.
//
// SEAM: deferred this wave. The triggers are all unbuilt — the Account section of
// the Settings page (Z12), the Tutorial Complete screen's Sign Up button (Z14), and
// the Auth0 health check that must pass before opening these (Z04). With no consumer
// and no Auth0 client, there is nothing to drive or test yet. The documented entry
// points below are inert stubs; when Z04/Z12/Z14 land, these will build the modal
// shells (reusing modal_base) and the health-check-fail path will call
// trigger_outage_banner (which IS built and callable now).

namespace poker_trainer::modal {

// Open the Sign In / Sign Up modal. Inert seam (no Auth0 client, no trigger yet).
void open_sign_in_modal();
void open_sign_up_modal();

}  // namespace poker_trainer::modal
