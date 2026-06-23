#include "settings/settings_modal.hpp"

#include "settings/account_modal.hpp"
#include "settings/auth_form.hpp"
#include "settings/search.hpp"
#include "settings/settings.hpp"
#include "settings/settings_logic.hpp"

#include "modal/modal_base.hpp"
#include "modal/modals.hpp"

#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"
#include "backbone/modal_state.hpp"  // current_modal_id (search-filtered Tab gate)

#include "theme/theme.hpp"
#include "theme/theme_tokens.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>
#ifdef __EMSCRIPTEN__
#include <imgui_internal.h>  // Bug C: clear ImGui's nav/keyboard target (the neutral-click equivalent)
#endif

#include "bridge/focus_registry.hpp"

// Zone 12 — Settings modal content. The body (search + sidebar + scrollable sections),
// the per-control widgets (coupled/clamped over the pure cores in settings_logic.hpp),
// the shared focus-registry population + dispatch (mirroring custom_popup), the two
// sub-modals (reset-section multi-select, legal document), and the boot wiring of the
// Zone 11 content-provider seam. All sections are rendered here as internal functions;
// the ZONES "one file per section" split is a mechanical follow-up with no logic change.

namespace poker_trainer::settings {

namespace {

// ----- small render helpers -----

[[nodiscard]] ImU32 token_u32(theme::ColorToken t) {
    return ImGui::ColorConvertFloat4ToU32(theme::get_color(t));
}

// Same color at ~50% alpha (for the ghost-default marker). ImU32 packs 0xAABBGGRR.
[[nodiscard]] ImU32 faded(ImU32 c) { return (c & 0x00FFFFFFu) | 0x80000000u; }

// Keystroke digit filter for the numeric text inputs (volume, custom time, street).
int digit_char_filter(ImGuiInputTextCallbackData* data) {
    if (data->EventChar < 256 && is_digit(static_cast<char>(data->EventChar))) {
        return 0;  // keep
    }
    return 1;  // discard
}

void format_int(std::array<char, 4>& buf, int v) {
    std::snprintf(buf.data(), buf.size(), "%d", v);
}

// Mouse-click into a focusable: snap focus_manager + engage keyboard mode (required
// under the reconcile so the ring + ImGui text focus + focus_manager stay in lockstep).
void focus_on_click(backbone::FocusableId id) {
    backbone::activate_keyboard_mode();
    backbone::snap_focus_to(id);
}

// Scroll-follow-focus (Bug C): the SINGLE scroll authority for focus-following the
// Settings body. When `scroll_to` names the control just submitted (`id`), center it with
// ONE explicit, clamped SetScrollY computed from the control's content-space center. The
// item rect is taken from GetItemRectMin/Max, which ItemAdd records BEFORE the clip test,
// so it is reliable even when the focused control is currently scrolled off-screen — this
// is why we no longer use SetScrollHereY (its internal CursorPosPrevLine measure produced
// the constant-59 snap). Runs AFTER widget submission (see the widget helpers), so it is
// the frame's final ScrollTarget. render() raises scroll_to ONLY on the frame keyboard
// focus moved to a NON-text control (text fields are revealed by their non-text sibling
// plus ImGui's own SetKeyboardFocusHere nav scroll — see render_settings_modal; never
// during a sidebar jump), so on no-focus-change frames this writes nothing — no competing
// writer between Tabs. Acts on the body child. ImGui-only; a no-op natively (rendering is
// browser-verified).
void scroll_into_view_if([[maybe_unused]] backbone::FocusableId scroll_to,
                         [[maybe_unused]] backbone::FocusableId id) {
#ifdef __EMSCRIPTEN__
    if (scroll_to.value == 0 || scroll_to != id) {
        return;
    }
    const float win_top = ImGui::GetWindowPos().y;
    const float view_h = ImGui::GetWindowSize().y;
    const float before = ImGui::GetScrollY();
    const float max_y = ImGui::GetScrollMaxY();
    // Content-space center of the focused control (reliable even when clipped).
    const float content_y =
        (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f - win_top + before;
    float target = content_y - view_h * 0.5f;
    target = target < 0.0f ? 0.0f : (target > max_y ? max_y : target);
    ImGui::SetScrollY(target);  // ONE clamped write; the frame's final ScrollTarget
#endif
}

// Bug C — the neutral-click equivalent. grab_keyboard_if_target() calls
// SetKeyboardFocusHere() on a text field to steer the typing target; that ALSO records
// the field as ImGui's keyboard/nav item (g.NavId) and arms ImGui's own nav scroll-
// into-view, which applies one frame LATE (next NewFrame's NavUpdate). Once focus has
// tabbed onto a non-text control, that stale nav target lingers and its deferred scroll
// fires on a frame our follow stays silent, overriding our SetScrollY (Bug C's yank).
// A manual neutral click cleared g.NavId and visibly stopped it; ImGui's own click path
// does the same, SetNavID(0,...) (imgui.cpp ~12900). This evicts
// it in code: drop any in-flight nav move, then forget the focused item. Called ONLY on
// frames a non-text control holds focus, so an active input keeps its typing target
// (Bug A's leaked-capture guard). ImGui-only; a no-op natively.
void clear_imgui_keyboard_nav() {
#ifdef __EMSCRIPTEN__
    ImGuiContext& g = *ImGui::GetCurrentContext();
    if (g.NavMoveSubmitted) {
        ImGui::NavMoveRequestCancel();  // no deferred nav scroll survives the tab-off
    }
    if (g.NavId != 0) {
        ImGui::SetNavID(0, g.NavLayer, 0, ImRect{});  // forget the text field we left
    }
#endif
}

// ----- Account auth state (read through the boot-wired seam; guest when unwired) -----

[[nodiscard]] bool account_is_logged_in(const SettingsModalState& s) {
    return s.account != nullptr && account_snapshot(*s.account).is_authenticated;
}

// The Account section's first focus stop for the active auth state (sidebar jump target).
[[nodiscard]] backbone::FocusableId account_first_focus(const SettingsModalState& s) {
    return account_is_logged_in(s) ? kAcViewProfile : kAcSignIn;
}

// Build the active focus order from kSettingsFocusOrder, swapping the two-stop account
// block (Sign In / Sign Up) for the four-stop logged-in block when authenticated. For the
// guest case the result is byte-for-byte kSettingsFocusOrder, so Part 1's Tab/search
// behavior is unchanged; only the (currently unreachable) logged-in traversal differs.
void build_account_focus_order(SettingsModalState& s) {
    s.logged_in_focus = account_is_logged_in(s);
    std::size_t n = 0;
    for (const backbone::FocusableId id : kSettingsFocusOrder) {
        if (id == kAcSignIn) {
            if (s.logged_in_focus) {
                s.active_focus_order[n++] = kAcViewProfile;
                s.active_focus_order[n++] = kAcChangePassword;
                s.active_focus_order[n++] = kAcSignOut;
                s.active_focus_order[n++] = kAcDeleteAccount;
            } else {
                s.active_focus_order[n++] = kAcSignIn;
                s.active_focus_order[n++] = kAcSignUp;
            }
        } else if (id == kAcSignUp) {
            // already emitted alongside kAcSignIn
        } else {
            s.active_focus_order[n++] = id;
        }
    }
    s.active_focus_count = n;
}

// Section header (accent label + scroll anchor + separator). Returns false when the
// search query filters the whole section out (the body skips it).
bool begin_section(SettingsModalState& s, SettingsSection section, std::string_view query) {
    if (!section_has_match(section, query)) {
        return false;
    }
    const std::string_view label = section_label(section);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::AccentPrimary));
    ImGui::TextUnformatted(label.data(), label.data() + label.size());
    ImGui::PopStyleColor();
    if (s.scroll_pending && s.scroll_target == section) {
        ImGui::SetScrollHereY(0.0f);  // jump this section's header to the top
        s.scroll_pending = false;
    }
    ImGui::Separator();
    return true;
}

// ----- generic control widgets (draw + focus ring + click snap) -----

bool widget_checkbox(backbone::FocusableId id, const char* label, bool& value, ImU32 ring,
                     backbone::FocusableId scroll_to = backbone::kNoFocus) {
    const bool changed = ImGui::Checkbox(label, &value);
    if (ImGui::IsItemClicked()) {
        focus_on_click(id);
    }
    bridge::draw_focus_ring(id, ring);
    scroll_into_view_if(scroll_to, id);
    return changed;
}

bool widget_button(backbone::FocusableId id, const char* label, ImU32 ring,
                   backbone::FocusableId scroll_to = backbone::kNoFocus) {
    const bool clicked = ImGui::Button(label);
    if (ImGui::IsItemClicked()) {
        focus_on_click(id);
    }
    bridge::draw_focus_ring(id, ring);
    scroll_into_view_if(scroll_to, id);
    return clicked;
}

// Int slider [lo,hi] with a faded ghost-default tick at `def`. value updated by ref.
bool widget_slider(backbone::FocusableId id, const char* imgui_id, int& value, int lo, int hi,
                   int def, ImU32 ring, backbone::FocusableId scroll_to = backbone::kNoFocus) {
    ImGui::SetNextItemWidth(-1.0f);
    const bool changed = ImGui::SliderInt(imgui_id, &value, lo, hi);
    if (ImGui::IsItemClicked()) {
        focus_on_click(id);
    }
    const ImVec2 mn = ImGui::GetItemRectMin();
    const ImVec2 mx = ImGui::GetItemRectMax();
    const float frac = slider_fraction(static_cast<float>(def), static_cast<float>(lo),
                                       static_cast<float>(hi));
    const float gx = mn.x + frac * (mx.x - mn.x);
    ImGui::GetWindowDrawList()->AddLine(ImVec2{gx, mn.y}, ImVec2{gx, mx.y},
                                        faded(token_u32(theme::ColorToken::BorderDefault)), 2.0f);
    bridge::draw_focus_ring(id, ring);
    scroll_into_view_if(scroll_to, id);
    return changed;
}

bool widget_int_input(backbone::FocusableId id, const char* imgui_id, char* buf, std::size_t buflen,
                      const bridge::FocusReconcile& rec, float width, ImU32 ring,
                      backbone::FocusableId scroll_to = backbone::kNoFocus) {
    bridge::grab_keyboard_if_target(rec, id);
    ImGui::SetNextItemWidth(width);
    const bool edited = ImGui::InputText(imgui_id, buf, buflen, ImGuiInputTextFlags_CallbackCharFilter,
                                         &digit_char_filter);
    if (ImGui::IsItemClicked()) {
        focus_on_click(id);
    }
    bridge::draw_focus_ring(id, ring);
    scroll_into_view_if(scroll_to, id);
    return edited;
}

bool widget_combo(backbone::FocusableId id, const char* imgui_id, int& index,
                  std::span<const char* const> options, ImU32 ring,
                  backbone::FocusableId scroll_to = backbone::kNoFocus) {
    const int count = static_cast<int>(options.size());
    if (index < 0 || index >= count) {
        index = 0;
    }
    ImGui::SetNextItemWidth(-1.0f);
    bool changed = false;
    if (ImGui::BeginCombo(imgui_id, options[static_cast<std::size_t>(index)])) {
        for (int i = 0; i < count; ++i) {
            const bool selected = (i == index);
            if (ImGui::Selectable(options[static_cast<std::size_t>(i)], selected)) {
                index = i;
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemClicked()) {
        focus_on_click(id);
    }
    bridge::draw_focus_ring(id, ring);
    scroll_into_view_if(scroll_to, id);
    return changed;
}

// ----- option label tables -----

constexpr std::array<const char*, 4> kThemeOptions{"No Limit", "Slate", "Ocean", "Sage"};
constexpr std::array<const char*, 4> kMusicOptions{"Lounge Jazz", "Classical", "Bossa Nova",
                                                   "Ambient"};
constexpr std::array<const char*, 2> kRecapTabOptions{"Tier 1", "Summary"};
constexpr std::array<const char*, 2> kUnitOptions{"Cash", "Big Blinds"};

// ----- About / Credits data -----

constexpr const char* kAppVersion = "Poker Trainer V1.1";
struct Credit {
    const char* name;
    const char* note;
};
constexpr std::array<Credit, 6> kCredits{{
    {"Dear ImGui", "UI rendering (MIT)"},
    {"Emscripten", "WebAssembly toolchain (MIT / University of Illinois)"},
    {"miniaudio", "audio playback (public domain / MIT-0)"},
    {"stb_image / stb_vorbis", "PNG + Vorbis decoding (public domain / MIT)"},
    {"GoogleTest", "unit testing (BSD-3-Clause)"},
    {"Auth0", "authentication (Part 2)"},
}};

// ----- per-control commit helpers (mouse + keyboard share these) -----

void apply_audio_now(SettingsModalState& s) {
    if (s.apply_audio && s.live != nullptr) {
        s.apply_audio(s.live->audio);
    }
}
void persist_now(SettingsModalState& s) {
    if (s.persist) {
        s.persist();
    }
}

void set_volume(SettingsModalState& s, int v) {
    if (s.live == nullptr) {
        return;
    }
    s.live->audio.volume = static_cast<std::uint8_t>(clamp_int(v, 0, 100));
    on_setting_change(s, SettingId::Volume);
}
void set_custom_time(SettingsModalState& s, int v) {
    if (s.live == nullptr) {
        return;
    }
    s.live->gameplay.time_pressure_custom_seconds = static_cast<std::uint16_t>(clamp_int(v, 1, 300));
    on_setting_change(s, SettingId::TimePressure);
}
void set_diff_low(SettingsModalState& s, int d) {
    if (s.live == nullptr) {
        return;
    }
    const DifficultyDisplay r = set_difficulty_low(difficulty_display_range(s.live->gameplay), d);
    apply_difficulty(s.live->gameplay, r);
    on_setting_change(s, SettingId::DifficultyRange);
}
void set_diff_high(SettingsModalState& s, int d) {
    if (s.live == nullptr) {
        return;
    }
    const DifficultyDisplay r = set_difficulty_high(difficulty_display_range(s.live->gameplay), d);
    apply_difficulty(s.live->gameplay, r);
    on_setting_change(s, SettingId::DifficultyRange);
}
void set_theme_index(SettingsModalState& s, int idx) {
    if (s.live == nullptr) {
        return;
    }
    s.live->display.active_theme_id =
        static_cast<std::uint8_t>(clamp_int(idx, 0, theme::kThemeIdCount - 1));
    on_setting_change(s, SettingId::Theme);
}
void cycle_theme(SettingsModalState& s) {
    if (s.live == nullptr) {
        return;
    }
    set_theme_index(s, (s.live->display.active_theme_id + 1) % theme::kThemeIdCount);
}
void set_music_index(SettingsModalState& s, int idx) {
    if (s.live == nullptr) {
        return;
    }
    s.live->audio.current_music_genre = static_cast<ActiveMusicGenre>(clamp_int(idx, 0, 3));
    on_setting_change(s, SettingId::MusicType);
}
void cycle_music(SettingsModalState& s) {
    if (s.live == nullptr) {
        return;
    }
    set_music_index(s, (static_cast<int>(s.live->audio.current_music_genre) + 1) % 4);
}
void set_unit_index(SettingsModalState& s, int idx) {
    if (s.live == nullptr) {
        return;
    }
    s.live->units.cash_mode = (idx == 0);  // 0 = Cash, 1 = Big Blinds
    on_setting_change(s, SettingId::UnitToggle);
}
void set_recap_tab_index(SettingsModalState& s, int idx) {
    if (s.live == nullptr) {
        return;
    }
    s.live->recap.default_aggressor_recap_tab =
        (idx == 1) ? DefaultAggressorRecapTab::Summary : DefaultAggressorRecapTab::Tier1;
    on_setting_change(s, SettingId::DefaultRecapTab);
}

void street_touch(SettingsModalState& s, Street st, int v) {
    s.street_staged = redistribute_street_weights(s.street_staged, st, v);  // staged; not persisted
}
void street_save(SettingsModalState& s) {
    if (s.live == nullptr) {
        return;
    }
    apply_street_weights(s.live->gameplay, s.street_staged);  // applies to the NEXT game
    persist_now(s);
}
void street_reset(SettingsModalState& s) {
    if (s.live != nullptr) {
        s.street_staged = street_weights_of(s.live->gameplay);  // revert to last saved
    }
}

// ----- reset flows -----

void reset_section_fields(SettingsModalState& s, SettingsSection sec) {
    if (s.live == nullptr) {
        return;
    }
    const Settings def{};
    switch (sec) {
        case SettingsSection::Gameplay:
            s.live->gameplay = def.gameplay;
            s.street_staged = street_weights_of(s.live->gameplay);
            break;
        case SettingsSection::Units:
            s.live->units = def.units;
            break;
        case SettingsSection::Display:
            s.live->display = def.display;
            theme::set_theme(s.live->display.active_theme_id);
            break;
        case SettingsSection::Audio:
            s.live->audio = def.audio;
            apply_audio_now(s);
            break;
        case SettingsSection::Recap:
            s.live->recap = def.recap;
            break;
        case SettingsSection::Tomatoes:
            s.live->tomatoes = def.tomatoes;
            break;
        case SettingsSection::Account:
            s.live->account = def.account;
            break;
        case SettingsSection::General:
            s.live->general = def.general;
            break;
        case SettingsSection::Legal:
            break;  // no persistent fields
    }
}

void do_reset_all(SettingsModalState& s) {
    if (s.live == nullptr) {
        return;
    }
    *s.live = Settings{};
    s.street_staged = street_weights_of(s.live->gameplay);
    theme::set_theme(s.live->display.active_theme_id);
    apply_audio_now(s);
    persist_now(s);
}

void do_reset_selected_sections(SettingsModalState& s) {
    for (std::size_t i = 0; i < kSectionCount; ++i) {
        if (s.section_reset_selected[i]) {
            reset_section_fields(s, static_cast<SettingsSection>(i));
        }
    }
    persist_now(s);
}

modal::ConfirmSpec make_reset_all_confirm(SettingsModalState& s) {
    return modal::ConfirmSpec{.body = "Reset ALL settings to their defaults?",
                              .on_yes = [&s] { do_reset_all(s); }};
}
modal::ConfirmSpec make_reset_tomatoes_confirm(SettingsModalState& s) {
    return modal::ConfirmSpec{
        .body = "Reset your Tomatoes wallet? This cannot be undone.",
        .on_yes = [&s] {
            if (s.reset_tomatoes) {
                s.reset_tomatoes();
            }
        }};
}
modal::ConfirmSpec make_reset_section_confirm(SettingsModalState& s) {
    return modal::ConfirmSpec{.body = "Reset the selected sections to their defaults?",
                              .on_yes = [&s] { do_reset_selected_sections(s); }};
}

// ----- sidebar -----

void activate_sidebar(SettingsModalState& s, SettingsSection section) {
    s.search_buf[0] = '\0';  // clicking/entering a sidebar section clears any search
    s.scroll_target = section;
    s.scroll_pending = true;
    backbone::activate_keyboard_mode();
    // Account's first control depends on auth state (logged out -> Sign In, logged in ->
    // View Profile); every other section has a fixed first control.
    const backbone::FocusableId target = section == SettingsSection::Account
                                             ? account_first_focus(s)
                                             : first_control_of(section);
    backbone::snap_focus_to(target);  // jump to the section's first control
}

// ----- sub-modal focus ids -----

constexpr backbone::FocusableId kDocClose = backbone::make_focusable_id("settings.doc.close");
constexpr std::array<backbone::FocusableId, 1> kDocFocusOrder{kDocClose};

constexpr std::array<backbone::FocusableId, kSectionCount> kSrToggle{
    backbone::make_focusable_id("settings.sr.gameplay"),
    backbone::make_focusable_id("settings.sr.units"),
    backbone::make_focusable_id("settings.sr.display"),
    backbone::make_focusable_id("settings.sr.audio"),
    backbone::make_focusable_id("settings.sr.recap"),
    backbone::make_focusable_id("settings.sr.tomatoes"),
    backbone::make_focusable_id("settings.sr.account"),
    backbone::make_focusable_id("settings.sr.general"),
    backbone::make_focusable_id("settings.sr.legal"),
};
constexpr backbone::FocusableId kSrReset = backbone::make_focusable_id("settings.sr.reset");
constexpr backbone::FocusableId kSrClose = backbone::make_focusable_id("settings.sr.close");
constexpr std::array<backbone::FocusableId, kSectionCount + 2> kSrFocusOrder{
    kSrToggle[0], kSrToggle[1], kSrToggle[2], kSrToggle[3], kSrToggle[4], kSrToggle[5],
    kSrToggle[6], kSrToggle[7], kSrToggle[8], kSrReset,     kSrClose};

void open_doc(SettingsModalState& s, SettingsModalState::DocKind kind) {
    s.doc_kind = kind;
    modal::open_modal(modal::kSettingsDocId);
}
void open_reset_section(SettingsModalState& s) {
    s.section_reset_selected.fill(false);
    modal::open_modal(modal::kSettingsSectionResetId);
}

// ----- registries (one per provider; all use the shared FocusRegistry) -----

void populate_main_registry(SettingsModalState& s) {
    if (s.focus_registry == nullptr) {
        return;
    }
    bridge::FocusRegistry& reg = *s.focus_registry;
    reg.clear();

    reg.register_element(kFocusSearch, bridge::FocusableEntry{.is_text_field = true});

    for (std::size_t i = 0; i < kSectionCount; ++i) {
        const SettingsSection section = static_cast<SettingsSection>(i);
        reg.register_element(kSidebarFocus[i], bridge::FocusableEntry{
                                                   .activate = [&s, section] { activate_sidebar(s, section); }});
    }

    // Gameplay — street split (staged; arrows nudge ±1, no persist until Save).
    reg.register_element(kGpStreetPreflopSlider,
                         bridge::FocusableEntry{.adjust = [&s](int d) {
                             street_touch(s, Street::Preflop, s.street_staged.preflop + d);
                         }});
    reg.register_element(kGpStreetPreflopInput,
                         bridge::FocusableEntry{.is_text_field = true, .adjust = [&s](int d) {
                             street_touch(s, Street::Preflop, s.street_staged.preflop + d);
                         }});
    reg.register_element(kGpStreetFlopSlider, bridge::FocusableEntry{.adjust = [&s](int d) {
                             street_touch(s, Street::Flop, s.street_staged.flop + d);
                         }});
    reg.register_element(kGpStreetFlopInput,
                         bridge::FocusableEntry{.is_text_field = true, .adjust = [&s](int d) {
                             street_touch(s, Street::Flop, s.street_staged.flop + d);
                         }});
    reg.register_element(kGpStreetTurnSlider, bridge::FocusableEntry{.adjust = [&s](int d) {
                             street_touch(s, Street::Turn, s.street_staged.turn + d);
                         }});
    reg.register_element(kGpStreetTurnInput,
                         bridge::FocusableEntry{.is_text_field = true, .adjust = [&s](int d) {
                             street_touch(s, Street::Turn, s.street_staged.turn + d);
                         }});
    reg.register_element(kGpStreetRiverSlider, bridge::FocusableEntry{.adjust = [&s](int d) {
                             street_touch(s, Street::River, s.street_staged.river + d);
                         }});
    reg.register_element(kGpStreetRiverInput,
                         bridge::FocusableEntry{.is_text_field = true, .adjust = [&s](int d) {
                             street_touch(s, Street::River, s.street_staged.river + d);
                         }});
    reg.register_element(kGpStreetSave, bridge::FocusableEntry{.activate = [&s] { street_save(s); }});
    reg.register_element(kGpStreetReset, bridge::FocusableEntry{.activate = [&s] { street_reset(s); }});

    reg.register_element(kGpChipDenom, bridge::FocusableEntry{.activate = [&s] {
                             if (s.live == nullptr) return;
                             s.live->gameplay.chip_denomination_mode =
                                 s.live->gameplay.chip_denomination_mode == ChipDenominationMode::Fixed
                                     ? ChipDenominationMode::StakeScaled
                                     : ChipDenominationMode::Fixed;
                             on_setting_change(s, SettingId::ChipDenomination);
                         }});
    reg.register_element(kGpBetSizing, bridge::FocusableEntry{.activate = [&s] {
                             if (s.live == nullptr) return;
                             s.live->gameplay.bet_sizing_engine_enabled =
                                 !s.live->gameplay.bet_sizing_engine_enabled;
                             on_setting_change(s, SettingId::BetSizingEngine);
                         }});
    reg.register_element(kGpDifficultyLow, bridge::FocusableEntry{.adjust = [&s](int d) {
                             if (s.live != nullptr) {
                                 set_diff_low(s, difficulty_display_range(s.live->gameplay).low + d);
                             }
                         }});
    reg.register_element(kGpDifficultyHigh, bridge::FocusableEntry{.adjust = [&s](int d) {
                             if (s.live != nullptr) {
                                 set_diff_high(s, difficulty_display_range(s.live->gameplay).high + d);
                             }
                         }});
    reg.register_element(kGpTimeCustomToggle, bridge::FocusableEntry{.activate = [&s] {
                             if (s.live == nullptr) return;
                             s.live->gameplay.time_pressure_custom_enabled =
                                 !s.live->gameplay.time_pressure_custom_enabled;
                             on_setting_change(s, SettingId::TimePressure);
                         }});
    reg.register_element(kGpTimeCustomInput,
                         bridge::FocusableEntry{.is_text_field = true, .adjust = [&s](int d) {
                             if (s.live != nullptr) {
                                 set_custom_time(s, s.live->gameplay.time_pressure_custom_seconds + d);
                             }
                         }});
    reg.register_element(kGpShowHud, bridge::FocusableEntry{.activate = [&s] {
                             if (s.live == nullptr) return;
                             s.live->gameplay.show_hud = !s.live->gameplay.show_hud;
                             on_setting_change(s, SettingId::ShowHud);
                         }});
    reg.register_element(kGpShowCountdown, bridge::FocusableEntry{.activate = [&s] {
                             if (s.live == nullptr) return;
                             s.live->gameplay.show_countdown = !s.live->gameplay.show_countdown;
                             on_setting_change(s, SettingId::ShowCountdown);
                         }});

    // Units
    reg.register_element(kUnUnitToggle, bridge::FocusableEntry{
                                            .activate = [&s] {
                                                if (s.live != nullptr) {
                                                    set_unit_index(s, s.live->units.cash_mode ? 1 : 0);
                                                }
                                            },
                                            .adjust = [&s](int) {
                                                if (s.live != nullptr) {
                                                    set_unit_index(s, s.live->units.cash_mode ? 1 : 0);
                                                }
                                            }});

    // Display
    reg.register_element(kDiTheme,
                         bridge::FocusableEntry{.activate = [&s] { cycle_theme(s); },
                                                .adjust = [&s](int d) {
                                                    if (s.live != nullptr) {
                                                        set_theme_index(s, s.live->display.active_theme_id + d);
                                                    }
                                                }});
    reg.register_element(kDiReduceMotion, bridge::FocusableEntry{.activate = [&s] {
                             if (s.live == nullptr) return;
                             s.live->display.reduce_motion = !s.live->display.reduce_motion;
                             on_setting_change(s, SettingId::ReduceMotion);
                         }});
    reg.register_element(kDiBackgroundMovement, bridge::FocusableEntry{.activate = [&s] {
                             if (s.live == nullptr) return;
                             s.live->display.background_atmospheric_movement =
                                 !s.live->display.background_atmospheric_movement;
                             on_setting_change(s, SettingId::BackgroundMovement);
                         }});
    reg.register_element(kDiParticleDrift, bridge::FocusableEntry{.activate = [&s] {
                             if (s.live == nullptr) return;
                             s.live->display.particle_drift = !s.live->display.particle_drift;
                             on_setting_change(s, SettingId::ParticleDrift);
                         }});

    // Audio
    reg.register_element(kAuMusicType,
                         bridge::FocusableEntry{.activate = [&s] { cycle_music(s); },
                                                .adjust = [&s](int d) {
                                                    if (s.live != nullptr) {
                                                        set_music_index(
                                                            s,
                                                            static_cast<int>(s.live->audio.current_music_genre) + d);
                                                    }
                                                }});
    reg.register_element(kAuVolumeSlider, bridge::FocusableEntry{.adjust = [&s](int d) {
                             if (s.live != nullptr) {
                                 set_volume(s, s.live->audio.volume + d);
                             }
                         }});
    reg.register_element(kAuVolumeInput,
                         bridge::FocusableEntry{.is_text_field = true, .adjust = [&s](int d) {
                             if (s.live != nullptr) {
                                 set_volume(s, s.live->audio.volume + d);
                             }
                         }});
    reg.register_element(kAuMuteAll, bridge::FocusableEntry{.activate = [&s] {
                             if (s.live == nullptr) return;
                             s.live->audio.mute_all = !s.live->audio.mute_all;
                             on_setting_change(s, SettingId::MuteAll);
                         }});
    reg.register_element(kAuMuteSfx, bridge::FocusableEntry{.activate = [&s] {
                             if (s.live == nullptr) return;
                             s.live->audio.mute_sfx = !s.live->audio.mute_sfx;
                             on_setting_change(s, SettingId::MuteSfx);
                         }});
    reg.register_element(kAuMuteMusic, bridge::FocusableEntry{.activate = [&s] {
                             if (s.live == nullptr) return;
                             s.live->audio.mute_music = !s.live->audio.mute_music;
                             on_setting_change(s, SettingId::MuteMusic);
                         }});

    // Recap
    reg.register_element(kReDealerArrival, bridge::FocusableEntry{.activate = [&s] {
                             if (s.live == nullptr) return;
                             s.live->recap.dealer_arrival_animation =
                                 !s.live->recap.dealer_arrival_animation;
                             on_setting_change(s, SettingId::DealerArrival);
                         }});
    reg.register_element(kReScreenTransitions, bridge::FocusableEntry{.activate = [&s] {
                             if (s.live == nullptr) return;
                             s.live->recap.transitions_enabled = !s.live->recap.transitions_enabled;
                             on_setting_change(s, SettingId::ScreenTransitions);
                         }});
    reg.register_element(
        kReDefaultRecapTab,
        bridge::FocusableEntry{
            .activate = [&s] {
                if (s.live != nullptr) {
                    set_recap_tab_index(
                        s, s.live->recap.default_aggressor_recap_tab == DefaultAggressorRecapTab::Tier1
                               ? 1
                               : 0);
                }
            },
            .adjust = [&s](int d) {
                if (s.live != nullptr) {
                    set_recap_tab_index(s,
                                        static_cast<int>(s.live->recap.default_aggressor_recap_tab) + d);
                }
            }});

    // Tomatoes
    reg.register_element(kToShopVisibility, bridge::FocusableEntry{.activate = [&s] {
                             if (s.live == nullptr) return;
                             s.live->tomatoes.shop_button_visible = !s.live->tomatoes.shop_button_visible;
                             on_setting_change(s, SettingId::ShopVisibility);
                         }});
    reg.register_element(kToResetTomatoes, bridge::FocusableEntry{.activate = [&s] {
                             modal::open_confirm_modal(make_reset_tomatoes_confirm(s));
                         }});
    reg.register_element(kToLeaderboardOptIn, bridge::FocusableEntry{.activate = [&s] {
                             if (s.live == nullptr) return;
                             s.live->tomatoes.leaderboard_opt_in = !s.live->tomatoes.leaderboard_opt_in;
                             on_setting_change(s, SettingId::LeaderboardOptIn);
                         }});

    // Account — logged-out (Sign In / Sign Up) and logged-in (View Profile / Change
    // Password / Sign Out / Delete Account) controls. Both sets are registered; only the
    // active set is in the focus order (build_account_focus_order), so the inactive set is
    // never focused. All route through the boot-wired auth seams (null-guarded). The
    // confirm/modal-opening hooks mirror kToResetTomatoes (open_confirm_modal in an activate
    // closure is the established Part 1 pattern — it pushes a new context, never closes this
    // registry mid-invocation).
    reg.register_element(kAcSignIn, bridge::FocusableEntry{.activate = [&s] {
                             if (s.account != nullptr) {
                                 account_open_sign_in(*s.account);
                             }
                         }});
    reg.register_element(kAcSignUp, bridge::FocusableEntry{.activate = [&s] {
                             if (s.account != nullptr) {
                                 account_open_sign_up(*s.account);
                             }
                         }});
    reg.register_element(kAcViewProfile, bridge::FocusableEntry{.activate = [&s] {
                             s.account_view_profile_open = !s.account_view_profile_open;
                         }});
    reg.register_element(kAcChangePassword, bridge::FocusableEntry{.activate = [&s] {
                             if (s.account != nullptr) {
                                 account_confirm_change_password(*s.account);
                             }
                         }});
    reg.register_element(kAcSignOut, bridge::FocusableEntry{.activate = [&s] {
                             if (s.account != nullptr) {
                                 account_sign_out(*s.account);
                             }
                         }});
    reg.register_element(kAcDeleteAccount, bridge::FocusableEntry{.activate = [&s] {
                             if (s.account != nullptr) {
                                 account_confirm_delete(*s.account);
                             }
                         }});

    // General
    reg.register_element(kGeConfirmLeave, bridge::FocusableEntry{.activate = [&s] {
                             if (s.live == nullptr) return;
                             s.live->general.confirm_before_leaving_site =
                                 !s.live->general.confirm_before_leaving_site;
                             on_setting_change(s, SettingId::ConfirmLeaveSite);
                         }});
    reg.register_element(kGeResetAll, bridge::FocusableEntry{.activate = [&s] {
                             modal::open_confirm_modal(make_reset_all_confirm(s));
                         }});
    reg.register_element(kGeResetSection,
                         bridge::FocusableEntry{.activate = [&s] { open_reset_section(s); }});

    // Legal
    reg.register_element(kLeTerms, bridge::FocusableEntry{.activate = [&s] {
                             open_doc(s, SettingsModalState::DocKind::Terms);
                         }});
    reg.register_element(kLePrivacy, bridge::FocusableEntry{.activate = [&s] {
                             open_doc(s, SettingsModalState::DocKind::Privacy);
                         }});
    reg.register_element(kLeAbout, bridge::FocusableEntry{.activate = [&s] {
                             open_doc(s, SettingsModalState::DocKind::About);
                         }});

    // X close (deferred — the activate closure cannot close + clear the registry it
    // is mid-invocation in; it raises request_close, the dispatch handler closes).
    reg.register_element(modal::kSettingsShellClose,
                         bridge::FocusableEntry{.activate = [&s] { s.request_close = true; }});
}

void populate_section_reset_registry(SettingsModalState& s) {
    if (s.focus_registry == nullptr) {
        return;
    }
    bridge::FocusRegistry& reg = *s.focus_registry;
    reg.clear();
    for (std::size_t i = 0; i < kSectionCount; ++i) {
        reg.register_element(kSrToggle[i], bridge::FocusableEntry{.activate = [&s, i] {
                                 s.section_reset_selected[i] = !s.section_reset_selected[i];
                             }});
    }
    reg.register_element(kSrReset, bridge::FocusableEntry{.activate = [&s] {
                             s.request_section_reset = true;  // close + open confirm (deferred)
                         }});
    reg.register_element(kSrClose, bridge::FocusableEntry{.activate = [&s] { s.request_close = true; }});
}

void populate_doc_registry(SettingsModalState& s) {
    if (s.focus_registry == nullptr) {
        return;
    }
    bridge::FocusRegistry& reg = *s.focus_registry;
    reg.clear();
    reg.register_element(kDocClose, bridge::FocusableEntry{.activate = [&s] { s.request_close = true; }});
}

// Body focus id -> owning SettingId, for the search-filtered Tab traversal (Bug D).
// Catalog/body order; the 43 entries are exactly kSettingsFocusOrder minus the search,
// the 9 sidebar stops, and the X close. A body control renders iff setting_visible(its
// setting) (which implies its section matched, since every keyword blob carries the
// section name), so this table + setting_visible reproduces "is this control on screen."
struct BodyFocusOwner {
    backbone::FocusableId focus;
    SettingId setting;
};
constexpr std::array<BodyFocusOwner, 47> kBodyFocusOwners{{
    {kGpStreetPreflopSlider, SettingId::StreetWeights},
    {kGpStreetPreflopInput, SettingId::StreetWeights},
    {kGpStreetFlopSlider, SettingId::StreetWeights},
    {kGpStreetFlopInput, SettingId::StreetWeights},
    {kGpStreetTurnSlider, SettingId::StreetWeights},
    {kGpStreetTurnInput, SettingId::StreetWeights},
    {kGpStreetRiverSlider, SettingId::StreetWeights},
    {kGpStreetRiverInput, SettingId::StreetWeights},
    {kGpStreetSave, SettingId::StreetWeights},
    {kGpStreetReset, SettingId::StreetWeights},
    {kGpChipDenom, SettingId::ChipDenomination},
    {kGpBetSizing, SettingId::BetSizingEngine},
    {kGpDifficultyLow, SettingId::DifficultyRange},
    {kGpDifficultyHigh, SettingId::DifficultyRange},
    {kGpTimeCustomToggle, SettingId::TimePressure},
    {kGpTimeCustomInput, SettingId::TimePressure},
    {kGpShowHud, SettingId::ShowHud},
    {kGpShowCountdown, SettingId::ShowCountdown},
    {kUnUnitToggle, SettingId::UnitToggle},
    {kDiTheme, SettingId::Theme},
    {kDiReduceMotion, SettingId::ReduceMotion},
    {kDiBackgroundMovement, SettingId::BackgroundMovement},
    {kDiParticleDrift, SettingId::ParticleDrift},
    {kAuMusicType, SettingId::MusicType},
    {kAuVolumeSlider, SettingId::Volume},
    {kAuVolumeInput, SettingId::Volume},
    {kAuMuteAll, SettingId::MuteAll},
    {kAuMuteSfx, SettingId::MuteSfx},
    {kAuMuteMusic, SettingId::MuteMusic},
    {kReDealerArrival, SettingId::DealerArrival},
    {kReScreenTransitions, SettingId::ScreenTransitions},
    {kReDefaultRecapTab, SettingId::DefaultRecapTab},
    {kToShopVisibility, SettingId::ShopVisibility},
    {kToResetTomatoes, SettingId::ResetTomatoes},
    {kToLeaderboardOptIn, SettingId::LeaderboardOptIn},
    {kAcSignIn, SettingId::Account},
    {kAcSignUp, SettingId::Account},
    {kAcViewProfile, SettingId::Account},
    {kAcChangePassword, SettingId::Account},
    {kAcSignOut, SettingId::Account},
    {kAcDeleteAccount, SettingId::Account},
    {kGeConfirmLeave, SettingId::ConfirmLeaveSite},
    {kGeResetAll, SettingId::ResetAll},
    {kGeResetSection, SettingId::ResetSection},
    {kLeTerms, SettingId::TermsOfService},
    {kLePrivacy, SettingId::PrivacyPolicy},
    {kLeAbout, SettingId::AboutCredits},
}};

// True when `id` is a live Tab stop under the active query: the search box and the X
// close are always live (structural); the sidebar is NOT Tab-reachable while a search
// filters the body; a body control is live iff its setting matches. An empty query
// makes every stop live (the caller handles that before reaching here).
[[nodiscard]] bool focus_stop_rendered(std::string_view query, backbone::FocusableId id) noexcept {
    if (id == kFocusSearch || id == modal::kSettingsShellClose) {
        return true;
    }
    for (const backbone::FocusableId sb : kSidebarFocus) {
        if (id == sb) {
            return false;
        }
    }
    for (const BodyFocusOwner& o : kBodyFocusOwners) {
        if (o.focus == id) {
            return setting_visible(o.setting, query);
        }
    }
    return false;  // unknown id -> not a stop during search
}

// One dispatch for all three providers (they share the registry + the deferral flags).
bool dispatch_settings_key(SettingsModalState& s, const backbone::KeyEvent& e) {
    if (e.type != backbone::KeyEventType::KeyDown || s.focus_registry == nullptr) {
        return false;
    }
    // Search-filtered Tab traversal (Bug D): while a query hides controls in the MAIN
    // Settings modal, Tab/Shift-Tab must skip the unrendered stops so focus never lands
    // on an off-screen control. With no query — or in a sub-modal (no search box) — this
    // returns false so the backbone focus-nav handler runs its normal full-list advance.
    // Gated to the main modal so a sub-modal's small context is never walked against
    // kSettingsFocusOrder.
    if (e.code == backbone::KeyCode::Tab) {
        const std::optional<backbone::ModalId> top = backbone::current_modal_id();
        const std::string_view query{s.search_buf.data()};
        if (!top.has_value() || *top != modal::kSettingsModalId || query.empty()) {
            return false;
        }
        const bool reverse = backbone::has_mod(e.mods, backbone::ModMask::Shift);
        // Advance, skipping stops not rendered under the filter. Bounded by the active list
        // length (account auth state can change the count); search + X are always live, so
        // the loop always lands on a real stop.
        const std::size_t bound =
            s.active_focus_count != 0 ? s.active_focus_count : kSettingsFocusOrder.size();
        for (std::size_t step = 0; step < bound; ++step) {
            backbone::advance_focus(reverse);
            if (focus_stop_rendered(query, backbone::get_focused_element())) {
                break;
            }
        }
        return true;  // Tab consumed here; the backbone focus-nav handler must not re-advance
    }
    const bridge::FocusRegistry& reg = *s.focus_registry;
    const backbone::FocusableId focused = bridge::active_focus_or_none();
    if (reg.is_text_field(focused) && (e.code == backbone::KeyCode::ArrowLeft ||
                                       e.code == backbone::KeyCode::ArrowRight)) {
        return false;  // text-cursor keys belong to ImGui's InputText
    }
    const bool handled = bridge::dispatch_focus_key(reg, focused, e.code);

    // Deferred closes, performed AFTER dispatch_focus_key returns (no registry closure
    // on the stack when close_modal clears the registry).
    if (s.request_section_reset) {
        s.request_section_reset = false;
        modal::close_modal();                                       // close the section-select modal
        modal::open_confirm_modal(make_reset_section_confirm(s));   // then the standard confirm
    } else if (s.request_close) {
        s.request_close = false;
        modal::close_modal();
    }
    return handled;
}

// ----- section renders -----

void render_gameplay(SettingsModalState& s, const bridge::FocusReconcile& rec, ImU32 ring,
                     std::string_view q) {
    if (!begin_section(s, SettingsSection::Gameplay, q) || s.live == nullptr) {
        return;
    }
    GameplaySettings& g = s.live->gameplay;

    if (setting_visible(SettingId::StreetWeights, q)) {
        ImGui::TextUnformatted("Street split weights (sum 100; applies next game)");
        const std::array<std::pair<const char*, Street>, 4> rows{{
            {"Pre-flop", Street::Preflop},
            {"Flop", Street::Flop},
            {"Turn", Street::Turn},
            {"River", Street::River},
        }};
        const std::array<int, 4> defs{15, 35, 30, 20};
        const std::array<backbone::FocusableId, 4> sliders{kGpStreetPreflopSlider, kGpStreetFlopSlider,
                                                           kGpStreetTurnSlider, kGpStreetRiverSlider};
        const std::array<backbone::FocusableId, 4> inputs{kGpStreetPreflopInput, kGpStreetFlopInput,
                                                          kGpStreetTurnInput, kGpStreetRiverInput};
        std::array<char, 4>* bufs[4] = {&s.street_buf_preflop, &s.street_buf_flop, &s.street_buf_turn,
                                        &s.street_buf_river};
        const std::array<std::uint8_t, 4> vals{s.street_staged.preflop, s.street_staged.flop,
                                               s.street_staged.turn, s.street_staged.river};
        const std::array<const char*, 4> sids{"##sp_pf", "##sp_fl", "##sp_tn", "##sp_rv"};
        const std::array<const char*, 4> iids{"##si_pf", "##si_fl", "##si_tn", "##si_rv"};
        for (std::size_t i = 0; i < 4; ++i) {
            ImGui::TextUnformatted(rows[i].first);
            int v = vals[i];
            if (widget_slider(sliders[i], sids[i], v, 0, 100, defs[i], ring, s.scroll_follow_focus)) {
                street_touch(s, rows[i].second, v);
            }
            format_int(*bufs[i], vals[i]);
            if (widget_int_input(inputs[i], iids[i], bufs[i]->data(), bufs[i]->size(), rec, 60.0f, ring,
                                 s.scroll_follow_focus)) {
                street_touch(s, rows[i].second, parse_clamped_int(std::string_view{bufs[i]->data()}, 0, 100));
            }
        }
        if (widget_button(kGpStreetSave, "Save weights", ring, s.scroll_follow_focus)) {
            street_save(s);
        }
        ImGui::SameLine();
        if (widget_button(kGpStreetReset, "Reset weights", ring, s.scroll_follow_focus)) {
            street_reset(s);
        }
        ImGui::Spacing();
    }

    if (setting_visible(SettingId::ChipDenomination, q)) {
        bool fixed = g.chip_denomination_mode == ChipDenominationMode::Fixed;
        if (widget_checkbox(kGpChipDenom, "Fixed chip denominations (off = stake-scaled)", fixed, ring,
                            s.scroll_follow_focus)) {
            g.chip_denomination_mode = fixed ? ChipDenominationMode::Fixed : ChipDenominationMode::StakeScaled;
            on_setting_change(s, SettingId::ChipDenomination);
        }
    }
    if (setting_visible(SettingId::BetSizingEngine, q)) {
        if (widget_checkbox(kGpBetSizing, "Bet sizing engine (Aggressor multi-tier)",
                            g.bet_sizing_engine_enabled, ring, s.scroll_follow_focus)) {
            on_setting_change(s, SettingId::BetSizingEngine);
        }
    }
    if (setting_visible(SettingId::DifficultyRange, q)) {
        const DifficultyDisplay dr = difficulty_display_range(g);
        ImGui::TextUnformatted("Difficulty range (opponent fold tendency, %)");
        int lo = dr.low;
        if (widget_slider(kGpDifficultyLow, "##diff_lo", lo, 0, 100, 20, ring, s.scroll_follow_focus)) {
            set_diff_low(s, lo);
        }
        int hi = dr.high;
        if (widget_slider(kGpDifficultyHigh, "##diff_hi", hi, 0, 100, 80, ring, s.scroll_follow_focus)) {
            set_diff_high(s, hi);
        }
    }
    if (setting_visible(SettingId::TimePressure, q)) {
        if (widget_checkbox(kGpTimeCustomToggle, "Custom flat time pressure",
                            g.time_pressure_custom_enabled, ring, s.scroll_follow_focus)) {
            on_setting_change(s, SettingId::TimePressure);
        }
        ImGui::TextUnformatted("Custom seconds (1-300)");
        ImGui::SameLine();
        format_int(s.custom_time_buf, static_cast<int>(g.time_pressure_custom_seconds));
        if (widget_int_input(kGpTimeCustomInput, "##cust_time", s.custom_time_buf.data(),
                             s.custom_time_buf.size(), rec, 70.0f, ring, s.scroll_follow_focus)) {
            set_custom_time(s, parse_clamped_int(std::string_view{s.custom_time_buf.data()}, 1, 300));
        }
    }
    if (setting_visible(SettingId::ShowHud, q)) {
        if (widget_checkbox(kGpShowHud, "Show HUD numbers", g.show_hud, ring, s.scroll_follow_focus)) {
            on_setting_change(s, SettingId::ShowHud);
        }
    }
    if (setting_visible(SettingId::ShowCountdown, q)) {
        if (widget_checkbox(kGpShowCountdown, "Show countdown timer", g.show_countdown, ring,
                            s.scroll_follow_focus)) {
            on_setting_change(s, SettingId::ShowCountdown);
        }
    }
    ImGui::Spacing();
}

void render_units(SettingsModalState& s, ImU32 ring, std::string_view q) {
    if (!begin_section(s, SettingsSection::Units, q) || s.live == nullptr) {
        return;
    }
    if (setting_visible(SettingId::UnitToggle, q)) {
        ImGui::TextUnformatted("Units");
        int idx = s.live->units.cash_mode ? 0 : 1;
        if (widget_combo(kUnUnitToggle, "##units", idx, kUnitOptions, ring, s.scroll_follow_focus)) {
            set_unit_index(s, idx);
        }
    }
    ImGui::Spacing();
}

void render_display(SettingsModalState& s, ImU32 ring, std::string_view q) {
    if (!begin_section(s, SettingsSection::Display, q) || s.live == nullptr) {
        return;
    }
    DisplaySettings& d = s.live->display;
    if (setting_visible(SettingId::Theme, q)) {
        ImGui::TextUnformatted("Color theme");
        int idx = static_cast<int>(d.active_theme_id);
        if (widget_combo(kDiTheme, "##theme", idx, kThemeOptions, ring, s.scroll_follow_focus)) {
            set_theme_index(s, idx);
        }
    }
    if (setting_visible(SettingId::ReduceMotion, q)) {
        if (widget_checkbox(kDiReduceMotion, "Reduce motion", d.reduce_motion, ring, s.scroll_follow_focus)) {
            on_setting_change(s, SettingId::ReduceMotion);
        }
    }
    if (setting_visible(SettingId::BackgroundMovement, q)) {
        if (widget_checkbox(kDiBackgroundMovement, "Background atmospheric movement",
                            d.background_atmospheric_movement, ring, s.scroll_follow_focus)) {
            on_setting_change(s, SettingId::BackgroundMovement);
        }
    }
    if (setting_visible(SettingId::ParticleDrift, q)) {
        if (widget_checkbox(kDiParticleDrift, "Particle drift", d.particle_drift, ring,
                            s.scroll_follow_focus)) {
            on_setting_change(s, SettingId::ParticleDrift);
        }
    }
    ImGui::Spacing();
}

void render_audio(SettingsModalState& s, const bridge::FocusReconcile& rec, ImU32 ring,
                  std::string_view q) {
    if (!begin_section(s, SettingsSection::Audio, q) || s.live == nullptr) {
        return;
    }
    AudioSettings& a = s.live->audio;
    if (setting_visible(SettingId::MusicType, q)) {
        ImGui::TextUnformatted("Music type");
        int idx = static_cast<int>(a.current_music_genre);
        if (widget_combo(kAuMusicType, "##music", idx, kMusicOptions, ring, s.scroll_follow_focus)) {
            set_music_index(s, idx);
        }
    }
    if (setting_visible(SettingId::Volume, q)) {
        ImGui::TextUnformatted("Volume");
        int vol = static_cast<int>(a.volume);
        if (widget_slider(kAuVolumeSlider, "##vol_slider", vol, 0, 100, 50, ring, s.scroll_follow_focus)) {
            set_volume(s, vol);
        }
        format_int(s.volume_buf, static_cast<int>(a.volume));
        if (widget_int_input(kAuVolumeInput, "##vol_input", s.volume_buf.data(), s.volume_buf.size(),
                             rec, 70.0f, ring, s.scroll_follow_focus)) {
            set_volume(s, parse_clamped_int(std::string_view{s.volume_buf.data()}, 0, 100));
        }
    }
    if (setting_visible(SettingId::MuteAll, q)) {
        if (widget_button(kAuMuteAll, a.mute_all ? "Unmute all" : "Mute all", ring, s.scroll_follow_focus)) {
            a.mute_all = !a.mute_all;
            on_setting_change(s, SettingId::MuteAll);
        }
    }
    if (setting_visible(SettingId::MuteSfx, q)) {
        if (widget_checkbox(kAuMuteSfx, "Mute sound effects", a.mute_sfx, ring, s.scroll_follow_focus)) {
            on_setting_change(s, SettingId::MuteSfx);
        }
    }
    if (setting_visible(SettingId::MuteMusic, q)) {
        if (widget_checkbox(kAuMuteMusic, "Mute music", a.mute_music, ring, s.scroll_follow_focus)) {
            on_setting_change(s, SettingId::MuteMusic);
        }
    }
    ImGui::Spacing();
}

void render_recap(SettingsModalState& s, ImU32 ring, std::string_view q) {
    if (!begin_section(s, SettingsSection::Recap, q) || s.live == nullptr) {
        return;
    }
    RecapSettings& r = s.live->recap;
    if (setting_visible(SettingId::DealerArrival, q)) {
        if (widget_checkbox(kReDealerArrival, "Dealer arrival animation", r.dealer_arrival_animation, ring,
                            s.scroll_follow_focus)) {
            on_setting_change(s, SettingId::DealerArrival);
        }
    }
    if (setting_visible(SettingId::ScreenTransitions, q)) {
        if (widget_checkbox(kReScreenTransitions, "Screen transitions", r.transitions_enabled, ring,
                            s.scroll_follow_focus)) {
            on_setting_change(s, SettingId::ScreenTransitions);
        }
    }
    if (setting_visible(SettingId::DefaultRecapTab, q)) {
        ImGui::TextUnformatted("Default Aggressor recap tab");
        int idx = static_cast<int>(r.default_aggressor_recap_tab);
        if (widget_combo(kReDefaultRecapTab, "##recap_tab", idx, kRecapTabOptions, ring,
                         s.scroll_follow_focus)) {
            set_recap_tab_index(s, idx);
        }
    }
    ImGui::Spacing();
}

void render_tomatoes(SettingsModalState& s, ImU32 ring, std::string_view q) {
    if (!begin_section(s, SettingsSection::Tomatoes, q) || s.live == nullptr) {
        return;
    }
    if (setting_visible(SettingId::ShopVisibility, q)) {
        if (widget_checkbox(kToShopVisibility, "Show Shop button", s.live->tomatoes.shop_button_visible,
                            ring, s.scroll_follow_focus)) {
            on_setting_change(s, SettingId::ShopVisibility);
        }
    }
    if (setting_visible(SettingId::ResetTomatoes, q)) {
        if (widget_button(kToResetTomatoes, "Reset tomatoes", ring, s.scroll_follow_focus)) {
            modal::open_confirm_modal(make_reset_tomatoes_confirm(s));
        }
    }
    if (setting_visible(SettingId::LeaderboardOptIn, q)) {
        if (widget_checkbox(kToLeaderboardOptIn, "Opt in to the leaderboard",
                            s.live->tomatoes.leaderboard_opt_in, ring, s.scroll_follow_focus)) {
            on_setting_change(s, SettingId::LeaderboardOptIn);
        }
    }
    ImGui::Spacing();
}

void render_account(SettingsModalState& s, ImU32 ring, std::string_view q) {
    if (!begin_section(s, SettingsSection::Account, q)) {
        return;
    }
    if (s.account == nullptr) {
        // Unwired (native tests / before install): inert logged-out shell.
        ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::TextSecondary));
        ImGui::TextWrapped("Account is not available.");
        ImGui::PopStyleColor();
        widget_button(kAcSignIn, "Sign In", ring, s.scroll_follow_focus);
        ImGui::SameLine();
        widget_button(kAcSignUp, "Sign Up", ring, s.scroll_follow_focus);
        ImGui::Spacing();
        return;
    }

    const AccountSnapshot acc = account_snapshot(*s.account);
    if (!acc.is_authenticated) {
        // Logged out: the Sign In / Sign Up pair, each health-check-gated (a failed check
        // triggers the outage banner instead of opening the modal).
        if (widget_button(kAcSignIn, "Sign In", ring, s.scroll_follow_focus)) {
            account_open_sign_in(*s.account);
        }
        ImGui::SameLine();
        if (widget_button(kAcSignUp, "Sign Up", ring, s.scroll_follow_focus)) {
            account_open_sign_up(*s.account);
        }
        ImGui::Spacing();
        return;
    }

    // Logged in: identity + account actions.
    ImGui::TextUnformatted(acc.display_name.c_str());  // username (text_primary)
    ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::TextSecondary));
    const std::string masked = mask_email(acc.email);
    ImGui::TextUnformatted(masked.c_str());  // partially masked email (text_secondary)
    ImGui::PopStyleColor();
    ImGui::Spacing();

    if (widget_button(kAcViewProfile, s.account_view_profile_open ? "Hide Profile" : "View Profile",
                      ring, s.scroll_follow_focus)) {
        s.account_view_profile_open = !s.account_view_profile_open;
    }
    if (s.account_view_profile_open) {
        // The Profile view is one of the three sanctioned tomato-display surfaces.
        const WalletSnapshot w = account_wallet(*s.account);
        ImGui::Indent();
        ImGui::Text("Spendable Tomatoes: %llu", static_cast<unsigned long long>(w.spendable));
        ImGui::Text("Lifetime Tomatoes: %llu", static_cast<unsigned long long>(w.lifetime));
        ImGui::Unindent();
    }
    if (widget_button(kAcChangePassword, "Change Password", ring, s.scroll_follow_focus)) {
        account_confirm_change_password(*s.account);
    }
    if (widget_button(kAcSignOut, "Sign Out", ring, s.scroll_follow_focus)) {
        account_sign_out(*s.account);
    }
    ImGui::PushStyleColor(ImGuiCol_Button, theme::get_color(theme::ColorToken::StateFail));
    const bool del = widget_button(kAcDeleteAccount, "Delete Account", ring, s.scroll_follow_focus);
    ImGui::PopStyleColor();
    if (del) {
        account_confirm_delete(*s.account);
    }
    ImGui::Spacing();
}

