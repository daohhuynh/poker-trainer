#include "modal/modal_base.hpp"

#include "modal/auth_modals.hpp"
#include "modal/confirm_modal.hpp"
#include "modal/help_modal.hpp"
#include "modal/modals.hpp"
#include "modal/outage_banner.hpp"

#include "backbone/animation_clock.hpp"
#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"
#include "backbone/modal_state.hpp"
#include "backbone/screen_state.hpp"

#include "assets/asset_paths.hpp"
#include "theme/theme_tokens.hpp"

#include <array>
#include <cassert>
#include <functional>
#include <optional>
#include <span>
#include <string_view>

#include <imgui.h>
#ifdef __EMSCRIPTEN__
#include <imgui_internal.h>  // ImGui::ClearActiveID — drop a provider modal's active text input on close
#endif

#include "bridge/asset_image.hpp"
#include "bridge/focus_registry.hpp"
#include "bridge/screen_dispatch.hpp"

// Zone 11 core: the app-root runtime pointer, the open/close lifecycle (driving the
// sealed backbone modal stack + focus_manager focus traps), the shared modal shell
// chrome, the overlay-render dispatch, the Settings/Shop seam shells, and the
// ModalLayer + cluster keyboard handlers. Per-modal bodies live in help_modal.cpp /
// confirm_modal.cpp / leaderboard_view.cpp.

namespace poker_trainer::modal {

namespace {

// App-root runtime, owned by Z05 boot; set once at install. Free functions reach it
// through this pointer (mirrors bridge::runtime() / game_screen's g_installed_runtime).
// Null before install / in native tests that never install — every entry null-guards.
ModalRuntime* g_runtime = nullptr;

[[nodiscard]] ImU32 token_u32(theme::ColorToken token) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(token));
}

// The Z14 tutorial-start seam: the Help modal's "Open Tutorial" button initiates the
// tutorial overlay (Notes — Tutorial System). Z14 is unbuilt, so this is inert.
void tutorial_start_seam() {
    // SEAM(Z14): tutorial_start(). No-op until the tutorial system lands.
}

// Push the focus trap for `id` (the modal's own focusables, armed at its default).
void push_modal_focus(backbone::ModalId id) {
    // A registered content provider supplies its own focus list (the generic seam,
    // checked first so a provider modal traps on its full list, not the shell's X).
    if (const ModalContentProvider* p = modal_content_for(id);
        p != nullptr && p->focus_list) {
        // A provider that leaves initial_focus unset (kNoFocus) defaults to the first stop
        // of its list. The auth modal uses this so its default focus tracks the mode-
        // dependent list (Sign In -> id/email, Sign Up -> username) without a fixed id;
        // Settings sets initial_focus explicitly (kFocusSearch) and is unaffected.
        const std::span<const backbone::FocusableId> list = p->focus_list();
        const backbone::FocusableId initial =
            p->initial_focus != backbone::kNoFocus
                ? p->initial_focus
                : (list.empty() ? backbone::kNoFocus : list.front());
        backbone::push_focus_context(list, initial, "modal.content");
        return;
    }
    if (id == kHelpModalId) {
        static constexpr std::array<backbone::FocusableId, 2> kHelp{kHelpTutorial, kHelpClose};
        backbone::push_focus_context(kHelp, kHelpTutorial, "modal.help");
    } else if (id == kLeaveDrillConfirmId) {
        backbone::push_focus_context(kConfirmFocusOrder, kConfirmNo, "modal.confirm");
    } else if (id == kSettingsModalId) {
        static constexpr std::array<backbone::FocusableId, 1> kS{kSettingsShellClose};
        backbone::push_focus_context(kS, kSettingsShellClose, "modal.settings");
    } else if (id == kShopModalId) {
        static constexpr std::array<backbone::FocusableId, 1> kSh{kShopShellClose};
        backbone::push_focus_context(kSh, kShopShellClose, "modal.shop");
    } else {
        static constexpr std::array<backbone::FocusableId, 1> kL{kShopShellClose};
        backbone::push_focus_context(kL, kShopShellClose, "modal.leaderboard");
    }
}

}  // namespace

ModalRuntime* modal_runtime() { return g_runtime; }

const ModalContentProvider* modal_content_for(backbone::ModalId id) {
    if (g_runtime == nullptr) {
        return nullptr;
    }
    for (const auto& [pid, provider] : g_runtime->content_providers) {
        if (pid == id) {
            return &provider;
        }
    }
    return nullptr;
}

