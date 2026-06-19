#pragma once

#include "modal/confirm_modal.hpp"
#include "modal/outage_banner.hpp"

#include "backbone/focus_manager.hpp"
#include "backbone/modal_state.hpp"

#include "animations/button_morph.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

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
