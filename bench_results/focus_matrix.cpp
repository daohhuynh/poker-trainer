// Step 6: focus-correctness matrix.
//
// Drives the REAL, unmodified focus machinery compiled from the shipping tree:
//   * backbone::focus_manager  (src/backbone/focus_manager.cpp) -- tab fwd/back,
//     wraparound, modal-trap via push/pop context, nested context restoration.
//   * bridge::dispatch_focus_key (src/bridge/focus_registry.cpp) -- activation-key
//     semantics (Space/Enter -> activate; arrows -> adjust).
// No reconstruction of the logic: the harness only calls the public API and asserts
// on observable state (get_focused_element / context_depth / hook side effects).
//
// Two rows are SOURCE-VERIFIED rather than executed: the Game-screen "Space-only,
// Enter reserved" cluster rule lives in on_cluster_key(), a translation-unit-static
// in src/modal/modal_base.cpp entangled with ImGui + a file-static runtime, so it is
// not independently invocable natively. Those rows read the real source file and
// assert the guard clauses are present; they are clearly tagged mode="source" in the
// emitted matrix so they are not conflated with the executed behavioral rows.

#include "bridge/focus_registry.hpp"

#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"
#include "backbone/screen_state.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace bb = poker_trainer::backbone;
namespace br = poker_trainer::bridge;