void register_modal_content(backbone::ModalId id, ModalContentProvider provider) {
    if (g_runtime == nullptr) {
        return;  // native tests never install; the registry is wired at boot
    }
    for (auto& [pid, existing] : g_runtime->content_providers) {
        if (pid == id) {
            existing = std::move(provider);  // replace
            return;
        }
    }
    g_runtime->content_providers.emplace_back(id, std::move(provider));
}

// ===== Open / close lifecycle =====

void open_modal(backbone::ModalId id) {
    if (g_runtime == nullptr) {
        return;
    }
    g_runtime->modal_just_opened = true;
    backbone::notify_modal_opened(id);  // drives modal_stack_depth (swoosh edge + Z10 pause)
    push_modal_focus(id);
    if (const ModalContentProvider* p = modal_content_for(id); p != nullptr && p->on_open) {
        p->on_open();  // provider builds its registry / resets per-open state
    }
}

void close_modal() {
    const std::optional<backbone::ModalId> id = backbone::current_modal_id();
    if (!id.has_value()) {
        return;
    }
    // Provider on_close BEFORE the pop: it clears the provider's focus-registry
    // entries while its context is still the active one (mirrors custom_popup's
    // close-before-launch ordering — no provider closure is mid-invocation here).
    const ModalContentProvider* p = modal_content_for(*id);
    if (p != nullptr && p->on_close) {
        p->on_close();
    }
    backbone::notify_modal_closed(*id);
    // Invariant: every modal open pushed exactly one focus context, so there is one
    // to pop here. A debug assert catches any open/close imbalance (stripped in the
    // Release wasm build under NDEBUG). The balance is what keeps Tab healthy after a
    // modal-close-that-navigates (e.g. leave-drill -> Yes -> Mode Selection): close
    // FULLY first (pop here), then the caller navigates.
    assert(backbone::context_depth() > 0 && "modal close without a pushed focus context");
    backbone::pop_focus_context();
#ifdef __EMSCRIPTEN__
    // Generic provider-modal text-capture teardown. A provider modal (Zone 12 Settings;
    // a future Shop) may carry text inputs that grabbed ImGui's keyboard focus. Popping
    // the focus context above restores the underlying screen's focus pointer, but ImGui's
    // active text item does NOT tear down with it — left active, it captures the next
    // keystrokes on the restored screen (e.g. the Game screen's math boxes, which then
    // swallow Space as a literal space and eat digits). Drop it here so any text-field
    // provider modal relinquishes ImGui keyboard capture on every close path (X / click-
    // outside / Escape all route through here). Gated to provider modals so the non-text
    // Help / confirm / Shop shells are untouched.
    if (p != nullptr) {
        ImGui::ClearActiveID();
    }
#endif
    if (g_runtime != nullptr) {
        g_runtime->modal_just_opened = false;
        g_runtime->confirm = ConfirmSpec{};
    }
}

void open_help_modal() { open_modal(kHelpModalId); }
void open_settings_modal() { open_modal(kSettingsModalId); }
void open_shop_modal() { open_modal(kShopModalId); }

void open_confirm_modal(ConfirmSpec spec) {
    if (g_runtime == nullptr) {
        return;
    }
    // Reuse the shared confirmation instance + its No->Yes->X focus list and the
    // ModalLayer Yes/No handling. Stacks over an open Settings modal (push/pop).
    g_runtime->confirm = std::move(spec);
    open_modal(kLeaveDrillConfirmId);
}

void open_leave_drill_confirm() {
    if (g_runtime == nullptr) {
        return;
    }
    // Leave-drill: Yes returns to Mode Selection (NOT Root — the X is for switching
    // drill modes mid-session). The ceremonial transition is a Z14 seam (instant
    // cut here). ARCHITECTURE leaves the body text to the visual pass; this is clear,
    // spec-faithful wording.
    g_runtime->confirm = ConfirmSpec{
        .body = "Leave this drill? Your progress on this scenario will be lost.",
        .on_yes = [] { backbone::set_screen(backbone::ScreenId::ModeSelection, std::nullopt); }};
    open_modal(kLeaveDrillConfirmId);
}

void trigger_outage_banner(std::string_view message) {
    if (g_runtime == nullptr) {
        return;
    }
    banner_trigger(g_runtime->banner, message, backbone::total_ms_since_app_start());
}

