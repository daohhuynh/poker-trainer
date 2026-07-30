#include "screens/tutorial_complete_screen.hpp"

#include "tutorial/overlay.hpp"
#include "tutorial/tutorial.hpp"

#include "animations/button_morph.hpp"

#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"
#include "backbone/modal_state.hpp"
#include "backbone/screen_state.hpp"

#include "assets/asset_paths.hpp"

#include "theme/theme_tokens.hpp"

#include <array>
#include <cstddef>
#include <string_view>

#include <imgui.h>

#include "bridge/asset_image.hpp"
#include "bridge/screen_dispatch.hpp"
#include "modal/modals.hpp"

namespace poker_trainer::screens {

namespace {

namespace be = backbone;
using animations::Canvas;
using animations::Rect;

constexpr std::string_view kCongrats = "You've seen the basic loop. You're ready to play.";
constexpr std::string_view kSignUpUnavailable =
    "Sign up temporarily unavailable. Please try again later.";

constexpr be::FocusableId kFocusTcSignUp = be::make_focusable_id("tc.signup");
constexpr be::FocusableId kFocusTcContinue = be::make_focusable_id("tc.continue");
constexpr be::FocusableId kFocusTcHome = be::make_focusable_id("tc.home");

enum class TcAction : std::uint8_t { SignUp, Continue, Home };

struct TcSlot {
    be::FocusableId focus{};
    const char* label{""};
    Rect rect{};
    TcAction action{TcAction::Continue};
};

// Focus-list registration is once-per-entry (register_focus_list relocks); we also
// re-register when the account state flips (a successful in-place Sign Up removes the
// Sign Up button), so the Tab order matches what's drawn.
bool g_focus_registered = false;
bool g_last_guest = true;

[[nodiscard]] Canvas viewport_canvas() {
    const ImVec2 size = ImGui::GetMainViewport()->Size;
    return Canvas{size.x, size.y};
}

[[nodiscard]] bool pt(float x, float y, const Rect& r) noexcept {
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

[[nodiscard]] bool is_guest() {
    const tutorial::TutorialRuntime* rt = tutorial::tutorial_runtime();
    if (rt == nullptr || !rt->seams.is_authenticated) {
        return true;  // no account state wired → treat as guest (shows Sign Up)
    }
    return !rt->seams.is_authenticated();
}

// Build the account-state-dependent button row (equal sizing, centered). Guest gets
// three buttons (Sign Up / Continue Playing / Home); a signed-in user gets two
// (Continue Playing / Home).
[[nodiscard]] std::size_t build_buttons(Canvas c, bool guest, std::array<TcSlot, 3>& out) {
    std::size_t n = 0;
    if (guest) {
        out[n++] = TcSlot{kFocusTcSignUp, "Sign Up", {}, TcAction::SignUp};
    }
    out[n++] = TcSlot{kFocusTcContinue, "Continue Playing", {}, TcAction::Continue};
    out[n++] = TcSlot{kFocusTcHome, "Home", {}, TcAction::Home};

    const float bw = c.width * 0.18f;
    const float bh = c.height * 0.08f;
    const float gap = c.width * 0.025f;
    const float total = static_cast<float>(n) * bw + static_cast<float>(n - 1) * gap;
    const float x0 = (c.width - total) * 0.5f;
    const float y = c.height * 0.62f;
    for (std::size_t i = 0; i < n; ++i) {
        out[i].rect = Rect{x0 + static_cast<float>(i) * (bw + gap), y, bw, bh};
    }
    return n;
}

void run_action(TcAction action) {
    switch (action) {
        case TcAction::SignUp: {
            // Sign Up does NOT restore settings or set the flag — the user is still in
            // the completion flow. Auth0 health check first; banner on failure.
            const tutorial::TutorialRuntime* rt = tutorial::tutorial_runtime();
            const bool healthy =
                rt != nullptr && rt->seams.auth_health_check && rt->seams.auth_health_check();
            if (healthy) {
                if (rt->seams.open_sign_up_modal) {
                    rt->seams.open_sign_up_modal();
                }
            } else {
                modal::trigger_outage_banner(kSignUpUnavailable);
            }
            break;
        }
        case TcAction::Continue:
            tutorial::tutorial_finish_to(be::ScreenId::ModeSelection);
            break;
        case TcAction::Home:
            tutorial::tutorial_finish_to(be::ScreenId::Root);
            break;
    }
}

[[nodiscard]] ImU32 token_u32(theme::ColorToken t) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(t));
}

void draw_text_centered(ImDrawList* dl, float cx, float y, std::string_view text,
                        theme::ColorToken color) {
    const ImVec2 size = ImGui::CalcTextSize(text.data(), text.data() + text.size());
    dl->AddText(ImVec2{cx - size.x * 0.5f, y}, token_u32(color), text.data(),
                text.data() + text.size());
}

void draw_button(ImDrawList* dl, const TcSlot& slot, bool focused) {
    dl->AddRectFilled(ImVec2{slot.rect.x, slot.rect.y},
                      ImVec2{slot.rect.x + slot.rect.w, slot.rect.y + slot.rect.h},
                      token_u32(theme::ColorToken::ButtonBg), 6.0f);
    const ImVec2 ts = ImGui::CalcTextSize(slot.label);
    dl->AddText(ImVec2{slot.rect.x + (slot.rect.w - ts.x) * 0.5f,
                       slot.rect.y + (slot.rect.h - ts.y) * 0.5f},
                token_u32(theme::ColorToken::TextButton), slot.label);
    if (focused) {
        dl->AddRect(ImVec2{slot.rect.x, slot.rect.y},
                    ImVec2{slot.rect.x + slot.rect.w, slot.rect.y + slot.rect.h},
                    token_u32(theme::ColorToken::BorderFocus), 6.0f, 0, 2.0f);
    }
}

void render_tutorial_complete_screen() {
    const Canvas c = viewport_canvas();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // Room background (blurred, for continuity); bg_primary wash when unavailable.
    if (!bridge::draw_asset_image(dl, ImVec2{0.0f, 0.0f}, ImVec2{c.width, c.height},
                                  assets::AssetId::BackgroundRoom)) {
        dl->AddRectFilled(ImVec2{0.0f, 0.0f}, ImVec2{c.width, c.height},
                          token_u32(theme::ColorToken::BgPrimary));
    }

    draw_text_centered(dl, c.width * 0.5f, c.height * 0.40f, kCongrats,
                       theme::ColorToken::TextPrimary);

    const bool guest = is_guest();
    std::array<TcSlot, 3> slots{};
    const std::size_t n = build_buttons(c, guest, slots);

    // Keep the Tab order in sync with the drawn set.
    if (!g_focus_registered || guest != g_last_guest) {
        std::array<be::FocusableId, 3> ids{};
        for (std::size_t i = 0; i < n; ++i) {
            ids[i] = slots[i].focus;
        }
        be::register_focus_list(be::ScreenId::TutorialComplete,
                                std::span<const be::FocusableId>{ids.data(), n});
        g_focus_registered = true;
        g_last_guest = guest;
    }

    const bool kb = be::is_keyboard_mode_active();
    const be::FocusableId focused = be::get_focused_element();
    for (std::size_t i = 0; i < n; ++i) {
        draw_button(dl, slots[i], kb && focused == slots[i].focus);
    }
}

bool on_screen() {
    return be::read_screen_state().current == be::ScreenId::TutorialComplete &&
           !be::is_any_modal_open();
}

bool on_click(const be::MouseEvent& e) {
    if (e.type != be::MouseEventType::MouseDown || e.button != 0) {
        return false;
    }
    const Canvas c = viewport_canvas();
    std::array<TcSlot, 3> slots{};
    const std::size_t n = build_buttons(c, is_guest(), slots);
    for (std::size_t i = 0; i < n; ++i) {
        if (pt(e.x, e.y, slots[i].rect)) {
            be::snap_focus_to(slots[i].focus);
            run_action(slots[i].action);
            return true;
        }
    }
    return false;
}

bool on_key(const be::KeyEvent& e) {
    if (e.type != be::KeyEventType::KeyDown) {
        return false;
    }
    if (e.code != be::KeyCode::Space && e.code != be::KeyCode::Enter) {
        return false;
    }
    if (!be::is_keyboard_mode_active()) {
        return false;
    }
    const be::FocusableId focused = be::get_focused_element();
    std::array<TcSlot, 3> slots{};
    const std::size_t n = build_buttons(viewport_canvas(), is_guest(), slots);
    for (std::size_t i = 0; i < n; ++i) {
        if (slots[i].focus == focused) {
            run_action(slots[i].action);
            return true;
        }
    }
    return false;
}

}  // namespace

void install_tutorial_complete_screen() {
    g_focus_registered = false;
    g_last_guest = true;
    bridge::register_screen_renderer(be::ScreenId::TutorialComplete,
                                     [] { render_tutorial_complete_screen(); });
    (void)be::register_mouse_handler(on_screen, on_click, be::HandlerPriority::ScreenContext,
                                     "tutorial_complete.click");
    (void)be::register_key_handler(on_screen, on_key, be::HandlerPriority::ScreenContext,
                                   "tutorial_complete.key");
}

}  // namespace poker_trainer::screens
