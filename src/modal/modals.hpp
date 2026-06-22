#pragma once

#include "modal/confirm_modal.hpp"
#include "modal/outage_banner.hpp"

#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"
#include "backbone/modal_state.hpp"

#include "assets/asset_paths.hpp"

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

struct ImDrawList;

namespace poker_trainer::bridge {
class FocusRegistry;
}

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

// ----- Settings / Shop shell focusables (content is Z12 / Module 7 seams) -----
inline constexpr backbone::FocusableId kSettingsShellClose =
    backbone::make_focusable_id("settings.close");
inline constexpr backbone::FocusableId kShopShellClose =
    backbone::make_focusable_id("shop.close");

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

// ----- App-root Z11 runtime (owned by Z05 boot; CLAUDE.md sec.10) -----
struct ModalRuntime {
    // Shared focus/input reconciliation registry (off BridgeRuntime). Used only by
    // text-field modals (the leaderboard search); Help / confirm / shells have no
    // text fields and never touch it.
    bridge::FocusRegistry* focus_registry{nullptr};

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

    // Leaderboard search buffer (thin shell; data is a server-side seam).
    std::string leaderboard_search{};

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
