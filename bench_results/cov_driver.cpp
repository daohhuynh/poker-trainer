// Combined coverage driver: exercises every branch of the focus-reconciliation
// seam's pure logic (the real functions in src/bridge/focus_registry.cpp and
// src/backbone/focus_manager.cpp) so llvm-cov can report end-to-end seam coverage.
// This is a coverage driver, NOT a timing harness -- it just hits each code path
// once. Correctness is asserted by focus_matrix.cpp and the shipping gtest suite.

#include "bridge/focus_registry.hpp"

#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"
#include "backbone/screen_state.hpp"

#include <cassert>

namespace bb = poker_trainer::backbone;
namespace br = poker_trainer::bridge;

int main() {
    const bb::FocusableId t0 = bb::make_focusable_id("cov.text0");
    const bb::FocusableId t1 = bb::make_focusable_id("cov.text1");
    const bb::FocusableId grp = bb::make_focusable_id("cov.group");
    const bb::FocusableId absent = bb::make_focusable_id("cov.absent");

    // --- registry: register, replace, find, is_text_field, clear ---
    br::FocusRegistry reg;
    reg.register_element(t0, br::FocusableEntry{.is_text_field = true});
    reg.register_element(t1, br::FocusableEntry{.is_text_field = true});
    int activated = 0, adjusted = 0;
    reg.register_element(grp, br::FocusableEntry{.is_text_field = false,
                                                 .activate = [&] { ++activated; },
                                                 .adjust = [&](int d) { adjusted += d; }});
    reg.register_element(grp, br::FocusableEntry{.is_text_field = false,
                                                 .activate = [&] { ++activated; },
                                                 .adjust = [&](int d) { adjusted += d; }});  // replace path
    (void)reg.find(t0);
    (void)reg.find(absent);
    (void)reg.is_text_field(t0);
    (void)reg.is_text_field(grp);
    (void)reg.is_text_field(absent);

    // --- decide_focus_reconcile: all four branches ---
    (void)br::decide_focus_reconcile(reg, bb::kNoFocus, grp);   // YieldKeyboard (non-text)
    (void)br::decide_focus_reconcile(reg, t0, t0);              // None (unchanged text)
    (void)br::decide_focus_reconcile(reg, bb::kNoFocus, t0);    // FocusTextBox (change to text)
    (void)br::decide_focus_reconcile(reg, grp, absent);         // None (unowned)

    // --- active_focus_or_none + begin_focus_reconcile (keyboard off, then on) ---
    bb::reset_focus_manager_for_testing();
    (void)br::active_focus_or_none();                           // keyboard mode inactive branch
    static const bb::FocusableId list[] = {t0, t1, grp};
    bb::register_focus_list(bb::ScreenId::Game, list);
    bb::activate_keyboard_mode();
    bb::snap_focus_to(t0);
    (void)br::active_focus_or_none();                           // active branch
    (void)br::begin_focus_reconcile(reg, bb::kNoFocus);

    // --- dispatch_focus_key: activate, adjust(+/-), unregistered, missing hook, non-key ---
    assert(br::dispatch_focus_key(reg, grp, bb::KeyCode::Space));
    assert(br::dispatch_focus_key(reg, grp, bb::KeyCode::Enter));
    assert(br::dispatch_focus_key(reg, grp, bb::KeyCode::ArrowUp));
    assert(br::dispatch_focus_key(reg, grp, bb::KeyCode::ArrowRight));
    assert(br::dispatch_focus_key(reg, grp, bb::KeyCode::ArrowDown));
    assert(br::dispatch_focus_key(reg, grp, bb::KeyCode::ArrowLeft));
    (void)br::dispatch_focus_key(reg, absent, bb::KeyCode::Enter);   // unregistered
    (void)br::dispatch_focus_key(reg, t0, bb::KeyCode::Enter);       // registered text, no hook
    (void)br::dispatch_focus_key(reg, t0, bb::KeyCode::ArrowUp);     // no adjust hook
    (void)br::dispatch_focus_key(reg, grp, bb::KeyCode::Tab);        // non-dispatch key

    // --- focus_manager: full state machine ---
    bb::advance_focus(false); bb::advance_focus(false); bb::advance_focus(false);  // fwd + wrap
    bb::advance_focus(true);                                                       // reverse
    bb::snap_focus_to(absent);                                                     // snap no-op
    bb::push_focus_context(list, t1, "cov");                                       // push
    (void)bb::context_depth();
    (void)bb::is_keyboard_mode_active();
    (void)bb::get_focused_element();
    bb::pop_focus_context();
    bb::pop_focus_context();                                                       // base no-op
    reg.clear();
    return 0;
}
