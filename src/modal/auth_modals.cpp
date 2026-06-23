#include "modal/auth_modals.hpp"

#include "modal/modal_base.hpp"
#include "modal/modals.hpp"

#include <imgui.h>

// Zone 11 — auth modal shell. The view (Sign In / Sign Up) is a module-static set by the
// open functions and read/written by Zone 12 through auth_mode() / set_auth_mode(). The
// form itself (fields, focus list, dispatch) is the registered content provider's, so
// this TU never depends on Zone 12 or Zone 04 — it only frames the provider's body.

namespace poker_trainer::modal {

namespace {

AuthMode g_auth_mode = AuthMode::SignIn;

}  // namespace

void open_sign_in_modal() {
    g_auth_mode = AuthMode::SignIn;
    open_modal(kAuthModalId);
}

void open_sign_up_modal() {
    g_auth_mode = AuthMode::SignUp;
    open_modal(kAuthModalId);
}

AuthMode auth_mode() { return g_auth_mode; }

void set_auth_mode(AuthMode mode) { g_auth_mode = mode; }

void render_auth_modal() {
    const ModalContentProvider* p = modal_content_for(kAuthModalId);
    if (p == nullptr || !p->render_body) {
        // No Zone 12 provider (a build without Z12): nothing to frame. Escape still closes
        // via the ModalLayer key handler, so the modal cannot get stuck.
        return;
    }
    if (modal_begin_centered("##auth_modal", kAuthModalWidthFrac, kAuthModalHeightFrac)) {
        const bool x_clicked = modal_draw_x_close(p->close_focus);
        p->render_body();  // Zone 12 draws the person-glyph header + the active form
        if (x_clicked || modal_click_outside_dismissed()) {
            modal_end();
            close_modal();
            return;
        }
    }
    modal_end();
}

}  // namespace poker_trainer::modal