void render_general(SettingsModalState& s, ImU32 ring, std::string_view q) {
    if (!begin_section(s, SettingsSection::General, q) || s.live == nullptr) {
        return;
    }
    if (setting_visible(SettingId::ConfirmLeaveSite, q)) {
        if (widget_checkbox(kGeConfirmLeave, "Confirm before leaving site (active scenario)",
                            s.live->general.confirm_before_leaving_site, ring, s.scroll_follow_focus)) {
            on_setting_change(s, SettingId::ConfirmLeaveSite);
        }
    }
    if (setting_visible(SettingId::ResetAll, q)) {
        if (widget_button(kGeResetAll, "Reset all settings", ring, s.scroll_follow_focus)) {
            modal::open_confirm_modal(make_reset_all_confirm(s));
        }
    }
    if (setting_visible(SettingId::ResetSection, q)) {
        if (widget_button(kGeResetSection, "Reset section...", ring, s.scroll_follow_focus)) {
            open_reset_section(s);
        }
    }
    ImGui::Spacing();
}

void render_legal(SettingsModalState& s, ImU32 ring, std::string_view q) {
    if (!begin_section(s, SettingsSection::Legal, q)) {
        return;
    }
    if (setting_visible(SettingId::TermsOfService, q)) {
        if (widget_button(kLeTerms, "Terms of Service", ring, s.scroll_follow_focus)) {
            open_doc(s, SettingsModalState::DocKind::Terms);
        }
    }
    if (setting_visible(SettingId::PrivacyPolicy, q)) {
        if (widget_button(kLePrivacy, "Privacy Policy", ring, s.scroll_follow_focus)) {
            open_doc(s, SettingsModalState::DocKind::Privacy);
        }
    }
    if (setting_visible(SettingId::AboutCredits, q)) {
        if (widget_button(kLeAbout, "About / Credits", ring, s.scroll_follow_focus)) {
            open_doc(s, SettingsModalState::DocKind::About);
        }
    }
    ImGui::Spacing();
}