// ===== Shared modal shell chrome =====

bool modal_begin_centered(const char* imgui_id, float w_frac, float h_frac) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2{vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f},
                            ImGuiCond_Appearing, ImVec2{0.5f, 0.5f});
    ImGui::SetNextWindowSize(ImVec2{vp->Size.x * w_frac, vp->Size.y * h_frac}, ImGuiCond_Appearing);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoMove;
    return ImGui::Begin(imgui_id, nullptr, flags);
}

void modal_end() { ImGui::End(); }

void modal_draw_pill_header(assets::AssetId icon, const char* name) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float h = ImGui::GetTextLineHeight() * 1.4f;

    const ImVec2 icon_min = p;
    const ImVec2 icon_max{p.x + h, p.y + h};
    if (!bridge::draw_asset_image(dl, icon_min, icon_max, icon)) {
        dl->AddRect(icon_min, icon_max, token_u32(theme::ColorToken::TextPrimary), 0.0f, 0, 1.0f);
    }

    const float pad = h * 0.45f;
    const ImVec2 ts = ImGui::CalcTextSize(name);
    const ImVec2 pill_min{icon_max.x + pad * 0.5f, p.y};
    const ImVec2 pill_max{pill_min.x + ts.x + pad * 2.0f, p.y + h};
    dl->AddRectFilled(pill_min, pill_max, token_u32(theme::ColorToken::ButtonBg), h * 0.5f);
    dl->AddText(ImVec2{pill_min.x + pad, p.y + (h - ts.y) * 0.5f},
                token_u32(theme::ColorToken::TextPrimary), name);

    ImGui::Dummy(ImVec2{0.0f, h + ImGui::GetStyle().ItemSpacing.y});
}

bool modal_draw_x_close(backbone::FocusableId close_focus) {
    // Draw the X "out of band" at the top-right, then restore the cursor so the
    // caller's pill header / body start at the top-left (the X overlaps the header
    // row rather than pushing it down).
    const ImVec2 saved = ImGui::GetCursorPos();
    const float btn = ImGui::GetTextLineHeight() * 1.6f;
    const float region_w = ImGui::GetWindowContentRegionMax().x;
    ImGui::SetCursorPos(ImVec2{region_w - btn, ImGui::GetStyle().WindowPadding.y});
    const bool clicked = ImGui::Button("X##modal_close", ImVec2{btn, btn});
    const std::uint32_t ring = token_u32(theme::ColorToken::BorderFocus);
    bridge::draw_focus_ring(close_focus, ring);
    ImGui::SetCursorPos(saved);
    return clicked;
}

bool modal_click_outside_dismissed() {
    if (g_runtime == nullptr) {
        return false;
    }
    // Opening-frame guard: keep it raised until the opening mouse button releases,
    // then arm outside-dismiss for any subsequent click. "Outside" is WantCaptureMouse
    // == false (true whenever over the modal window or an active item), exactly as
    // custom_popup does — IsWindowHovered misreads an in-modal widget click as outside.
    if (g_runtime->modal_just_opened) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            g_runtime->modal_just_opened = false;
        }
        return false;
    }
    return !ImGui::GetIO().WantCaptureMouse && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
}

bool modal_is_locked() { return backbone::is_modal_locked(); }

void modal_draw_lock_banner() {
    if (!modal_is_locked()) {
        return;
    }
    ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::TextSecondary));
    ImGui::TextUnformatted("Locked during tutorial — close to continue");
    ImGui::PopStyleColor();
    ImGui::Separator();
}

void modal_begin_locked_controls() {
    if (modal_is_locked()) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.4f);
        ImGui::BeginDisabled();  // silently suppresses clicks on the wrapped controls
    }
}

void modal_end_locked_controls() {
    if (modal_is_locked()) {
        ImGui::EndDisabled();
        ImGui::PopStyleVar();
    }
}

// ===== Settings / Shop seam shells =====

