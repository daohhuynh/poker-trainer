#pragma once

#include "modal/modals.hpp"

#include <cstdint>

// Zone 11 — Module 7 Shop content, rendered inside the cluster-modal frame
// (ARCHITECTURE Module 7 "The Shop UI"). Fixed size, no scroll: all 12 tracks render at
// once, grouped into the four genre sections, each row a three-state Buy/Add/Remove
// button with the armed-confirm Buy, the in-rotation dot, and the insufficient-funds
// state. The header carries the Leaderboard-swap icon + the Spendable Tomatoes count.
//
// The render reads a boot-computed ShopSnapshot and drives mutations through the wired
// ShopController callbacks (modals.hpp) — Z11 never touches Zone 04 / Zone 03 directly.
// The render is a marked seam (CLAUDE.md §9, browser-verified); the pure button-state
// helper is unit-tested.

namespace poker_trainer::modal {

// The visible state of a track row's action button, derived from the row + whether its
// Buy is armed. BuyDisabled is the insufficient-funds state (reduced opacity, no arming).
enum class ShopButtonKind : std::uint8_t { Buy, Confirm, Add, Remove, BuyDisabled };

// Pure: the button kind for a row. Owned tracks toggle Add/Remove by rotation; locked
// tracks show Buy (or Confirm when armed) when affordable, BuyDisabled otherwise.
[[nodiscard]] ShopButtonKind shop_button_kind(const ShopRowView& row, bool armed) noexcept;

// Render the Shop content into the open cluster-modal frame. Dispatched from
// render_shop_shell when a ShopController is wired (else the placeholder shell renders).
void render_shop_view(ModalRuntime& runtime);

}  // namespace poker_trainer::modal