// ----- sub-modal bodies -----

void render_section_reset_body(SettingsModalState& s) {
    const ImU32 ring = token_u32(theme::ColorToken::BorderFocus);
    ImGui::TextWrapped("Select the sections to reset to their defaults, then Reset.");
    ImGui::Spacing();
    for (std::size_t i = 0; i < kSectionCount; ++i) {
        const bool selected = s.section_reset_selected[i];
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, theme::get_color(theme::ColorToken::StateFail));
        }
        const std::string_view label = section_label(static_cast<SettingsSection>(i));
        const std::string btn{label};
        if (widget_button(kSrToggle[i], btn.c_str(), ring)) {
            s.section_reset_selected[i] = !selected;
        }
        if (selected) {
            ImGui::PopStyleColor();
        }
    }
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, theme::get_color(theme::ColorToken::StateFail));
    const bool reset_clicked = widget_button(kSrReset, "Reset selected", ring);
    ImGui::PopStyleColor();
    if (reset_clicked) {
        // Mouse path: close this modal then open the standard confirm (safe in render —
        // no registry closure is on the stack here).
        modal::close_modal();
        modal::open_confirm_modal(make_reset_section_confirm(s));
    }
}

void render_doc_body(SettingsModalState& s) {
    switch (s.doc_kind) {
        case SettingsModalState::DocKind::About:
            ImGui::TextUnformatted(kAppVersion);
            ImGui::Separator();
            ImGui::TextUnformatted("Library attributions:");
            for (const Credit& c : kCredits) {
                ImGui::BulletText("%s — %s", c.name, c.note);
            }
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::TextSecondary));
            ImGui::TextWrapped("Music: CC-BY tracks — credits pending from the audio pipeline.");
            ImGui::PopStyleColor();
            break;
        case SettingsModalState::DocKind::Terms:
            ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::TextSecondary));
            ImGui::TextWrapped("Terms of Service — placeholder. The final legal copy is pending "
                               "(supplied separately).");
            ImGui::PopStyleColor();
            break;
        case SettingsModalState::DocKind::Privacy:
            ImGui::PushStyleColor(ImGuiCol_Text, theme::get_color(theme::ColorToken::TextSecondary));
            ImGui::TextWrapped("Privacy Policy — placeholder. The final legal copy is pending "
                               "(supplied separately).");
            ImGui::PopStyleColor();
            break;
    }
}

