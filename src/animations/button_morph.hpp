#pragma once

// Zone 07 — Root -> Mode Selection button morph.
//
// All motion is expressed as pure functions of normalized progress t in [0, 1]
// (easing, per-button stagger, position/scale lerp, the synchronized background
// crossfade alpha) plus pure relative-layout geometry (functions of the canvas
// dimensions only — never absolute pixels). Production converts elapsed
// milliseconds to t at the call site via the animation clock; these functions
// never read the clock, which is what lets the unit tests drive them
// deterministically without a clock or an ImGui context.
//
// MorphController carries the only transient state (the start timestamp). It is
// a value type owned by its caller (Zone 05's main loop in production; a local
// in tests) — there is no global morph state, per CLAUDE.md section 10.

#include <cstdint>
#include <optional>

namespace poker_trainer::animations {

// ----- Timing constants (milliseconds) -----
//
// Per ARCHITECTURE: the morph is eased (smooth curve, not linear) and staggered
// by ~50 ms per button; the synchronized background crossfade is ~300 ms,
// ease-out. The per-button motion duration and the resulting total are a
// visual-implementation-pass call kept proportional and documented here.
inline constexpr std::uint64_t kStaggerMs = 50;       // per-button start offset
inline constexpr std::uint64_t kButtonMotionMs = 300;  // one button's travel time
inline constexpr std::uint32_t kMorphButtonCount = 4;
inline constexpr std::uint64_t kTotalMorphMs =
    kButtonMotionMs + kStaggerMs * (kMorphButtonCount - 1);  // 450 ms
inline constexpr std::uint64_t kCrossfadeMs = 300;     // background blur crossfade

// ----- Tiny geometry primitives (no ImGui dependency, so the math stays pure
// and unit-testable). The render layer converts these to ImVec2/ImGui calls. ---

struct Vec2 {
    float x{0.0f};
    float y{0.0f};
    [[nodiscard]] constexpr bool operator==(const Vec2&) const noexcept = default;
};

struct Rect {
    float x{0.0f};       // top-left corner
    float y{0.0f};
    float w{0.0f};       // size
    float h{0.0f};
    [[nodiscard]] constexpr bool operator==(const Rect&) const noexcept = default;
};

// The current canvas (== browser viewport) dimensions. All Zone 07 layout
// derives from this; nothing uses absolute pixel coordinates.
struct Canvas {
    float width{0.0f};
    float height{0.0f};
};

// The four buttons that morph from the Root 2x2 grid to the Mode Selection
// screen. Play becomes the STANDARD button (top-left); the other three shrink
// into the top-right icon cluster.
enum class MorphButton : std::uint8_t {
    Play = 0,      // -> STANDARD, top-left region
    Settings = 1,  // -> Settings cog icon, top-right cluster
    Shop = 2,      // -> Shop icon, top-right cluster
    Help = 3,      // -> Help icon, top-right cluster
};

// ----- Pure scalar math -----

[[nodiscard]] float lerp(float a, float b, float t) noexcept;

// Smooth ease-in-out curve (cubic), monotonic with ease(0)=0, ease(1)=1.
// Explicitly not linear, per ARCHITECTURE's "smooth easing curve, not linear".
[[nodiscard]] float ease_in_out(float t) noexcept;

// Ease-out curve for the background crossfade alpha, monotonic 0->1.
[[nodiscard]] float crossfade_alpha(float t) noexcept;

// The fraction of the global morph timeline at which button `index` begins to
// move (its 50 ms-per-button stagger offset, normalized by the total duration).
[[nodiscard]] float button_start_fraction(MorphButton button) noexcept;

// A single button's local, un-eased progress at global progress `global_t`:
// 0 before its staggered start, ramping to 1 once it has moved for its full
// motion duration. Clamped to [0, 1].
[[nodiscard]] float button_local_t(float global_t, MorphButton button) noexcept;

// A single button's eased progress (ease_in_out applied to button_local_t).
[[nodiscard]] float button_eased_progress(float global_t, MorphButton button) noexcept;

// ----- Pure relative-layout geometry (functions of the canvas only) -----

[[nodiscard]] Rect lerp_rect(const Rect& a, const Rect& b, float t) noexcept;

// Resting rect of a button in the Root screen's middle 2x2 grid
// (Play=TL, Settings=TR, Shop=BL, Help=BR of the grid).
[[nodiscard]] Rect root_grid_button_rect(MorphButton button, Canvas canvas) noexcept;

// Morph target rect on the Mode Selection screen: Play -> STANDARD button in the
// top-left region; Settings/Shop/Help -> small icon slots in the top-right
// cluster, to the left of the (stationary) Home icon.
[[nodiscard]] Rect mode_button_target_rect(MorphButton button, Canvas canvas) noexcept;

// The interpolated rect of a morphing button at global progress `global_t`
// (position and scale both lerp from the Root grid rect to the Mode target rect,
// driven by the button's eased, staggered progress).
[[nodiscard]] Rect morph_button_rect(MorphButton button, float global_t, Canvas canvas) noexcept;

// Static Zone 07 layout helpers (used by the resting-screen renders).
[[nodiscard]] Rect logo_rect(Canvas canvas) noexcept;          // top-left logo
[[nodiscard]] Rect home_icon_rect(Canvas canvas) noexcept;     // top-right Home (stationary)
[[nodiscard]] Rect standard_button_rect(Canvas canvas) noexcept;  // == Play's morph target
// The three centered middle buttons on Mode Selection, left->right:
// 0 = Aggressor, 1 = Caller, 2 = Custom.
[[nodiscard]] Rect mode_middle_button_rect(std::uint32_t index, Canvas canvas) noexcept;

// ----- Morph driver (the only stateful piece; a value type, no globals) -----

// The outcome of one advance_morph step.
enum class MorphTick : std::uint8_t {
    Idle = 0,          // no morph running
    InProgress = 1,    // running, not yet complete
    JustCompleted = 2,  // crossed the finish line on this step
};

// Tracks a single in-flight Root->Mode morph. Owned by the caller; reset to idle
// after completion is acknowledged via advance_morph.
class MorphController {
public:
    // Begin a morph at `now_ms`. Ignored (debounced) if one is already in
    // flight — a second Play click mid-morph does not restart it.
    void start(std::uint64_t now_ms) noexcept;

    // True while a morph is in flight (started, not yet acknowledged complete).
    [[nodiscard]] bool active() const noexcept { return start_ms_.has_value(); }

    // Global normalized progress in [0, 1] at `now_ms`. 0 when idle.
    [[nodiscard]] float progress(std::uint64_t now_ms) const noexcept;

    // Background crossfade alpha in [0, 1] at `now_ms` (its own ~300 ms,
    // ease-out timeline, synchronized to the morph start). 0 when idle.
    [[nodiscard]] float crossfade(std::uint64_t now_ms) const noexcept;

    // True once `now_ms` is at or past the full morph duration.
    [[nodiscard]] bool is_complete(std::uint64_t now_ms) const noexcept;

    // Force back to idle (without touching screen state).
    void reset() noexcept { start_ms_.reset(); }

private:
    std::optional<std::uint64_t> start_ms_;
};

// Advance an in-flight morph. Returns InProgress while running; on the step that
// crosses the finish line it sets the screen state to Mode Selection, resets the
// controller to idle, and returns JustCompleted; returns Idle when nothing is
// running. This is the single place the screen transition is committed.
MorphTick advance_morph(MorphController& morph, std::uint64_t now_ms) noexcept;

}  // namespace poker_trainer::animations
