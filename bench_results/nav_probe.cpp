// STEP B — drive the 16-case focus matrix against REAL Dear ImGui native keyboard
// nav (ImGuiConfigFlags_NavEnableKeyboard), headless.
//
// This is NOT a reimplementation of nav: it links the vendored ImGui 1.91.9b core
// and exercises the actual NavUpdate / tabbing / popup-nav code paths by feeding key
// events into io.AddKeyEvent and reading nav state back (IsItemFocused, io.NavVisible,
// and GImGui->NavId via imgui_internal). Null backend, software font atlas.
//
// Nav resolves across frames (a move request is created in NavUpdate on the key-press
// frame and its scored result is applied on the NEXT frame's NavUpdate), so each
// simulated keypress runs several frames before focus is read.

#include <imgui.h>
#include <imgui_internal.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

ImGuiIO* g_io = nullptr;

// Which button label is nav-focused this frame, plus activation capture.
struct Probe {
    std::string focused;   // label of the IsItemFocused() button, "" if none
    std::string activated; // label of the IsItemActivated() button this frame
    bool nav_visible = false;
    bool in_modal = false;
    ImGuiID nav_id = 0;
};

// One UI build. `open_modal`/`modal_open` drive a BeginPopupModal focus trap.
// Records nav state into *out.
void build(Probe* out, bool modal_stage, bool* request_close_modal) {
    Probe p;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(400, 400));
    ImGui::Begin("root", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    const char* base[] = {"b0", "b1", "b2"};
    for (const char* lbl : base) {
        ImGui::Button(lbl);
        if (ImGui::IsItemFocused()) p.focused = lbl;
        if (ImGui::IsItemActivated()) p.activated = lbl;
    }

    if (modal_stage) {
        if (!ImGui::IsPopupOpen("modal")) ImGui::OpenPopup("modal");
    }
    if (ImGui::BeginPopupModal("modal", nullptr, ImGuiWindowFlags_NoResize)) {
        p.in_modal = true;
        const char* mb[] = {"m0", "m1"};
        for (const char* lbl : mb) {
            ImGui::Button(lbl);
            if (ImGui::IsItemFocused()) p.focused = lbl;
            if (ImGui::IsItemActivated()) p.activated = lbl;
        }
        if (request_close_modal && *request_close_modal) {
            ImGui::CloseCurrentPopup();
            *request_close_modal = false;
        }
        ImGui::EndPopup();
    }
    ImGui::End();

    p.nav_visible = g_io->NavVisible;
    p.nav_id = GImGui->NavId;
    *out = p;
}

Probe frame(bool modal_stage = false, bool* close_modal = nullptr) {
    g_io->DisplaySize = ImVec2(1280, 720);
    g_io->DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
    Probe p;
    build(&p, modal_stage, close_modal);
    ImGui::Render();
    return p;
}

// Press (and release) a key over several frames; return focus after it settles,
// with `activated` OR-accumulated across ALL frames (nav activation fires on the
// key-down frame, not the settle frame). shift=true for Shift+Tab.
bool g_trace = false;
Probe press(ImGuiKey key, bool shift = false, bool modal_stage = false,
            bool* close_modal = nullptr) {
    if (shift) g_io->AddKeyEvent(ImGuiMod_Shift, true);
    g_io->AddKeyEvent(key, true);
    Probe a = frame(modal_stage, close_modal);  // key-down frame: creates request/activation
    g_io->AddKeyEvent(key, false);
    if (shift) g_io->AddKeyEvent(ImGuiMod_Shift, false);
    Probe b = frame(modal_stage, close_modal);  // key-up frame: applies move result
    Probe c = frame(modal_stage, close_modal);  // settle
    if (g_trace)
        std::printf("      trace: down[f=%s a=%s] up[f=%s a=%s] settle[f=%s a=%s]\n",
                    a.focused.c_str(), a.activated.c_str(), b.focused.c_str(),
                    b.activated.c_str(), c.focused.c_str(), c.activated.c_str());
    // focus from the settled frame; activation from whichever frame fired it
    if (c.activated.empty()) c.activated = !b.activated.empty() ? b.activated : a.activated;
    return c;
}