// ----- provider builders -----

modal::ModalContentProvider make_main_provider(SettingsModalState& s) {
    modal::ModalContentProvider p{};
    p.header_icon = assets::AssetId::IconSettings;
    p.header_name = "Settings";
    p.close_focus = modal::kSettingsShellClose;
    p.render_body = [&s] { render_settings_modal(s); };
    // Built lazily here because push_modal_focus calls focus_list() at open BEFORE on_open;
    // the account block reflects the current auth state at that moment.
    p.focus_list = [&s] {
        build_account_focus_order(s);
        return std::span<const backbone::FocusableId>(s.active_focus_order.data(),
                                                      s.active_focus_count);
    };
    p.initial_focus = kFocusSearch;
    p.dispatch = [&s](const backbone::KeyEvent& e) { return dispatch_settings_key(s, e); };
    p.on_open = [&s] {
        if (s.live != nullptr) {
            s.street_staged = street_weights_of(s.live->gameplay);
        }
        s.last_synced_focus = backbone::kNoFocus;
        populate_main_registry(s);
    };
    p.on_close = [&s] {
        if (s.focus_registry != nullptr) {
            s.focus_registry->clear();
        }
    };
    return p;
}

modal::ModalContentProvider make_section_reset_provider(SettingsModalState& s) {
    modal::ModalContentProvider p{};
    p.header_icon = assets::AssetId::IconSettings;
    p.header_name = "Reset sections";
    p.close_focus = kSrClose;
    p.render_body = [&s] { render_section_reset_body(s); };
    p.focus_list = [] { return std::span<const backbone::FocusableId>(kSrFocusOrder); };
    p.initial_focus = kSrToggle[0];
    p.dispatch = [&s](const backbone::KeyEvent& e) { return dispatch_settings_key(s, e); };
    p.on_open = [&s] {
        s.last_synced_focus = backbone::kNoFocus;
        populate_section_reset_registry(s);
    };
    p.on_close = [&s] { populate_main_registry(s); };  // restore the Settings registry
    return p;
}

