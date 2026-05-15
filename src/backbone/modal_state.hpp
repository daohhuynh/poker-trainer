#pragma once

#include <cstddef>
#include <cstdint>

namespace poker_trainer::backbone {

// Modal identifier. Each modal type has a unique ID. The full set
// of modal IDs is defined in Z11; this header treats them as opaque.
struct ModalId {
    std::uint32_t value{0};
    constexpr bool operator==(const ModalId&) const noexcept = default;
};

inline constexpr ModalId kNoModal{0};

// Returns true if any modal is currently open. Includes any modal
// type — Settings, Shop, Help, confirmation modals, auth modals,
// tutorial-related modals.
[[nodiscard]] bool is_any_modal_open() noexcept;

// Returns the ID of the topmost modal, or kNoModal if none.
// The topmost modal is the one most recently opened (or the only
// one open if there's just one).
[[nodiscard]] ModalId topmost_modal() noexcept;

// The current modal stack depth. 0 means no modals; each open
// modal adds 1.
[[nodiscard]] std::size_t modal_stack_depth() noexcept;

// ----- Writer API (Z11 only) -----

// Push a modal onto the stack. Called by Z11 when a modal opens.
void notify_modal_opened(ModalId id) noexcept;

// Pop a modal from the stack. Called by Z11 when a modal closes.
// The id should match the topmost modal; if it doesn't, the
// implementation logs a warning and pops anyway (treating the
// mismatch as a bug to fix elsewhere).
void notify_modal_closed(ModalId id) noexcept;

// Reset modal state. Used by the integration test only.
void reset_modal_state_for_testing() noexcept;

}  // namespace poker_trainer::backbone
