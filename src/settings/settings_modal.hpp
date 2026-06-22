#pragma once

// Zone 12 — the Settings modal CONTENT (the body rendered into Zone 11's cluster-modal
// shell via the generic content-provider seam). This header declares the per-open view
// state, the full continuous focus traversal (search -> sidebar -> body controls -> X),
// and the three exports: render_settings_modal (the body), on_setting_change (apply +
// persist after a mutation), and install_settings_content (boot wiring of the provider).
//
// The pure cores live in settings_logic.hpp / search.hpp; the render glue (the only
// ImGui in this zone) lives in settings_modal.cpp + sections.

#include "settings/search.hpp"
#include "settings/settings.hpp"
#include "settings/settings_logic.hpp"

#include "modal/modals.hpp"  // kSettingsShellClose (the shell's X-close focus id)

#include "backbone/focus_manager.hpp"

#include "bridge/focus_registry.hpp"  // owned per-modal FocusRegistry (see own_registry)

#include <array>
#include <cstddef>
#include <functional>

namespace poker_trainer::settings {

// ----- Focus ids, in the continuous traversal order -----
// search -> 9 sidebar sections -> every body control (document order) -> X close,
// wrapping from X back to search (Notes — Settings Page Specification).

inline constexpr backbone::FocusableId kFocusSearch =
    backbone::make_focusable_id("settings.search");

inline constexpr std::array<backbone::FocusableId, kSectionCount> kSidebarFocus{
    backbone::make_focusable_id("settings.sidebar.gameplay"),
    backbone::make_focusable_id("settings.sidebar.units"),
    backbone::make_focusable_id("settings.sidebar.display"),
    backbone::make_focusable_id("settings.sidebar.audio"),
    backbone::make_focusable_id("settings.sidebar.recap"),
    backbone::make_focusable_id("settings.sidebar.tomatoes"),
    backbone::make_focusable_id("settings.sidebar.account"),
    backbone::make_focusable_id("settings.sidebar.general"),
    backbone::make_focusable_id("settings.sidebar.legal"),
};

// Gameplay
inline constexpr backbone::FocusableId kGpStreetPreflopSlider = backbone::make_focusable_id("settings.gp.preflop_slider");
inline constexpr backbone::FocusableId kGpStreetPreflopInput = backbone::make_focusable_id("settings.gp.preflop_input");
inline constexpr backbone::FocusableId kGpStreetFlopSlider = backbone::make_focusable_id("settings.gp.flop_slider");
inline constexpr backbone::FocusableId kGpStreetFlopInput = backbone::make_focusable_id("settings.gp.flop_input");
inline constexpr backbone::FocusableId kGpStreetTurnSlider = backbone::make_focusable_id("settings.gp.turn_slider");
inline constexpr backbone::FocusableId kGpStreetTurnInput = backbone::make_focusable_id("settings.gp.turn_input");
inline constexpr backbone::FocusableId kGpStreetRiverSlider = backbone::make_focusable_id("settings.gp.river_slider");
inline constexpr backbone::FocusableId kGpStreetRiverInput = backbone::make_focusable_id("settings.gp.river_input");
inline constexpr backbone::FocusableId kGpStreetSave = backbone::make_focusable_id("settings.gp.street_save");
inline constexpr backbone::FocusableId kGpStreetReset = backbone::make_focusable_id("settings.gp.street_reset");
inline constexpr backbone::FocusableId kGpChipDenom = backbone::make_focusable_id("settings.gp.chip_denom");
inline constexpr backbone::FocusableId kGpBetSizing = backbone::make_focusable_id("settings.gp.bet_sizing");
inline constexpr backbone::FocusableId kGpDifficultyLow = backbone::make_focusable_id("settings.gp.difficulty_low");
inline constexpr backbone::FocusableId kGpDifficultyHigh = backbone::make_focusable_id("settings.gp.difficulty_high");
inline constexpr backbone::FocusableId kGpTimeCustomToggle = backbone::make_focusable_id("settings.gp.time_toggle");
inline constexpr backbone::FocusableId kGpTimeCustomInput = backbone::make_focusable_id("settings.gp.time_input");
inline constexpr backbone::FocusableId kGpShowHud = backbone::make_focusable_id("settings.gp.show_hud");
inline constexpr backbone::FocusableId kGpShowCountdown = backbone::make_focusable_id("settings.gp.show_countdown");

// Units
inline constexpr backbone::FocusableId kUnUnitToggle = backbone::make_focusable_id("settings.un.unit_toggle");

// Display
inline constexpr backbone::FocusableId kDiTheme = backbone::make_focusable_id("settings.di.theme");
inline constexpr backbone::FocusableId kDiReduceMotion = backbone::make_focusable_id("settings.di.reduce_motion");
inline constexpr backbone::FocusableId kDiBackgroundMovement = backbone::make_focusable_id("settings.di.background");
inline constexpr backbone::FocusableId kDiParticleDrift = backbone::make_focusable_id("settings.di.particles");

// Audio
inline constexpr backbone::FocusableId kAuMusicType = backbone::make_focusable_id("settings.au.music_type");
inline constexpr backbone::FocusableId kAuVolumeSlider = backbone::make_focusable_id("settings.au.volume_slider");
inline constexpr backbone::FocusableId kAuVolumeInput = backbone::make_focusable_id("settings.au.volume_input");
inline constexpr backbone::FocusableId kAuMuteAll = backbone::make_focusable_id("settings.au.mute_all");
inline constexpr backbone::FocusableId kAuMuteSfx = backbone::make_focusable_id("settings.au.mute_sfx");
inline constexpr backbone::FocusableId kAuMuteMusic = backbone::make_focusable_id("settings.au.mute_music");

// Recap
inline constexpr backbone::FocusableId kReDealerArrival = backbone::make_focusable_id("settings.re.dealer_arrival");
inline constexpr backbone::FocusableId kReScreenTransitions = backbone::make_focusable_id("settings.re.transitions");
inline constexpr backbone::FocusableId kReDefaultRecapTab = backbone::make_focusable_id("settings.re.recap_tab");

// Tomatoes
inline constexpr backbone::FocusableId kToShopVisibility = backbone::make_focusable_id("settings.to.shop_visible");
inline constexpr backbone::FocusableId kToResetTomatoes = backbone::make_focusable_id("settings.to.reset");
inline constexpr backbone::FocusableId kToLeaderboardOptIn = backbone::make_focusable_id("settings.to.leaderboard");

// Account (logged-out shell only; click handlers are Part-2 seams)
inline constexpr backbone::FocusableId kAcSignIn = backbone::make_focusable_id("settings.ac.sign_in");
inline constexpr backbone::FocusableId kAcSignUp = backbone::make_focusable_id("settings.ac.sign_up");

// General
inline constexpr backbone::FocusableId kGeConfirmLeave = backbone::make_focusable_id("settings.ge.confirm_leave");
inline constexpr backbone::FocusableId kGeResetAll = backbone::make_focusable_id("settings.ge.reset_all");
inline constexpr backbone::FocusableId kGeResetSection = backbone::make_focusable_id("settings.ge.reset_section");

// Legal
inline constexpr backbone::FocusableId kLeTerms = backbone::make_focusable_id("settings.le.terms");
inline constexpr backbone::FocusableId kLePrivacy = backbone::make_focusable_id("settings.le.privacy");
inline constexpr backbone::FocusableId kLeAbout = backbone::make_focusable_id("settings.le.about");

// The full Tab order (54 stops). Stable storage; the content provider's focus_list
// returns a span over this.
inline constexpr std::array<backbone::FocusableId, 54> kSettingsFocusOrder{
    kFocusSearch,
    kSidebarFocus[0], kSidebarFocus[1], kSidebarFocus[2], kSidebarFocus[3], kSidebarFocus[4],
    kSidebarFocus[5], kSidebarFocus[6], kSidebarFocus[7], kSidebarFocus[8],
    // Gameplay
    kGpStreetPreflopSlider, kGpStreetPreflopInput, kGpStreetFlopSlider, kGpStreetFlopInput,
    kGpStreetTurnSlider, kGpStreetTurnInput, kGpStreetRiverSlider, kGpStreetRiverInput,
    kGpStreetSave, kGpStreetReset, kGpChipDenom, kGpBetSizing, kGpDifficultyLow, kGpDifficultyHigh,
    kGpTimeCustomToggle, kGpTimeCustomInput, kGpShowHud, kGpShowCountdown,
    // Units
    kUnUnitToggle,
    // Display
    kDiTheme, kDiReduceMotion, kDiBackgroundMovement, kDiParticleDrift,
    // Audio
    kAuMusicType, kAuVolumeSlider, kAuVolumeInput, kAuMuteAll, kAuMuteSfx, kAuMuteMusic,
    // Recap
    kReDealerArrival, kReScreenTransitions, kReDefaultRecapTab,
    // Tomatoes
    kToShopVisibility, kToResetTomatoes, kToLeaderboardOptIn,
    // Account
    kAcSignIn, kAcSignUp,
    // General
    kGeConfirmLeave, kGeResetAll, kGeResetSection,
    // Legal
    kLeTerms, kLePrivacy, kLeAbout,
    // X close (Zone 11 shell)
    modal::kSettingsShellClose,
};

// The first body control of each section (the sidebar enter/click jump target).
[[nodiscard]] backbone::FocusableId first_control_of(SettingsSection section) noexcept;

// ----- Per-open view state (owned by Z05 boot, alongside the other runtimes) -----

struct SettingsModalState {
    // The Settings modal drives its OWN focus/reconcile registry rather than the
    // shared app-root one, so opening/closing it never clears the Game screen's
    // reconcile entries (math boxes as text fields, cluster icons as non-text stops).
    // A provider modal that shared the app-root registry would clobber those on open
    // (clear()+repopulate) and again on close, leaving the restored Game context
    // unable to YieldKeyboard on its cluster icons — so a leaked text capture lands on
    // a math box. install_settings_content points focus_registry at own_registry; the
    // pointer stays the wired/null guard the populate/dispatch paths check.
    bridge::FocusRegistry own_registry{};