modal::ModalContentProvider make_doc_provider(SettingsModalState& s) {
    modal::ModalContentProvider p{};
    p.header_icon = assets::AssetId::IconSettings;
    p.header_name = "Legal";
    p.close_focus = kDocClose;
    p.render_body = [&s] { render_doc_body(s); };
    p.focus_list = [] { return std::span<const backbone::FocusableId>(kDocFocusOrder); };
    p.initial_focus = kDocClose;
    p.dispatch = [&s](const backbone::KeyEvent& e) { return dispatch_settings_key(s, e); };
    p.on_open = [&s] {
        s.last_synced_focus = backbone::kNoFocus;
        populate_doc_registry(s);
    };
    p.on_close = [&s] { populate_main_registry(s); };  // restore the Settings registry
    return p;
}

}  // namespace

// ----- exports -----

backbone::FocusableId first_control_of(SettingsSection section) noexcept {
    switch (section) {
        case SettingsSection::Gameplay:
            return kGpStreetPreflopSlider;
        case SettingsSection::Units:
            return kUnUnitToggle;
        case SettingsSection::Display:
            return kDiTheme;
        case SettingsSection::Audio:
            return kAuMusicType;
        case SettingsSection::Recap:
            return kReDealerArrival;
        case SettingsSection::Tomatoes:
            return kToShopVisibility;
        case SettingsSection::Account:
            return kAcSignIn;
        case SettingsSection::General:
            return kGeConfirmLeave;
        case SettingsSection::Legal:
            return kLeTerms;
    }
    return kFocusSearch;
}

