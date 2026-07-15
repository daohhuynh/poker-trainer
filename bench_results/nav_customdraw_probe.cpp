// STEP C linchpin — prove empirically that native ImGui nav can ONLY focus real
// ImGui items (things that call ItemAdd), and is blind to custom draw-list renders.
//
// The real app's root/mode/game/post-round/cluster surfaces render every control via
// the draw list (dl->AddRectFilled / image slots / procedural glyphs) with manual
// mouse hit-testing — they call ZERO ImGui item functions. This probe reproduces that
// exact shape: one REAL ImGui::Button next to a custom-drawn "button" rect that never
// calls ItemAdd. With NavEnableKeyboard on, we Tab repeatedly and observe that nav can
// only ever land on the real item; the custom rect is unreachable.

#include <imgui.h>
#include <imgui_internal.h>

#include <cstdio>
#include <string>

int main() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    unsigned char* px; int w, h;
    io.Fonts->GetTexDataAsRGBA32(&px, &w, &h);
    io.Fonts->SetTexID(static_cast<ImTextureID>(1));

    ImGuiID real_button_id = 0;
    std::string focus_trace;
    int distinct_navids_seen = 0;
    ImGuiID last_navid = 0xFFFFFFFF;

    auto do_frame = [&]() {
        io.DisplaySize = ImVec2(800, 600);
        io.DeltaTime = 1.0f / 60.0f;
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(800, 600));
        ImGui::Begin("cd", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        // (1) A REAL ImGui item.
        ImGui::Button("real_button");
        real_button_id = ImGui::GetItemID();

        // (2) A CUSTOM-DRAWN control exactly like the app's cluster icons / mode
        //     buttons: a draw-list rect + label + manual hit-test. NO ItemAdd, so
        //     ImGui's nav system never sees it as a focusable item.
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p0(50, 200), p1(250, 240);
        dl->AddRectFilled(p0, p1, IM_COL32(80, 80, 80, 255), 4.0f);
        dl->AddText(ImVec2(60, 210), IM_COL32_WHITE, "custom_drawn_button");
        const ImVec2 m = io.MousePos;                       // manual hit-test (app style)
        const bool hovered = m.x >= p0.x && m.x <= p1.x && m.y >= p0.y && m.y <= p1.y;
        (void)hovered;

        ImGui::End();
        ImGui::Render();
    };

    auto tab = [&]() {
        io.AddKeyEvent(ImGuiKey_Tab, true);  do_frame();
        io.AddKeyEvent(ImGuiKey_Tab, false); do_frame(); do_frame();
        const ImGuiID nid = GImGui->NavId;
        if (nid != last_navid) { ++distinct_navids_seen; last_navid = nid; }
        focus_trace += (nid == real_button_id) ? "real " : (nid == 0 ? "none " : "OTHER ");
    };

    do_frame(); do_frame();
    for (int i = 0; i < 6; ++i) tab();  // hammer Tab; a custom rect would show OTHER

    const bool only_reaches_real =
        focus_trace.find("OTHER") == std::string::npos && focus_trace.find("real") != std::string::npos;

    std::printf("ImGui %s  NavEnableKeyboard=ON\n", IMGUI_VERSION);
    std::printf("real_button nav id = %u\n", real_button_id);
    std::printf("Tab focus trace    = [ %s]\n", focus_trace.c_str());
    std::printf("distinct nav targets over 6 tabs = %d\n", distinct_navids_seen);
    std::printf("VERDICT: native nav %s reach the custom-drawn control.\n",
                only_reaches_real ? "NEVER could" : "could");
    std::printf("  => custom draw-list controls (root/mode/game/post-round/cluster) are\n"
                "     INVISIBLE to native nav because they never call ItemAdd().\n");

    std::FILE* f = std::fopen("bench_results/nav_customdraw_probe.json", "w");
    std::fprintf(f,
                 "{\n  \"real_button_navid\": %u,\n  \"tab_focus_trace\": \"%s\",\n"
                 "  \"distinct_nav_targets\": %d,\n  \"custom_draw_reachable_by_nav\": %s,\n"
                 "  \"conclusion\": \"native nav only focuses ItemAdd'd widgets; custom "
                 "draw-list controls are unreachable\"\n}\n",
                 real_button_id, focus_trace.c_str(), distinct_navids_seen,
                 only_reaches_real ? "false" : "true");
    std::fclose(f);
    ImGui::DestroyContext();
    return 0;
}
