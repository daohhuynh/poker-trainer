#include "screens/clipboard_fallback_modal.hpp"

#include "backbone/focus_manager.hpp"
#include "backbone/modal_state.hpp"

#include "theme/theme_tokens.hpp"

#include <algorithm>
#include <cstring>

#include <imgui.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace poker_trainer::screens {

namespace {

// A Z13-local modal id for the clipboard fallback. The authoritative ModalId set
// is Zone 11's; until Z11 lands, the Post-Round Screen registers its own id with
// modal_state so is_any_modal_open() suppresses the Again/Exit key handlers while
// the fallback is open.
constexpr backbone::ModalId kClipboardFallbackModalId{0x1305u};

constexpr backbone::FocusableId kFocusFallbackClose =
    backbone::make_focusable_id("post_round.clipboard_fallback.close");

}  // namespace

#ifdef __EMSCRIPTEN__

// clang-format off
EM_JS(int, pt_copy_text, (const char* s), {
    var text = UTF8ToString(s);
    try {
        if (navigator.clipboard && navigator.clipboard.writeText) {
            navigator.clipboard.writeText(text);
            return 1;  // async write issued; treat as success (the flash confirms intent)
        }
    } catch (e) { /* fall through to the legacy path */ }
    try {
        var ta = document.createElement('textarea');
        ta.value = text;
        ta.style.position = 'fixed';
        ta.style.opacity = '0';
        document.body.appendChild(ta);
        ta.focus();
        ta.select();
        var ok = document.execCommand('copy');
        document.body.removeChild(ta);
        return ok ? 1 : 0;
    } catch (e) { return 0; }
});

EM_JS(int, pt_web_share, (const char* url), {
    try {
        if (navigator.share) {
            navigator.share({ url: UTF8ToString(url) });
            return 1;  // async share sheet invoked
        }
    } catch (e) { return 0; }
    return 0;  // unsupported -> caller falls back to copy
});
// clang-format on

bool platform_copy_text(std::string_view text) {
    std::array<char, kClipboardTextCapacity> buf{};
    const std::size_t n = std::min(text.size(), buf.size() - 1);
    std::memcpy(buf.data(), text.data(), n);
    buf[n] = '\0';
    return pt_copy_text(buf.data()) != 0;
}

bool platform_web_share(std::string_view url) {
    std::array<char, kClipboardTextCapacity> buf{};
    const std::size_t n = std::min(url.size(), buf.size() - 1);
    std::memcpy(buf.data(), url.data(), n);
    buf[n] = '\0';
    return pt_web_share(buf.data()) != 0;
}

#else

// Native build (tests): no browser. Report failure so the logic paths are
// exercisable; these are never reached through a live UI in the test binaries.
bool platform_copy_text(std::string_view) { return false; }
bool platform_web_share(std::string_view) { return false; }

#endif

void open_clipboard_fallback(ClipboardFallback& modal, std::string_view text) {
    modal.text.fill('\0');
    const std::size_t n = std::min(text.size(), modal.text.size() - 1);
    std::memcpy(modal.text.data(), text.data(), n);
    modal.open = true;
    modal.needs_autoselect = true;

    static constexpr std::array<backbone::FocusableId, 1> kCtx = {kFocusFallbackClose};
    backbone::push_focus_context(kCtx, kFocusFallbackClose, "post_round.clipboard_fallback");
    backbone::notify_modal_opened(kClipboardFallbackModalId);
}

void close_clipboard_fallback(ClipboardFallback& modal) {
    if (!modal.open) {
        return;
    }
    modal.open = false;
    backbone::notify_modal_closed(kClipboardFallbackModalId);
    backbone::pop_focus_context();
}

void render_clipboard_fallback(ClipboardFallback& modal) {
    if (!modal.open) {
        return;
    }
    const ImGuiViewport* vp = ImGui::GetMainViewport();

    // Scrim behind the modal.
    ImVec4 scrim = theme::get_color(theme::ColorToken::BgModalScrim);
    ImGui::GetForegroundDrawList()->AddRectFilled(
        ImVec2{0.0f, 0.0f}, ImVec2{vp->Size.x, vp->Size.y},
        ImGui::ColorConvertFloat4ToU32(scrim));

    ImGui::SetNextWindowPos(ImVec2{vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f},
                            ImGuiCond_Always, ImVec2{0.5f, 0.5f});
    ImGui::SetNextWindowSize(ImVec2{vp->Size.x * 0.4f, 0.0f}, ImGuiCond_Always);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("##clipboard_fallback", nullptr, flags)) {
        ImGui::TextUnformatted("Copy this:");
        ImGui::SameLine(ImGui::GetWindowWidth() - 40.0f);
        if (ImGui::SmallButton("X")) {
            close_clipboard_fallback(modal);
            ImGui::End();
            return;
        }
        if (modal.needs_autoselect) {
            ImGui::SetKeyboardFocusHere();
            modal.needs_autoselect = false;
        }
        ImGui::PushItemWidth(-1.0f);
        ImGui::InputText("##fallback_field", modal.text.data(), modal.text.size(),
                         ImGuiInputTextFlags_AutoSelectAll);
        ImGui::PopItemWidth();
    }
    ImGui::End();
}

}  // namespace poker_trainer::screens
