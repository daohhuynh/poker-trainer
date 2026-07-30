#include "screens/root_frog.hpp"

#include "screens/render_util.hpp"

#include "assets/asset_paths.hpp"

#include "backbone/screen_state.hpp"

#include <array>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string_view>

#include <imgui.h>

#include "theme/theme_tokens.hpp"

namespace poker_trainer::screens {

namespace {

namespace ru = render_util;

[[nodiscard]] animations::Canvas viewport_canvas() {
    const ImVec2 size = ImGui::GetMainViewport()->Size;
    return animations::Canvas{size.x, size.y};
}

// ----- Layout ----------------------------------------------------------------
//
// Height-relative, like home_icon_rect and the rest of the Root layout, so the
// frog holds its optical size across window aspects.

// Frog square, as a fraction of canvas height.
constexpr float kFrogSide = 0.15f;

// Distance from the right / bottom canvas edges. The x margin matches
// home_icon_rect's, so the frog and the Home icon share a right gutter.
constexpr float kFrogMarginX = 0.03f;
constexpr float kFrogMarginY = 0.03f;

// Bubble height as a fraction of canvas height; its width follows the art's
// aspect so the drawn rect never stretches the panel.
constexpr float kBubbleHeight = 0.17f;
constexpr float kBubbleAspect = 768.0f / 512.0f;  // speech_bubble.png w/h

// Horizontal gap between the bubble and the frog, as a fraction of canvas width.
// Same value the Mode Selection cluster puts between its icons.
constexpr float kBubbleGap = 0.012f;

// Where the bubble's bottom edge sits, as a fraction down the frog: the tail is
// a spike off the bubble's bottom-right, so it points down and to the right at
// the frog's head rather than at its feet.
constexpr float kBubbleBottomOnFrog = 0.45f;

// The text-safe interior of speech_bubble.png, in fractions of the drawn rect.
// These are the numbers the art declares in assets/svg/tier2/speech_bubble.svg
// ("TEXT INTERIOR RECT ... x = 0.21  y = 0.24  w = 0.58  h = 0.38"): the oval body
// is drawn so the wobble and the ink stay clear of this box, so it can be filled
// edge to edge. The interior is opaque bg_modal with the ink in text_primary,
// which is why the body below draws in TextPrimary.
//
// Keep these four in step with that comment in the SVG. The bubble outline is
// deliberately hand-drawn and uneven, so a text box even slightly larger than the
// declared safe rect puts glyphs under the wobble instead of inside it.
constexpr float kInteriorLeft = 0.21f;
constexpr float kInteriorRight = 0.79f;   // 0.21 + 0.58
constexpr float kInteriorTop = 0.24f;
constexpr float kInteriorBottom = 0.62f;  // 0.24 + 0.38

// Body size as a fraction of the interior's height. Capacity works out at roughly
// 4.5 / kTextSizeOfInterior^2 characters and is independent of the bubble's pixel
// size, so this value alone decides whether a line fits: 0.24 holds about 79
// characters against a longest line of 61, and the paired unit test caps the
// dialogue at 64. Derived from the bubble rather than from the ImGui atlas size
// so the text scales with the canvas the same way the panel does.
constexpr float kTextSizeOfInterior = 0.24f;

[[nodiscard]] animations::Rect bubble_interior(const animations::Rect& bubble) noexcept {
    return animations::Rect{bubble.x + bubble.w * kInteriorLeft,
                            bubble.y + bubble.h * kInteriorTop,
                            bubble.w * (kInteriorRight - kInteriorLeft),
                            bubble.h * (kInteriorBottom - kInteriorTop)};
}

// Most wrapped lines a bubble will ever draw. The interior holds ~79 characters
// at kTextSizeOfInterior and the paired unit test caps every line at 64, so four
// wrapped lines is the realistic worst case; six leaves headroom without letting
// a pathological string run off the art.
constexpr std::size_t kMaxBubbleLines = 6;

// Draw the bubble body, wrapped and centered inside the art's text-safe interior.
void draw_bubble_text(ImDrawList* dl, const animations::Rect& bubble, std::string_view text) {
    const animations::Rect interior = bubble_interior(bubble);
    ImFont* font = ImGui::GetFont();
    const float px = interior.h * kTextSizeOfInterior;

    const auto width_of = [&](std::string_view sv) {
        return font->CalcTextSizeA(px, FLT_MAX, 0.0f, sv.data(), sv.data() + sv.size()).x;
    };

    // Wrap on SPACES ONLY, rather than handing ImGui a wrap_width. ImGui also
    // breaks after punctuation, which splits the frog's trailing "ribbit..." into
    // "ribbit." and ".." on separate lines -- that reads as a rendering fault
    // rather than as the pause it is meant to be.
    std::array<std::string_view, kMaxBubbleLines> lines{};
    std::size_t count = 0;
    std::size_t line_begin = 0;
    std::size_t line_end = 0;  // end of the last word known to fit
    std::size_t cursor = 0;
    while (cursor <= text.size() && count < kMaxBubbleLines) {
        const std::size_t space = text.find(' ', cursor);
        const std::size_t word_end = (space == std::string_view::npos) ? text.size() : space;
        const std::string_view candidate = text.substr(line_begin, word_end - line_begin);
        // Force the first word of a line even if it alone overflows, so a single
        // long token cannot spin the loop.
        if (width_of(candidate) <= interior.w || line_end == line_begin) {
            line_end = word_end;
            if (word_end == text.size()) {
                lines[count++] = text.substr(line_begin, line_end - line_begin);
                break;
            }
            cursor = word_end + 1;
        } else {
            lines[count++] = text.substr(line_begin, line_end - line_begin);
            line_begin = line_end + 1;
            line_end = line_begin;
            cursor = line_begin;
        }
    }

    const float line_h = font->CalcTextSizeA(px, FLT_MAX, 0.0f, "Ag").y;
    const float block_h = line_h * static_cast<float>(count);
    float y = interior.y + (interior.h - block_h) * 0.5f;
    const ImU32 ink = ru::token_u32(theme::ColorToken::TextPrimary);
    for (std::size_t i = 0; i < count; ++i) {
        const std::string_view line = lines[i];
        const float x = interior.x + (interior.w - width_of(line)) * 0.5f;
        dl->AddText(font, px, ImVec2{x, y}, ink, line.data(), line.data() + line.size());
        y += line_h;
    }
}

}  // namespace

// ----- Pure logic -------------------------------------------------------------

void seed_root_frog(RootFrogState& state) {
    std::random_device rd;
    const std::uint64_t hi = static_cast<std::uint64_t>(rd());
    const std::uint64_t lo = static_cast<std::uint64_t>(rd());
    state.session_seed = (hi << 32) ^ lo;
}

void on_root_entry(RootFrogState& state, std::uint64_t now_ms,
                   backbone::TutorialPhase phase) noexcept {
    ++state.nonce;
    // The session seed carries the entropy; the clock and nonce only separate
    // successive rolls within one session.
    state.rolled_in = roll_frog_appears(state.session_seed + now_ms, state.nonce);
    // Nothing carries across a visit: a bubble left open when the user walked to
    // Mode Selection is gone when they come back, and so is its line.
    state.bubble_open = false;
    state.line = 0;
    state.prompt_seen_this_visit = false;
    note_tutorial_phase(state, phase);
}

void note_tutorial_phase(RootFrogState& state, backbone::TutorialPhase phase) noexcept {
    if (phase == backbone::TutorialPhase::Prompt) {
        state.prompt_seen_this_visit = true;
    }
}

bool frog_showing(const RootFrogState& state) noexcept {
    return frog_showing(state, backbone::read_screen_state().tutorial_state.phase);
}

void advance_frog_bubble(RootFrogState& state, std::uint64_t now_ms) noexcept {
    ++state.nonce;
    const std::uint64_t entropy = state.session_seed + now_ms + state.nonce;
    if (!state.bubble_open) {
        // First click of the visit: any line is fair game, including line 0.
        state.line = static_cast<std::size_t>(frog_mix(entropy) % kFrogLineCount);
        state.bubble_open = true;
        return;
    }
    state.line = next_frog_line(state.line, entropy);
}

// ----- Geometry ---------------------------------------------------------------

animations::Rect frog_rect(animations::Canvas canvas) noexcept {
    const float side = kFrogSide * canvas.height;
    return animations::Rect{canvas.width - kFrogMarginX * canvas.width - side,
                            canvas.height - kFrogMarginY * canvas.height - side, side, side};
}

animations::Rect frog_bubble_rect(animations::Canvas canvas) noexcept {
    const animations::Rect frog = frog_rect(canvas);
    const float h = kBubbleHeight * canvas.height;
    const float w = h * kBubbleAspect;
    return animations::Rect{frog.x - kBubbleGap * canvas.width - w,
                            frog.y + frog.h * kBubbleBottomOnFrog - h, w, h};
}

// ----- Render -----------------------------------------------------------------

void render_root_frog(const RootFrogState& state) {
    if (!frog_showing(state)) {
        return;
    }
    const animations::Canvas canvas = viewport_canvas();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // Bubble first, frog over it: the frog is the thing being clicked, so it is
    // never occluded by its own dialogue.
    if (state.bubble_open) {
        const animations::Rect bubble = frog_bubble_rect(canvas);
        ru::draw_image_slot(dl, bubble, assets::AssetId::SpeechBubble, ru::SlotFallback::Icon,
                            /*focused=*/false);
        draw_bubble_text(dl, bubble, kFrogLines[state.line]);
    }
    ru::draw_image_slot(dl, frog_rect(canvas), assets::AssetId::FrogPeek, ru::SlotFallback::Icon,
                        /*focused=*/false);
}

}  // namespace poker_trainer::screens