namespace {

// A minimal shell: the pill header + an explicit "content lives in another zone"
// note + the X close. Dismiss via X / click-outside (the ModalLayer key handler
// covers Escape + keyboard X). Content is Z12 (Settings) / Module 7 (Shop).
void render_seam_shell(const char* imgui_id, assets::AssetId icon, const char* name,
                       backbone::FocusableId close_focus, const char* seam_note) {
    if (modal_begin_centered(imgui_id, kClusterModalWidthFrac, kClusterModalHeightFrac)) {
        const bool x_clicked = modal_draw_x_close(close_focus);
        modal_draw_pill_header(icon, name);
        modal_draw_lock_banner();
        ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::TextSecondary));
        ImGui::TextWrapped("%s", seam_note);
        ImGui::PopStyleColor();
        if (x_clicked || modal_click_outside_dismissed()) {
            modal_end();
            close_modal();
            return;
        }
    }
    modal_end();
}

}  // namespace

// Render a registered content provider into the standard cluster-modal shell: chrome
// (window + X close + pill header + lock banner) drawn by the shell, the interior by
// the provider's render_body. Dismiss on X / click-outside (Escape + keyboard X are
// the ModalLayer handler's job). The provider's own controls (reset buttons, inputs)
// drive themselves inside render_body.
void render_provider_shell(const ModalContentProvider& p) {
    if (modal_begin_centered("##settings_modal", kClusterModalWidthFrac, kClusterModalHeightFrac)) {
        const bool x_clicked = modal_draw_x_close(p.close_focus);
        modal_draw_pill_header(p.header_icon, p.header_name);
        modal_draw_lock_banner();
        if (p.render_body) {
            p.render_body();
        }
        if (x_clicked || modal_click_outside_dismissed()) {
            modal_end();
            close_modal();
            return;
        }
    }
    modal_end();
}

void render_settings_shell() {
    // Zone 12 fills the body via the content-provider seam; until it registers one
    // (or in a build without Z12) the placeholder seam shell renders.
    if (const ModalContentProvider* p = modal_content_for(kSettingsModalId);
        p != nullptr && p->render_body) {
        render_provider_shell(*p);
        return;
    }
    render_seam_shell("##settings_modal", assets::AssetId::IconSettings, "Settings",
                      kSettingsShellClose,
                      "Settings content is implemented by Zone 12 (Settings Page). This shell "
                      "provides the modal frame, header, and close behavior.");
}

void render_shop_shell() {
    render_seam_shell("##shop_modal", assets::AssetId::IconShop, "Shop", kShopShellClose,
                      "Shop track-purchase content is Module 7 (unbuilt). This shell provides the "
                      "modal frame, header, and close behavior. The Leaderboard view also lives in "
                      "this frame.");
}

// ===== Overlay render dispatch =====

void render_modal_overlay() {
    if (g_runtime == nullptr) {
        return;
    }
    const std::optional<backbone::ModalId> id = backbone::current_modal_id();
    if (id.has_value()) {
        // Scrim behind the modal (above the screen, below the modal window). Drawn on
        // the background draw list so the ImGui modal window composites on top.
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::GetBackgroundDrawList()->AddRectFilled(
            vp->Pos, ImVec2{vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y},
            token_u32(theme::ColorToken::BgModalScrim));

        if (*id == kHelpModalId) {
            render_help_modal();
        } else if (*id == kLeaveDrillConfirmId) {
            render_confirm_modal(g_runtime->confirm);
        } else if (*id == kSettingsModalId) {
            render_settings_shell();
        } else if (*id == kShopModalId) {
            render_shop_shell();
        } else if (*id == kLeaderboardModalId) {
            render_leaderboard_view(*g_runtime);
        } else if (*id == kAuthModalId) {
            render_auth_modal();
        }
    }

    // The outage banner renders above any modal (top-center foreground).
    render_outage_banner(g_runtime->banner);
}

// ===== Keyboard handlers =====