    // Seams injected by boot (null/empty in native tests / before install):
    bridge::FocusRegistry* focus_registry{nullptr};
    Settings* live{nullptr};                              // the mutable live-settings handle
    std::function<void()> persist{};                      // serialize live -> blob -> save_state
    std::function<void(const AudioSettings&)> apply_audio{};
    std::function<void()> reset_tomatoes{};               // wipe the tomato wallet + save

    // Per-open transient state:
    bool focus_registered{false};
    backbone::FocusableId last_synced_focus{backbone::kNoFocus};

    std::array<char, 64> search_buf{};

    // Street-split widget: edits a STAGED copy (its own Save/Reset; not autosaved).
    StreetWeights street_staged{kDefaultStreetWeights};
    std::array<char, 4> street_buf_preflop{};
    std::array<char, 4> street_buf_flop{};
    std::array<char, 4> street_buf_turn{};
    std::array<char, 4> street_buf_river{};

    std::array<char, 4> volume_buf{};
    std::array<char, 4> custom_time_buf{};

    // Sidebar scroll-to-section request (set by sidebar activate/click; consumed by
    // the body on the next frame). A strict one-shot: render clears it after the body
    // so a filtered-out target can never leave a stale jump pending.
    SettingsSection scroll_target{SettingsSection::Gameplay};
    bool scroll_pending{false};

