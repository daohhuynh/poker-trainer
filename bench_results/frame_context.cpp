// Step 4 frame-budget context.
//
// Measures a full Dear ImGui immediate-mode frame build (NewFrame -> submit UI ->
// Render) using the vendored ImGui 1.91.9b core with a NULL backend (no GPU): the
// font atlas is rasterized in software and Render() produces ImDrawData, which is
// exactly the per-frame CPU "UI-build" work. We do NOT upload/draw anything.
//
// HONEST SCOPE / STUB DISCLOSURE: this is NOT the poker-trainer app's exact frame.
// The app's real render layer (screen render hooks, the in-house WebGL2 renderer,
// texture binds, asset draws) is compiled behind `#ifdef __EMSCRIPTEN__` and needs
// a live GL context + loaded CDN assets; src/main.cpp natively is an empty
// `return 0`, so there is no runnable native full-frame app to time. Instead we
// build a REPRESENTATIVE Game-screen-shaped surface with the real ImGui API (the
// same library the app renders through): a fullscreen window, HUD text, 3 InputText
// boxes, a button row, and ~200 draw-list primitives standing in for the
// table/chips/cards. It is a same-library, same-order-of-magnitude proxy for "one
// UI-build frame," used only to contextualize the reconcile cost. The reconcile
// number itself (Steps 1-3) is measured against the real shipping functions.
//
// A full frame is microseconds, well above the ~41.7 ns Apple-Silicon clock tick,
// so frames are timed individually (no batching needed).

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using Clock = std::chrono::steady_clock;

namespace {

template <class T>
inline void do_not_optimize(const T& v) { asm volatile("" : : "r,m"(v) : "memory"); }

struct Stats { double mean, stddev, min, max, p50, p90, p99, p999; std::size_t n; };

Stats summarize(std::vector<double> xs) {
    Stats s{}; s.n = xs.size();
    double sum = 0; for (double x : xs) sum += x; s.mean = sum / static_cast<double>(xs.size());
    double var = 0; for (double x : xs) var += (x - s.mean) * (x - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(xs.size()));
    std::sort(xs.begin(), xs.end());
    auto pct = [&](double p) {
        const double idx = p * static_cast<double>(xs.size() - 1);
        const std::size_t lo = static_cast<std::size_t>(idx);
        const std::size_t hi = std::min(lo + 1, xs.size() - 1);
        const double f = idx - static_cast<double>(lo);
        return xs[lo] * (1 - f) + xs[hi] * f;
    };
    s.min = xs.front(); s.max = xs.back();
    s.p50 = pct(0.50); s.p90 = pct(0.90); s.p99 = pct(0.99); s.p999 = pct(0.999);
    return s;
}

// Build a representative Game-screen-shaped surface with the real ImGui API.
void build_game_like_surface() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1280, 720));
    ImGui::Begin("game", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

    // HUD text lines
    ImGui::Text("Pot: 1450");
    ImGui::Text("Blinds: 25 / 50");
    ImGui::Text("To call: 300");
    ImGui::SameLine();
    ImGui::Text("Position: BTN");

    // Math input boxes (the surface the reconcile actually serves)
    static char fold_pct[16] = "42";
    static char ev[16] = "30";
    static char bet[16] = "0.5";
    ImGui::InputText("Fold %", fold_pct, sizeof(fold_pct));
    ImGui::InputText("EV", ev, sizeof(ev));
    ImGui::InputText("Bet", bet, sizeof(bet));

    // Bet-size / cluster button row
    const char* labels[] = {"1/3", "1/2", "Full", "Over", "Settings", "Help"};
    for (int i = 0; i < 6; ++i) {
        if (i) ImGui::SameLine();
        ImGui::Button(labels[i], ImVec2(80, 28));
    }

    // ~200 draw-list primitives standing in for table/chips/cards/dealer.
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int i = 0; i < 120; ++i) {
        const float x = 100.0f + static_cast<float>(i % 20) * 40.0f;
        const float y = 300.0f + static_cast<float>(i / 20) * 30.0f;
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + 30, y + 20),
                          IM_COL32(180, 40, 40, 255), 3.0f);
    }
    for (int i = 0; i < 40; ++i) {
        const float cx = 150.0f + static_cast<float>(i) * 25.0f;
        dl->AddCircleFilled(ImVec2(cx, 560.0f), 10.0f, IM_COL32(230, 230, 230, 255), 16);
    }
    for (int i = 0; i < 40; ++i) {
        dl->AddText(ImVec2(120.0f + static_cast<float>(i) * 20.0f, 600.0f),
                    IM_COL32(255, 255, 255, 255), "A\xE2\x99\xA0");  // A of spades-ish
    }

    ImGui::End();
}