namespace {

struct Row { std::string name, category, mode; bool passed; };
std::vector<Row> g_rows;

void check(const char* name, const char* category, const char* mode, bool cond) {
    g_rows.push_back({name, category, mode, cond});
}

constexpr bb::FocusableId A = bb::make_focusable_id("m.a");
constexpr bb::FocusableId B = bb::make_focusable_id("m.b");
constexpr bb::FocusableId C = bb::make_focusable_id("m.c");
constexpr bb::FocusableId X = bb::make_focusable_id("m.x");
constexpr bb::FocusableId Y = bb::make_focusable_id("m.y");
constexpr bb::FocusableId P = bb::make_focusable_id("m.p");
constexpr bb::FocusableId Q = bb::make_focusable_id("m.q");
constexpr bb::FocusableId R = bb::make_focusable_id("m.r");

void register_base() {
    static const bb::FocusableId list[] = {A, B, C};
    bb::reset_focus_manager_for_testing();
    bb::register_focus_list(bb::ScreenId::Root, list);
}

bool file_contains_all(const std::string& path, const std::vector<std::string>& needles) {
    std::ifstream f(path);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    const std::string text = ss.str();
    for (const std::string& n : needles) {
        if (text.find(n) == std::string::npos) return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string repo = (argc > 1) ? argv[1] : ".";

    // ---- Tab forward ----
    register_base();
    bb::advance_focus(false);
    const bool tf1 = bb::get_focused_element() == A;  // first Tab lands on item 1
    bb::advance_focus(false);
    const bool tf2 = bb::get_focused_element() == B;
    bb::advance_focus(false);
    const bool tf3 = bb::get_focused_element() == C;
    check("tab_forward_lands_1_2_3", "traversal", "executed", tf1 && tf2 && tf3);

    // ---- Wraparound forward at end (C -> A) ----
    bb::advance_focus(false);
    check("wrap_forward_end_to_start", "wraparound", "executed",
          bb::get_focused_element() == A);

    // ---- Tab backward (fresh context, Shift-Tab lands on last) ----
    register_base();
    bb::advance_focus(true);
    const bool tb1 = bb::get_focused_element() == C;
    bb::advance_focus(true);
    const bool tb2 = bb::get_focused_element() == B;
    check("tab_backward_lands_last_then_prev", "traversal", "executed", tb1 && tb2);

    // ---- Wraparound backward at start (A -> C) ----
    register_base();
    bb::advance_focus(false);            // arm at A
    bb::advance_focus(true);             // Shift-Tab from first wraps to last
    check("wrap_backward_start_to_end", "wraparound", "executed",
          bb::get_focused_element() == C);

    // ---- Unarmed context shows nothing until first navigation ----
    register_base();
    const bool unarmed_nofocus = bb::get_focused_element() == bb::kNoFocus;
    check("fresh_context_unarmed_no_focus", "traversal", "executed", unarmed_nofocus);

    // ---- Modal-trap enter: focus cannot escape the modal context ----
    register_base();
    bb::advance_focus(false);            // base focus = A
    static const bb::FocusableId modal[] = {X, Y};
    bb::push_focus_context(modal, X, "modal");
    const bool trap_initial = bb::get_focused_element() == X;  // opens armed on initial
    bool trapped = true;
    for (int i = 0; i < 6; ++i) {
        bb::advance_focus(i % 2 == 0);   // tab around both directions
        const bb::FocusableId cur = bb::get_focused_element();
        if (cur != X && cur != Y) trapped = false;  // A/B/C would be an escape
    }
    check("modal_trap_focus_cannot_escape", "modal-trap", "executed",
          trap_initial && trapped);
    check("modal_trap_depth_is_1", "modal-trap", "executed", bb::context_depth() == 1);

    // ---- Modal-trap exit: prior context + focus restored ----
    bb::pop_focus_context();
    check("modal_trap_exit_restores_prior_focus", "modal-trap", "executed",
          bb::get_focused_element() == A && bb::context_depth() == 0);

    // ---- Nested push/pop restoration ----
    register_base();
    bb::advance_focus(false);            // base = A
    bb::push_focus_context(modal, X, "m1");
    bb::advance_focus(false);            // m1 focus -> Y
    const bool m1_at_y = bb::get_focused_element() == Y;
    static const bb::FocusableId modal2[] = {P, Q, R};
    bb::push_focus_context(modal2, Q, "m2");
    const bool m2_initial_q = bb::get_focused_element() == Q && bb::context_depth() == 2;
    bb::pop_focus_context();             // back to m1 (should restore Y)
    const bool back_to_m1_y = bb::get_focused_element() == Y && bb::context_depth() == 1;
    bb::pop_focus_context();             // back to base (should restore A)
    const bool back_to_base_a = bb::get_focused_element() == A && bb::context_depth() == 0;
    check("nested_push_pop_restores_each_level", "nested-context", "executed",
          m1_at_y && m2_initial_q && back_to_m1_y && back_to_base_a);

    // ---- Activation-key semantics (generic screens): Space OR Enter activates ----
    {
        int activated = 0;
        int adjusted = 0;
        br::FocusRegistry reg;
        reg.register_element(A, br::FocusableEntry{.is_text_field = false,
                                                   .activate = [&] { ++activated; },
                                                   .adjust = [&](int) { ++adjusted; }});
        const bool space_ok = br::dispatch_focus_key(reg, A, bb::KeyCode::Space);
        const bool enter_ok = br::dispatch_focus_key(reg, A, bb::KeyCode::Enter);
        check("activation_space_activates", "activation-key", "executed", space_ok && activated >= 1);
        check("activation_enter_activates", "activation-key", "executed", enter_ok && activated == 2);
        // Arrows adjust, not activate.
        const bool up_ok = br::dispatch_focus_key(reg, A, bb::KeyCode::ArrowUp);
        check("activation_arrow_adjusts_not_activates", "activation-key", "executed",
              up_ok && adjusted == 1 && activated == 2);
        // Non-dispatch key (Tab) is not consumed by activation dispatch.
        const bool tab_consumed = br::dispatch_focus_key(reg, A, bb::KeyCode::Tab);
        check("activation_tab_not_consumed", "activation-key", "executed", !tab_consumed);
    }

    // ---- Activation-key per screen: Game = Space-only, Enter reserved (SOURCE) ----
    // on_cluster_key(): the Game cluster rejects Enter so it falls through to Z09's
    // submit/advance handler.
    const bool game_space_only = file_contains_all(
        repo + "/src/modal/modal_base.cpp",
        {"ClusterScreen::Game && enter", "return false;", "Game cluster is Space-only"});
    check("game_cluster_space_only_enter_reserved", "activation-key", "source", game_space_only);
    // Generic screens accept Space OR Enter for button activation (representative sites).
    const bool root_space_or_enter = file_contains_all(
        repo + "/src/screens/root_screen.cpp",
        {"KeyCode::Space", "KeyCode::Enter"});
    const bool mode_space_or_enter = file_contains_all(
        repo + "/src/screens/mode_selection_screen.cpp",
        {"KeyCode::Space", "KeyCode::Enter"});
    check("generic_screens_space_or_enter", "activation-key", "source",
          root_space_or_enter && mode_space_or_enter);
    // Game math zone reserves Enter for submit / tier-advance.
    const bool enter_reserved_math = file_contains_all(
        repo + "/src/math/keybinds.cpp",
        {"KeyCode::Enter", "do_submit", "advance_tier"});
    check("game_enter_reserved_for_math_submit", "activation-key", "source", enter_reserved_math);

    // ---- report ----
    std::size_t passed = 0, executed = 0, source = 0, exec_pass = 0, src_pass = 0;
    for (const Row& r : g_rows) {
        if (r.passed) ++passed;
        if (r.mode == "executed") { ++executed; if (r.passed) ++exec_pass; }
        else { ++source; if (r.passed) ++src_pass; }
    }
    std::printf("=== Step 6: focus correctness matrix ===\n");
    std::printf("%-42s %-14s %-9s %s\n", "test", "category", "mode", "result");
    for (const Row& r : g_rows) {
        std::printf("%-42s %-14s %-9s %s\n", r.name.c_str(), r.category.c_str(), r.mode.c_str(),
                    r.passed ? "PASS" : "FAIL");
    }
    std::printf("\nTOTAL %zu/%zu passed  (executed %zu/%zu, source-verified %zu/%zu)\n",
                passed, g_rows.size(), exec_pass, executed, src_pass, source);

    // JSON
    std::string j = "{\n  \"total\": " + std::to_string(g_rows.size()) +
                    ",\n  \"passed\": " + std::to_string(passed) +
                    ",\n  \"executed\": " + std::to_string(executed) +
                    ",\n  \"executed_passed\": " + std::to_string(exec_pass) +
                    ",\n  \"source_verified\": " + std::to_string(source) +
                    ",\n  \"source_passed\": " + std::to_string(src_pass) + ",\n  \"rows\": [\n";
    for (std::size_t i = 0; i < g_rows.size(); ++i) {
        const Row& r = g_rows[i];
        j += "    {\"name\": \"" + r.name + "\", \"category\": \"" + r.category +
             "\", \"mode\": \"" + r.mode + "\", \"passed\": " + (r.passed ? "true" : "false") +
             "}" + (i + 1 == g_rows.size() ? "\n" : ",\n");
    }
    j += "  ]\n}\n";
    std::ofstream out(repo + "/bench_results/focus_matrix.json");
    out << j;

    std::printf("\nWrote bench_results/focus_matrix.json\n");
    return (passed == g_rows.size()) ? 0 : 1;
}