    // Scroll-follow-focus: the body control to minimally reveal this frame, set only
    // on the frame keyboard focus moved to a NON-text control (and never during a
    // sidebar jump). Text fields are excluded — focusing one arms ImGui's own nav
    // scroll-into-view, and a manual scroll on that frame would fight it (Bug C).
    // kNoFocus most frames; the focused control's widget reveals it when it matches.
    backbone::FocusableId scroll_follow_focus{backbone::kNoFocus};

    // Deferred keyboard X-close: the X-close activate closure lives in the shared
    // registry, so it cannot close (and clear the registry) mid-invocation. It raises
    // this flag; the dispatch handler closes after dispatch_focus_key returns (mirrors
    // custom_popup's request_close).
    bool request_close{false};
    // Deferred section-reset confirm (keyboard path): the section-select Reset button's
    // activate closure raises this; the dispatch handler closes the section modal and
    // opens the standard confirm after dispatch_focus_key returns.
    bool request_section_reset{false};

    // ----- Sub-modal state (rendered via the content-provider seam over Settings) ---
    // Reset-section multi-select: which of the nine sections are selected for reset.
    std::array<bool, kSectionCount> section_reset_selected{};
    // Legal document modal: which document the doc modal shows.
    enum class DocKind : std::uint8_t { Terms = 0, Privacy = 1, About = 2 };
    DocKind doc_kind{DocKind::About};
};

// ----- Exports -----

// Render the Settings body into Zone 11's open cluster-modal shell. Driven each frame
// by the content-provider seam while the Settings modal is topmost. ImGui.
void render_settings_modal(SettingsModalState& state);

// Apply a control's side-effects + persist after the live value changed. Theme ->
// theme::set_theme, audio -> apply_audio; everything else persists only (consumers read
// the live snapshot at their read points / next spawn). The new value is already in
// *state.live.
void on_setting_change(SettingsModalState& state, SettingId id);

// Boot wiring: build the content provider (body + focus list + dispatch, capturing
// `state`) and register it with Zone 11 for the Settings modal. Call once after the
// state's seams are filled.
void install_settings_content(SettingsModalState& state);

}  // namespace poker_trainer::settings
