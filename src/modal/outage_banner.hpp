#pragma once

#include <cstdint>
#include <string>
#include <string_view>

// Zone 11 — Service Outage Banner (ARCHITECTURE Notes — Service Outage Banner).
//
// A single, non-stacking transient banner: any zone calls trigger_outage_banner
// (declared in modals.hpp) and the banner slides in from above (~300ms ease-out),
// holds 5s while a white countdown bar drains full -> empty, then slides out
// (~300ms ease-in). A click anywhere on it dismisses (halts the bar, slides out).
// A second trigger while visible REPLACES the message in place and restarts the 5s
// hold at a full bar WITHOUT replaying the slide-in.
//
// This header is the PURE state machine (no ImGui), unit-tested directly. The
// ImGui foreground-draw-list render lives in outage_banner.cpp and is not tested.

namespace poker_trainer::modal {

inline constexpr float kBannerSlideMs = 300.0f;
inline constexpr std::uint64_t kBannerHoldMs = 5000;

enum class BannerPhase : std::uint8_t {
    Hidden = 0,
    SlidingIn = 1,
    Holding = 2,
    SlidingOut = 3,
};

struct BannerState {
    BannerPhase phase{BannerPhase::Hidden};
    std::uint64_t phase_start_ms{0};
    // Countdown-bar fraction frozen at the moment SlidingOut begins: 0 on natural
    // expiry (bar already drained), or the live value on a click dismiss (the bar
    // "halts" per spec).
    float bar_frozen{0.0f};
    std::string message{};
};

// What the render path needs this frame (pure function of state + now).
struct BannerGeometry {
    bool visible{false};
    float reveal{0.0f};  // 0 = fully above the canvas top edge, 1 = fully in view
    float bar{0.0f};     // countdown-bar width fraction, 1 = full .. 0 = empty
};

// Advance the phase machine to `now`. Call once per frame before banner_geometry.
// Drives SlidingIn -> Holding (after ~300ms), Holding -> SlidingOut (after 5s,
// freezing the bar at 0), SlidingOut -> Hidden (after ~300ms).
void banner_advance(BannerState& s, std::uint64_t now) noexcept;

// Trigger / re-trigger. Hidden -> SlidingIn (full slide-in). Any visible phase ->
// Holding in place (message replaced, 5s restarted, bar reset to full, slide-in
// NOT replayed).
void banner_trigger(BannerState& s, std::string_view message, std::uint64_t now);

// Click dismiss from any visible phase -> SlidingOut, freezing the countdown bar
// at its current value. A no-op when Hidden.
void banner_dismiss(BannerState& s, std::uint64_t now) noexcept;

[[nodiscard]] bool banner_visible(const BannerState& s) noexcept;

[[nodiscard]] BannerGeometry banner_geometry(const BannerState& s, std::uint64_t now) noexcept;

// Advance + draw the banner on the ImGui foreground draw list (top-center), reading
// the animation clock for `now`. Handles its own click-to-dismiss. ImGui;
// browser-verified, not unit-tested. A no-op when the banner is Hidden.
void render_outage_banner(BannerState& s);

}  // namespace poker_trainer::modal
