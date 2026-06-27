#pragma once

#include "modal/confirm_modal.hpp"
#include "modal/outage_banner.hpp"

#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"
#include "backbone/modal_state.hpp"

#include "assets/asset_paths.hpp"

#include "audio/audio_paths.hpp"

#include "animations/button_morph.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "bridge/focus_registry.hpp"

struct ImDrawList;

// Zone 11 — Modal Infrastructure + Persistent Cluster: the public interface.
//
// The modal system renders as a TOP-LEVEL overlay above the active screen each
// frame (registered with the bridge's overlay-render seam). Modals drive the sealed
// backbone modal stack (notify_modal_opened/closed), so modal_stack_depth() changes
// on open/close — which Z03 edge-polls for the swoosh and Z10 will poll to pause the
// Delta Timer. Focus traps go through focus_manager (push/pop_focus_context).

namespace poker_trainer::modal {

// ----- Modal identities (backbone::ModalId values; 0 = none) -----
inline constexpr backbone::ModalId kHelpModalId{1};
inline constexpr backbone::ModalId kSettingsModalId{2};
inline constexpr backbone::ModalId kShopModalId{3};
inline constexpr backbone::ModalId kLeaderboardModalId{4};
inline constexpr backbone::ModalId kLeaveDrillConfirmId{5};
// Zone 12 sub-modals, rendered via the content-provider seam, stacked over Settings.
inline constexpr backbone::ModalId kSettingsSectionResetId{6};  // multi-select reset
inline constexpr backbone::ModalId kSettingsDocId{7};           // ToS / Privacy / About
// Zone 12 Sign In / Sign Up auth modal (single, content-swapping). Rendered through the
// auth_modals shell (render_auth_modal) + the generic content-provider seam (Zone 12
// supplies the form body / focus list / dispatch). Openable from the Account section and
// (later) the Tutorial Complete screen.
inline constexpr backbone::ModalId kAuthModalId{8};

// ----- Settings / Shop shell focusables (content is Z12 / Module 7 seams) -----
inline constexpr backbone::FocusableId kSettingsShellClose =
    backbone::make_focusable_id("settings.close");
inline constexpr backbone::FocusableId kShopShellClose =
    backbone::make_focusable_id("shop.close");
// The auth modal's X-close focus id (last stop in every auth focus list; the form fields
// are Zone 12 focusables).
inline constexpr backbone::FocusableId kAuthShellClose =
    backbone::make_focusable_id("auth.close");

// ----- Shop traversal focusables -----
// The Leaderboard-swap icon (first stop) and the X close (kShopShellClose, last stop); the
// per-track row buttons sit between them, their ids derived from the track index in
// shop_view (shop_row_focus_id). The greyed insufficient-funds rows are not focus stops.
inline constexpr backbone::FocusableId kShopLeaderboardIcon =
    backbone::make_focusable_id("shop.leaderboard_icon");

// ----- Leaderboard traversal focusables (5 stops, fixed order) -----
inline constexpr backbone::FocusableId kLeaderboardShopIcon =
    backbone::make_focusable_id("leaderboard.shop_icon");
inline constexpr backbone::FocusableId kLeaderboardSearch =
    backbone::make_focusable_id("leaderboard.search");
inline constexpr backbone::FocusableId kLeaderboardList =
    backbone::make_focusable_id("leaderboard.list");
inline constexpr backbone::FocusableId kLeaderboardYourRow =
    backbone::make_focusable_id("leaderboard.your_row");
inline constexpr backbone::FocusableId kLeaderboardClose =
    backbone::make_focusable_id("leaderboard.close");

// Rolling window (ms) within which successive digit keystrokes on the leaderboard list
// stop APPEND to one rank number; after this long without a digit the buffer clears and
// the next digit starts a fresh number.
inline constexpr std::uint64_t kRankJumpWindowMs = 3000;

// Digit-accumulation state for the list-stop "jump to rank" navigation. `active` false
// means no sequence is in progress (the next digit starts fresh). The pure operators on
// this buffer live in leaderboard_view (rank_jump_digit / rank_jump_arrow).
struct RankJumpBuffer {
    std::int64_t value{-1};    // raw rank number typed so far (-1 when inactive)
    std::uint64_t last_ms{0};  // timestamp of the last accepted digit
    bool active{false};
};

// ----- Persistent cluster -----
enum class ClusterIcon : std::uint8_t { Shop, Help, Settings, Home, Close };
enum class ClusterScreen : std::uint8_t { ModeSelection, Game, PostRound };

// How the cluster icons render. Mode Selection keeps Zone 07's morph-handoff button
// look (the Root->Mode morph animates buttons into these rects, so they render as
// buttons-with-labels at rest to avoid a pop); Game / Post-Round render the spec's
// icon glyphs (text_primary glyph, bg_button_hover on hover, no default fill).
enum class ClusterStyle : std::uint8_t { IconGlyph, MorphButton };

// Everything render_persistent_cluster needs, supplied by the calling screen (which
// owns its geometry, so positions conform to each screen's existing layout). The
// rects/ids are left -> right: Shop, Help, Settings, [Home | Close].
struct ClusterContext {
    ClusterScreen screen{ClusterScreen::ModeSelection};
    ClusterStyle style{ClusterStyle::IconGlyph};
    std::array<animations::Rect, 4> rects{};
    std::array<backbone::FocusableId, 4> ids{};
};

// ----- Generic per-modal content provider seam (additive) -----
//
// A zone (Zone 12 Settings; later the Shop/Module 7) registers a content provider for
// a cluster-modal id. The shared shell renders the standard chrome (centered window,
// X close, pill header, lock banner, click-outside) and calls render_body in between;
// the modal focus trap is built from focus_list / initial_focus; and ModalLayer
// Space/Enter/arrow keys route to dispatch. Modals WITHOUT a provider keep their
// dedicated render / focus / key paths unchanged — only a provider-registered modal
// changes behavior. The std::function members keep this an opaque hook, so modal/
// never includes a consumer zone (no dependency inversion).
struct ModalContentProvider {
    assets::AssetId header_icon{};
    const char* header_name{""};
    backbone::FocusableId close_focus{};
    std::function<void()> render_body{};
    std::function<std::span<const backbone::FocusableId>()> focus_list{};
    backbone::FocusableId initial_focus{};
    std::function<bool(const backbone::KeyEvent&)> dispatch{};  // true => key consumed
    std::function<void()> on_open{};
    std::function<void()> on_close{};
};

// ----- Module 7 Shop seam (additive; dependency-inverted like ModalContentProvider) -----
//
// The Shop body (Zone 11) renders from a boot-computed snapshot and drives mutations
// through injected callbacks. Z11 holds only the Phase-0 audio track types + plain data;
// the wallet mutation (Zone 04) + audio shuffle-pool changes (Zone 03) + persistence are
// wired at boot, where that cross-zone composition is legal. Empty until boot wires it.

// Per-track render state for one Shop row, computed each frame from the live wallet +
// music library. Drives the three-state button (Buy / Add / Remove) and the price/dot.
struct ShopRowView {
    audio::MusicTrackId track{};
    bool owned{false};       // starter or purchased
    bool in_pool{false};     // currently in its genre's shuffle rotation (the dot)
    bool affordable{false};  // spendable >= price (meaningful only when !owned)
    std::uint32_t price{0};  // tomato price (0 for starters / owned)
};

struct ShopSnapshot {
    std::uint64_t spendable{0};
    std::array<ShopRowView, audio::kMusicTrackCount> rows{};
};

struct ShopController {
    std::function<ShopSnapshot()> snapshot{};             // live read each frame
    std::function<void(audio::MusicTrackId)> on_buy{};    // commit a purchase (+ persist)
    std::function<void(audio::MusicTrackId)> on_add{};    // add to rotation (+ audio + persist)
    std::function<void(audio::MusicTrackId)> on_remove{}; // remove from rotation (+ audio + persist)
    [[nodiscard]] bool wired() const noexcept { return static_cast<bool>(snapshot); }
};

// ----- Leaderboard seam (additive) -----
struct LeaderboardRow {
    std::uint32_t rank{0};
    std::string name;
    std::uint64_t lifetime{0};
};

// One leaderboard fetch outcome (cached on open / Retry, not refetched per frame).
struct LeaderboardData {
    bool ok{false};  // false => fetch failed (error + Retry state)
    std::vector<LeaderboardRow> rows;
};

// Account state for the persistent "your rank" bottom row.
enum class LeaderboardSelfState : std::uint8_t { Guest, OptedIn, OptedOut };
struct LeaderboardSelf {
    LeaderboardSelfState state{LeaderboardSelfState::Guest};
    std::string name;          // logged-in display name (empty for guests)
    std::uint64_t lifetime{0};  // logged-in Lifetime Tomatoes
};

struct LeaderboardController {
    std::function<LeaderboardData()> fetch{};            // hit the server (blocking)
    std::function<LeaderboardSelf()> self{};             // your-rank row content
    std::function<void()> open_sign_in{};                // guest row link
    std::function<void()> open_sign_up{};                // guest row link
    std::function<void()> enable_opt_in{};               // opted-out "opt in" link: flips
                                                         // the persisted+synced opt-in on
                                                         // in place (no nav to Settings)
    [[nodiscard]] bool wired() const noexcept { return static_cast<bool>(fetch); }
};

// Which option in the leaderboard guest-row grouped stop is arrow-highlighted (Category A
// positional toggle). None = no highlight; Enter/Space defaults to Sign in. SignIn/SignUp =
// the respective link is highlighted; Enter/Space opens it.
enum class GuestRowHighlight : std::uint8_t { None, SignIn, SignUp };

// ----- App-root Z11 runtime (owned by Z05 boot; CLAUDE.md sec.10) -----
struct ModalRuntime {
    // Focus/input reconciliation registry for the one text-field modal (the leaderboard
    // search). install_modals points this at own_registry below — NOT the shared app-root
    // registry, which Z09's Game-screen math inputs use: sharing it would clobber that
    // reconcile (see the shared-focus-registry lesson). Help / confirm / shop shells have
    // no text fields and never touch it.
    bridge::FocusRegistry* focus_registry{nullptr};
    // Backing store for focus_registry, owned by this runtime (clobber-safe, mirrors the
    // auth modal's AccountModalState::own_registry).
    bridge::FocusRegistry own_registry{};