namespace {

// ModalLayer: Escape dismisses (for confirmations, Escape = No); Space/Enter
// activate the focused modal control. Other keys (Tab) fall through to the backbone
// focus-nav handler, which cycles within the modal's trapped context.
bool on_modal_key(const backbone::KeyEvent& e) {
    if (e.type != backbone::KeyEventType::KeyDown) {
        return false;
    }
    const std::optional<backbone::ModalId> id = backbone::current_modal_id();
    if (!id.has_value()) {
        return false;
    }
    if (e.code == backbone::KeyCode::Escape) {
        close_modal();  // confirmations: Escape == No
        return true;
    }
    // A provider modal (Zone 12 Settings) owns Space/Enter/arrows on its controls;
    // route the key to its dispatch (which withholds text-cursor keys itself). Keys it
    // does not consume fall through to the backbone focus-nav handler (Tab). Modals
    // without a provider keep the legacy Space/Enter activation below, unchanged.
    if (const ModalContentProvider* p = modal_content_for(*id); p != nullptr) {
        return p->dispatch ? p->dispatch(e) : false;
    }
    if (e.code != backbone::KeyCode::Space && e.code != backbone::KeyCode::Enter) {
        return false;
    }
    const backbone::FocusableId focused = backbone::get_focused_element();
    if (*id == kHelpModalId) {
        if (focused == kHelpTutorial) {
            close_modal();        // close the Help modal first...
            tutorial_start_seam();  // ...then start the tutorial (Z14 seam)
        } else {
            close_modal();  // X close (or any other focus) dismisses
        }
        return true;
    }
    if (*id == kLeaveDrillConfirmId) {
        if (focused == kConfirmYes) {
            const std::function<void()> yes =
                g_runtime != nullptr ? g_runtime->confirm.on_yes : std::function<void()>{};
            close_modal();
            if (yes) {
                yes();
            }
        } else {
            close_modal();  // No / X close
        }
        return true;
    }
    // Settings / Shop / Leaderboard shells: the only focusable is the X close.
    close_modal();
    return true;
}

// ScreenContext: Space/Enter on a focused cluster icon activates it. Space||Enter on
// Mode Selection and Post-Round; Space-ONLY on the Game screen (Enter there is the
// math submit / tier-advance, owned by Z09). Registered after the screens' own
// activate handlers, which leave a focused cluster icon's key unconsumed.
bool on_cluster_key(const backbone::KeyEvent& e) {
    if (e.type != backbone::KeyEventType::KeyDown || g_runtime == nullptr ||
        !g_runtime->has_cluster || backbone::is_any_modal_open()) {
        return false;
    }
    const bool space = e.code == backbone::KeyCode::Space;
    const bool enter = e.code == backbone::KeyCode::Enter;
    if (!space && !enter) {
        return false;
    }
    if (g_runtime->cluster.screen == ClusterScreen::Game && enter) {
        return false;  // Game cluster is Space-only
    }
    const std::optional<ClusterIcon> icon =
        cluster_action_for_focus(backbone::get_focused_element());
    if (!icon.has_value()) {
        return false;
    }
    activate_cluster_icon(*icon);
    return true;
}

[[nodiscard]] bool cluster_screen_active() {
    const backbone::ScreenId s = backbone::read_screen_state().current;
    return s == backbone::ScreenId::ModeSelection || s == backbone::ScreenId::Game ||
           s == backbone::ScreenId::PostRound;
}

// Game screen: Escape opens the leave-drill confirmation (ARCHITECTURE Notes —
// Escape Key Behavior). Once the confirm is open, Escape == No is handled by the
// ModalLayer handler above; this handler is gated to Game with no modal open.
bool on_game_escape(const backbone::KeyEvent& e) {
    if (e.type != backbone::KeyEventType::KeyDown || e.code != backbone::KeyCode::Escape) {
        return false;
    }
    open_leave_drill_confirm();
    return true;
}

[[nodiscard]] bool game_no_modal() {
    return backbone::read_screen_state().current == backbone::ScreenId::Game &&
           !backbone::is_any_modal_open();
}

}  // namespace

void install_modals(ModalRuntime& runtime) {
    g_runtime = &runtime;

    bridge::register_overlay_renderer(render_modal_overlay);

    // ModalLayer key handler: active only while a modal is open (the topmost
    // interactive layer below the tutorial overlay).
    (void)backbone::register_key_handler(
        [] { return backbone::is_any_modal_open(); }, on_modal_key,
        backbone::HandlerPriority::ModalLayer, "modal.keys");

    // Cluster keyboard activation (ScreenContext), gated to the cluster screens with
    // no modal open. Registered last so the screens' own activate handlers run first.
    (void)backbone::register_key_handler(
        [] { return cluster_screen_active() && !backbone::is_any_modal_open(); }, on_cluster_key,
        backbone::HandlerPriority::ScreenContext, "modal.cluster_keys");

    // Game-screen Escape -> leave-drill confirmation (ScreenContext, gated to Game
    // with no modal open; Z09's Game handlers leave Escape unconsumed).
    (void)backbone::register_key_handler(game_no_modal, on_game_escape,
                                         backbone::HandlerPriority::ScreenContext,
                                         "modal.game_escape");
}

}  // namespace poker_trainer::modal