void build_minimal_surface() {
    ImGui::Begin("min");
    ImGui::Text("hello");
    ImGui::Button("ok");
    ImGui::End();
}

Stats time_frame(void (*build)(), std::size_t frames, std::size_t warmup) {
    std::vector<double> samples;
    samples.reserve(frames);
    for (std::size_t i = 0; i < frames + warmup; ++i) {
        const auto t0 = Clock::now();
        ImGui::NewFrame();
        build();
        ImGui::Render();
        const auto t1 = Clock::now();
        ImDrawData* dd = ImGui::GetDrawData();
        do_not_optimize(dd->TotalVtxCount);
        if (i >= warmup) {
            samples.push_back(
                std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(t1 - t0)
                    .count());
        }
    }
    return summarize(std::move(samples));
}

}  // namespace

int main() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1280, 720);
    io.DeltaTime = 1.0f / 60.0f;
    // Rasterize the default font atlas in software (no GPU) so NewFrame's
    // IsBuilt() assert is satisfied; assign a dummy texture id.
    unsigned char* pixels = nullptr;
    int tw = 0, th = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &tw, &th);
    io.Fonts->SetTexID(static_cast<ImTextureID>(1));

    const std::size_t kFrames = 20000, kWarmup = 2000;
    const Stats game = time_frame(&build_game_like_surface, kFrames, kWarmup);
    const Stats mini = time_frame(&build_minimal_surface, kFrames, kWarmup);

    ImGui::DestroyContext();

    // The reconcile numbers come from reconcile_bench.json (steady-frame mean).
    const double reconcile_ns = 2.5116;  // Step 1 steady-frame mean (this machine)
    const double frame_budget_ns = 16.6667e6;  // 60 fps

    std::printf("ImGui %s (%d)  font atlas %dx%d\n", IMGUI_VERSION, IMGUI_VERSION_NUM, tw, th);
    std::printf("game-like frame: mean=%.1f ns (%.3f us) median=%.1f p99=%.1f min=%.1f max=%.1f\n",
                game.mean, game.mean / 1000.0, game.p50, game.p99, game.min, game.max);
    std::printf("minimal frame:   mean=%.1f ns (%.3f us) median=%.1f p99=%.1f\n",
                mini.mean, mini.mean / 1000.0, mini.p50, mini.p99);
    std::printf("\nreconcile steady-frame mean = %.4f ns\n", reconcile_ns);
    std::printf("  reconcile / game-like frame  = %.6f %% \n", 100.0 * reconcile_ns / game.mean);
    std::printf("  reconcile / minimal frame    = %.6f %% \n", 100.0 * reconcile_ns / mini.mean);
    std::printf("  reconcile / 16.67ms budget   = %.9f %% \n", 100.0 * reconcile_ns / frame_budget_ns);
    std::printf("  game-like frame / budget     = %.4f %% \n", 100.0 * game.mean / frame_budget_ns);

    std::string j;
    char buf[1024];
    std::snprintf(buf, sizeof(buf),
                  "{\n  \"imgui_version\": \"%s\",\n  \"imgui_version_num\": %d,\n"
                  "  \"font_atlas\": \"%dx%d\",\n"
                  "  \"game_like_frame_ns\": {\"mean\": %.3f, \"median\": %.3f, \"p99\": %.3f, "
                  "\"min\": %.3f, \"max\": %.3f, \"stddev\": %.3f, \"n\": %zu},\n"
                  "  \"minimal_frame_ns\": {\"mean\": %.3f, \"median\": %.3f, \"p99\": %.3f, \"n\": %zu},\n"
                  "  \"reconcile_steady_ns\": %.4f,\n"
                  "  \"reconcile_pct_of_game_frame\": %.6f,\n"
                  "  \"reconcile_pct_of_minimal_frame\": %.6f,\n"
                  "  \"reconcile_pct_of_16_67ms\": %.9f,\n"
                  "  \"game_frame_pct_of_16_67ms\": %.4f\n}\n",
                  IMGUI_VERSION, IMGUI_VERSION_NUM, tw, th, game.mean, game.p50, game.p99,
                  game.min, game.max, game.stddev, game.n, mini.mean, mini.p50, mini.p99, mini.n,
                  reconcile_ns, 100.0 * reconcile_ns / game.mean, 100.0 * reconcile_ns / mini.mean,
                  100.0 * reconcile_ns / frame_budget_ns, 100.0 * game.mean / frame_budget_ns);
    j = buf;
    std::FILE* f = std::fopen("bench_results/frame_context.json", "w");
    std::fputs(j.c_str(), f);
    std::fclose(f);
    std::printf("\nWrote bench_results/frame_context.json\n");
    return 0;
}