    BannerState banner{};

    // The active confirmation instance's spec (body + Yes action), valid while a
    // confirmation modal is the topmost modal.
    ConfirmSpec confirm{};

    // Geometry/ids of the last-rendered cluster, cached so the screens' mouse
    // hit-test and the Z11 keyboard handler can resolve a hit / focus to an icon.
    bool has_cluster{false};
    ClusterContext cluster{};

    // Click-outside opening-frame guard (mirrors CustomPopupState::just_opened):
    // suppresses the click that opened the modal from immediately dismissing it.
    bool modal_just_opened{false};

    // Leaderboard search buffer (live case-insensitive substring filter).
    std::string leaderboard_search{};

    // Module 7 Shop controller (snapshot + buy/add/remove), wired at boot. Empty in
    // a build without the Module 7 wiring — render_shop_shell then falls back to the
    // placeholder seam shell.
    ShopController shop{};
    // The track whose Buy button is armed ("CONFIRM"); a second click on the same row
    // commits. Cleared on commit, on any other Buy arming, and on modal close.
    std::optional<audio::MusicTrackId> shop_armed_buy{};

    // Leaderboard controller (fetch + your-rank + guest/opt-out links), wired at boot.
    LeaderboardController leaderboard{};
    // Cached fetch for the open leaderboard view + whether a fetch has completed since
    // it opened (so the blocking fetch runs once per open / Retry, not every frame).
    LeaderboardData leaderboard_data{};
    bool leaderboard_loaded{false};
    // The single highlighted-player cursor shared by both jump mechanisms — the search
    // jump-by-name (Enter / click a result) and the list-stop jump-by-rank (digits /
    // arrows). -1 = none. The highlighted row is tinted and scrolled into view.
    std::int64_t leaderboard_highlight_rank{-1};
    // Rolling digit buffer for the list stop's jump-by-rank (reset on open, on an arrow,
    // and on a name jump).
    RankJumpBuffer leaderboard_rank_buffer{};
    // The element ImGui's keyboard focus was last reconciled to (search-field text-capture
    // sync); reset to kNoFocus on open. Mirrors AccountModalState::last_synced_focus.
    backbone::FocusableId leaderboard_last_synced{backbone::kNoFocus};
    // Set when navigation moves the highlight (digit / arrow / Enter / click); the list
    // render scrolls the highlighted row into view once, then clears it, so the user can
    // still scroll the list manually between jumps.
    bool leaderboard_scroll_to_highlight{false};