void on_setting_change(SettingsModalState& state, SettingId id) {
    if (state.live == nullptr) {
        return;
    }
    switch (id) {
        case SettingId::Theme:
            theme::set_theme(state.live->display.active_theme_id);  // apply immediately (Z06)
            break;
        case SettingId::MusicType:
        case SettingId::Volume:
        case SettingId::MuteAll:
        case SettingId::MuteSfx:
        case SettingId::MuteMusic:
            apply_audio_now(state);  // apply immediately (Z03)
            break;
        default:
            break;  // presentation / gameplay: read at the consumer's read point / next spawn
    }
    persist_now(state);  // autosave on every change (Z04)
}

void render_settings_modal(SettingsModalState& state) {
    const ImU32 ring = token_u32(theme::ColorToken::BorderFocus);
    const bridge::FocusReconcile rec =
        state.focus_registry != nullptr
            ? bridge::begin_focus_reconcile(*state.focus_registry, state.last_synced_focus)
            : bridge::FocusReconcile{};

    // Scroll-follow-focus (Bug C): reveal the focused body control only on the frame
    // focus MOVED (last_synced_focus still holds the previous frame's focus here, before
    // the end-of-render update), and never while a sidebar jump is pending (that owns the
    // scroll this frame). Minimal-reveal keeps it from yanking the body to a non-focused
    // location or fighting a manual scroll; each control's widget consumes it by id.
    //
    // The follow drives NON-text controls ONLY. A text field, when focused, calls
    // SetKeyboardFocusHere (grab_keyboard_if_target) which arms ImGui's OWN nav scroll-
    // into-view; driving our SetScrollY on that same frame makes us a second scroll
    // authority that ImGui's deferred (next-frame) nav scroll then fights — the override
    // the trace caught. Every text field is adjacent to a non-text sibling (slider /
    // toggle) the follow already centers, so the field stays in view without us touching
    // the scroll, and ImGui's reliable nav scroll only nudges it if partially clipped.
    const backbone::FocusableId focused_now = bridge::active_focus_or_none();
    const bool focus_is_text =
        state.focus_registry != nullptr && state.focus_registry->is_text_field(focused_now);
    state.scroll_follow_focus =
        (focused_now != state.last_synced_focus && !state.scroll_pending && !focus_is_text)
            ? focused_now
            : backbone::kNoFocus;
    // While a non-text control holds focus, evict the keyboard/nav target a prior
    // SetKeyboardFocusHere left on the text field we tabbed off of, so its lingering
    // deferred scroll can never override the follow. Skipped on text frames so an active
    // input keeps its typing target (Bug A). See clear_imgui_keyboard_nav.
    if (focused_now != backbone::kNoFocus && !focus_is_text) {
        clear_imgui_keyboard_nav();
    }

    // Tutorial lock: interactive controls render at 40% + clicks suppressed (no-op
    // until Z14). The shell already drew the lock banner.
    modal::modal_begin_locked_controls();

    // Search input (full width), FocusRegistry-registered as a text field.
    ImGui::TextUnformatted("Search");
    ImGui::SameLine();
    bridge::grab_keyboard_if_target(rec, kFocusSearch);
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##settings_search", state.search_buf.data(), state.search_buf.size());
    if (ImGui::IsItemClicked()) {
        focus_on_click(kFocusSearch);
    }
    bridge::draw_focus_ring(kFocusSearch, ring);
    ImGui::Separator();

    const std::string_view query{state.search_buf.data()};

    const float sidebar_w = ImGui::GetContentRegionAvail().x * 0.26f;
    ImGui::BeginChild("##settings_sidebar", ImVec2{sidebar_w, 0.0f}, true);
    for (std::size_t i = 0; i < kSectionCount; ++i) {
        const SettingsSection section = static_cast<SettingsSection>(i);
        const std::string_view label = section_label(section);
        const std::string btn{label};
        if (widget_button(kSidebarFocus[i], btn.c_str(), ring)) {
            activate_sidebar(state, section);  // clears search + scrolls + jumps focus
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##settings_body", ImVec2{0.0f, 0.0f}, true);
    if (!search_is_empty_result(query)) {  // empty-result => blank body (no controls)
        render_gameplay(state, rec, ring, query);
        render_units(state, ring, query);
        render_display(state, ring, query);
        render_audio(state, rec, ring, query);
        render_recap(state, ring, query);
        render_tomatoes(state, ring, query);
        render_account(state, ring, query);
        render_general(state, ring, query);
        render_legal(state, ring, query);
    }
    ImGui::EndChild();

    // Strict one-shot: a sidebar jump is consumed by its target section (begin_section)
    // or dropped here if that section was filtered out — never carried to a later frame
    // where it would re-pin the scroll as the user tabs (Bug C's stale-yank guard).
    state.scroll_pending = false;

    modal::modal_end_locked_controls();

    state.last_synced_focus = bridge::active_focus_or_none();
}

void install_settings_content(SettingsModalState& state) {
    // Drive the modal's own registry, not the shared app-root one boot wired in. This
    // is what keeps a Settings open/close from clobbering the Game screen's reconcile
    // entries: the shared registry (with the Game's math boxes + cluster stops) is left
    // untouched, so a Settings close restores the Game context exactly as a Shop/Help
    // close does. The pointer stays non-null in the wired app (the populate/dispatch
    // guard), and null in native tests that never install.
    state.focus_registry = &state.own_registry;
    modal::register_modal_content(modal::kSettingsModalId, make_main_provider(state));
    modal::register_modal_content(modal::kSettingsSectionResetId, make_section_reset_provider(state));
    modal::register_modal_content(modal::kSettingsDocId, make_doc_provider(state));
}

// ----- settings.hpp validate() (declared in the sealed Phase 0 header; defined here,
//       a Zone 12 owned TU — defining a declared free function does not modify the
//       header). Pure logic; mirrors the SettingsValidationResult enum's invariants.
SettingsValidationResult validate(const Settings& s) noexcept {
    const GameplaySettings& g = s.gameplay;
    if (g.street_weight_preflop + g.street_weight_flop + g.street_weight_turn + g.street_weight_river !=
        100) {
        return SettingsValidationResult::InvalidStreetWeights;
    }
    if (g.custom_aggressor_weight + g.custom_caller_weight != 100) {
        return SettingsValidationResult::InvalidCustomModeWeights;
    }
    if (g.side_pot_frequency < 0.0f || g.side_pot_frequency > 1.0f) {
        return SettingsValidationResult::InvalidSidePotFrequency;
    }
    if (g.difficulty_min < 0.0f || g.difficulty_max > 1.0f || g.difficulty_min > g.difficulty_max) {
        return SettingsValidationResult::InvalidDifficultyRange;
    }
    if (g.time_pressure_custom_enabled &&
        (g.time_pressure_custom_seconds < 1 || g.time_pressure_custom_seconds > 300)) {
        return SettingsValidationResult::InvalidTimePressureCustom;
    }
    if (s.audio.volume > 100) {
        return SettingsValidationResult::InvalidVolumeValue;
    }
    if (s.account.display_name_override.size() > kMaxDisplayNameOverrideLength) {
        return SettingsValidationResult::InvalidDisplayNameOverride;
    }
    return SettingsValidationResult::Ok;
}

}  // namespace poker_trainer::settings