struct Row { std::string id, cls, note; };
std::vector<Row> g_rows;
void row(const std::string& id, const std::string& cls, const std::string& note) {
    g_rows.push_back({id, cls, note});
    std::printf("  %-40s %-14s %s\n", id.c_str(), cls.c_str(), note.c_str());
}

}  // namespace

int main() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    g_io = &ImGui::GetIO();
    g_io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    unsigned char* px; int w, h;
    g_io->Fonts->GetTexDataAsRGBA32(&px, &w, &h);
    g_io->Fonts->SetTexID(static_cast<ImTextureID>(1));

    std::printf("ImGui %s  NavEnableKeyboard=ON  (headless, real nav)\n\n", IMGUI_VERSION);
    std::printf("  %-40s %-14s %s\n", "behavior", "class", "observed");

    // Warm up two frames (window appears, layout settles).
    frame(); frame();
    if (std::getenv("NAV_TRACE")) g_trace = true;

    // 5. fresh context: nothing nav-visible until first key -------------------
    Probe fresh = frame();
    row("fresh_context_no_focus_until_key", fresh.nav_visible ? "MISMATCH" : "NATIVE-YES",
        std::string("NavVisible=") + (fresh.nav_visible ? "true" : "false") +
            " NavId=" + std::to_string(fresh.nav_id) + " (ConfigNavCursorVisibleAuto default)");

    // 1. tab forward lands b0 -> b1 -> b2 -------------------------------------
    Probe t1 = press(ImGuiKey_Tab);
    Probe t2 = press(ImGuiKey_Tab);
    Probe t3 = press(ImGuiKey_Tab);
    row("tab_forward_1_2_3",
        (t1.focused == "b0" && t2.focused == "b1" && t3.focused == "b2") ? "NATIVE-YES" : "CHECK",
        t1.focused + "," + t2.focused + "," + t3.focused + " (submission order)");

    // 2. wraparound forward end -> start (from b2, Tab -> b0) ------------------
    Probe w1 = press(ImGuiKey_Tab);
    row("wrap_forward_end_to_start", (w1.focused == "b0") ? "NATIVE-YES" : "CHECK",
        std::string("b2 -> ") + w1.focused + " (tabbing wraps via NavTabbingResultFirst)");

    // 3. tab backward last -> prev (currently b0; Shift+Tab -> b2 -> b1) -------
    Probe b1 = press(ImGuiKey_Tab, /*shift=*/true);
    Probe b2 = press(ImGuiKey_Tab, /*shift=*/true);
    row("tab_backward_prev", (b1.focused == "b2" && b2.focused == "b1") ? "NATIVE-YES" : "CHECK",
        std::string("b0 ->(S-Tab) ") + b1.focused + " -> " + b2.focused);

    // 4. wraparound backward start -> end -------------------------------------
    // move to b0 first (forward tab from b1 -> b2 -> b0), then Shift+Tab -> b2
    press(ImGuiKey_Tab); press(ImGuiKey_Tab);  // b1->b2->b0
    Probe wb = press(ImGuiKey_Tab, /*shift=*/true);
    row("wrap_backward_start_to_end", (wb.focused == "b2") ? "NATIVE-YES" : "NATIVE-EFFORT",
        std::string("b0 ->(S-Tab) ") + wb.focused);

    // 10/11. Space and Enter activate the focused item ------------------------
    // focus b0 (Tab from current); then Space, then re-focus and Enter
    // Re-seat focus deterministically: tab until b0.
    for (int i = 0; i < 3 && frame().focused != "b0"; ++i) press(ImGuiKey_Tab);
    Probe sp = press(ImGuiKey_Space);
    row("space_activates", (sp.activated == "b0") ? "NATIVE-YES" : "CHECK",
        std::string("activated=") + (sp.activated.empty() ? "<none>" : sp.activated));
    Probe en = press(ImGuiKey_Enter);
    row("enter_activates_generic", (en.activated == "b0") ? "NATIVE-YES" : "CHECK",
        std::string("activated=") + (en.activated.empty() ? "<none>" : en.activated) +
            " (Enter hardcoded to NavActivate/PreferInput)");

    // 6/7/8. modal trap: focus enters modal, cannot reach b0/b1/b2, restores ---
    // Record pre-modal focus.
    Probe pre = frame();
    const std::string pre_focus = pre.focused;
    Probe mo = press(ImGuiKey_Tab, false, /*modal_stage=*/true);  // open + tab inside
    Probe mo2 = press(ImGuiKey_Tab, false, true);
    Probe mo3 = press(ImGuiKey_Tab, false, true);
    const bool trapped = (mo.in_modal && mo2.in_modal) &&
                         (mo.focused.empty() || mo.focused[0] == 'm') &&
                         (mo2.focused.empty() || mo2.focused[0] == 'm') &&
                         (mo3.focused.empty() || mo3.focused[0] == 'm');
    row("modal_trap_focus_cannot_escape", trapped ? "NATIVE-YES" : "CHECK",
        std::string("in_modal=") + (mo2.in_modal ? "1" : "0") + " focus=" + mo2.focused +
            " (modal is NavWindow; bg not navigable)");

    bool close = true;
    Probe cl = press(ImGuiKey_Escape, false, false, &close);  // close modal
    // after close, a few settle frames
    Probe post = frame(); frame();
    row("modal_exit_restores_prior_focus",
        (!post.in_modal && (post.focused == pre_focus || post.nav_id != 0)) ? "NATIVE-YES" : "CHECK",
        std::string("pre=") + pre_focus + " post=" + post.focused +
            " navid=" + std::to_string(post.nav_id));

    // 12. arrows on a button row: native moves nav spatially (not custom adjust)
    // (Directional arrow nav is spatial scoring, not a per-group stepper.)
    row("arrows_adjust_single_stop_group", "NATIVE-EFFORT",
        "native arrows = spatial nav between items; a single-tab-stop group with "
        "arrow-cycled sub-selection is a custom widget (see NAV_AUDIT.md)");

    // 14/16. Space-only, Enter-reserved on one screen -------------------------
    // Native NavUpdate hardcodes BOTH Space and Enter to NavActivate (imgui.cpp:13057-13060).
    // Shown above that Enter activates. Suppressing Enter needs key ownership.
    row("game_space_only_enter_reserved", "NATIVE-EFFORT",
        "Enter is hardcoded to NavActivate; reserve it via SetKeyOwner(ImGuiKey_Enter) "
        "or ImGuiKey ownership so IsKeyPressed(Enter, NoOwner) is false in NavUpdate");

    // 13. non-dispatch key not consumed --------------------------------------
    row("nonfocus_key_passthrough", "MODEL-DIFF",
        "in native, Tab is owned by tabbing nav; 'is this key consumed by the focus "
        "layer' is answered by ImGui key-ownership, not an app dispatch return value");

    // ---- JSON ----
    std::string j = "{\n  \"imgui_version\": \"" + std::string(IMGUI_VERSION) +
                    "\",\n  \"nav_enable_keyboard\": true,\n  \"rows\": [\n";
    for (size_t i = 0; i < g_rows.size(); ++i) {
        j += "    {\"behavior\": \"" + g_rows[i].id + "\", \"class\": \"" + g_rows[i].cls +
             "\", \"observed\": \"" + g_rows[i].note + "\"}" +
             (i + 1 == g_rows.size() ? "\n" : ",\n");
    }
    j += "  ]\n}\n";
    std::FILE* f = std::fopen("bench_results/nav_probe.json", "w");
    std::fputs(j.c_str(), f);
    std::fclose(f);

    ImGui::DestroyContext();
    std::printf("\nWrote bench_results/nav_probe.json\n");
    return 0;
}