    // Arrow highlight for the leaderboard guest-row grouped stop (Category A positional
    // toggle). Reset on open; set by Left/Right/digit keys while the stop is focused.
    GuestRowHighlight guest_row_highlight{GuestRowHighlight::None};

    // Content providers registered by consumer zones (Zone 12 Settings). Searched on
    // open/close/render/key by id. Empty until a zone registers one.
    std::vector<std::pair<backbone::ModalId, ModalContentProvider>> content_providers{};
};

// Boot wiring: store the runtime pointer, register the overlay renderer with the
// bridge, and install the ModalLayer (Escape + activation) and cluster keyboard
// handlers. Called once from Z05 boot.
void install_modals(ModalRuntime& runtime);

// The overlay render entry (registered with the bridge): draws the active modal
// then the outage banner, above the active screen. Safe to call every frame.
void render_modal_overlay();

// ----- Open / close (ZONES.md exports) -----
void open_modal(backbone::ModalId id);
void close_modal();
void open_help_modal();
void open_settings_modal();
void open_shop_modal();
void open_leave_drill_confirm();

// Open the Leaderboard view fresh (resets the cached fetch, the search buffer, and the
// Enter-jump highlight so each open re-fetches and starts unfiltered).
void open_leaderboard_modal();

// In-place content swap between the Shop and Leaderboard views (same modal frame, per
// the Module 7 modal-swap): close the current cluster modal then open the target. The
// modal-depth pollers (Z03 swoosh / Z10 pause) sample once per frame, so the
// within-frame close+open nets to no depth edge.
void swap_to_leaderboard();
void swap_to_shop();

// Generic confirmation opener (foreshadowed by confirm_modal.hpp): sets the active
// confirm spec (body + Yes action) and opens the shared confirmation modal. Used by
// Zone 12's reset-all / reset-section / reset-tomatoes flows (and any future confirm).
void open_confirm_modal(ConfirmSpec spec);

// ----- Generic content-provider registry (additive seam) -----
// Register (or replace) the content provider for a cluster-modal id. Called once at
// boot by the owning zone, after install_modals.
void register_modal_content(backbone::ModalId id, ModalContentProvider provider);
// The provider for `id`, or nullptr when none is registered.
[[nodiscard]] const ModalContentProvider* modal_content_for(backbone::ModalId id);

// ----- Outage banner (callable from any zone) -----
void trigger_outage_banner(std::string_view message);

// ----- Cluster (ZONES.md export render_persistent_cluster) -----
void render_persistent_cluster(ImDrawList* dl, const ClusterContext& ctx);

// Map a click / a focused element to the cluster icon it hits, using the geometry
// cached by the last render_persistent_cluster. nullopt when nothing matches.
[[nodiscard]] std::optional<ClusterIcon> cluster_hit_test(float x, float y);
[[nodiscard]] std::optional<ClusterIcon> cluster_action_for_focus(backbone::FocusableId focused);

// Perform an icon's action: Shop/Help/Settings open their modal; Home returns to
// Root (instant cut; the ceremonial transition is a Z14 seam); Close opens the
// leave-drill confirmation (Game).
void activate_cluster_icon(ClusterIcon icon);

}  // namespace poker_trainer::modal
