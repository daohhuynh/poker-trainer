// Shared focus/input reconciliation substrate — the pure parts: the registry, the
// reconcile DECISION (generalized from Z09's former reconcile_imgui_focus), and the
// key dispatch. The ImGui render glue (begin_focus_reconcile / grab / ring) is
// browser-verified, not unit-tested (CLAUDE.md §9).

#include "bridge/focus_registry.hpp"

#include "backbone/event_router.hpp"
#include "backbone/focus_manager.hpp"

#include <gtest/gtest.h>

namespace br = poker_trainer::bridge;
namespace bb = poker_trainer::backbone;

namespace {

constexpr bb::FocusableId kBox0 = bb::make_focusable_id("test.focus.box0");
constexpr bb::FocusableId kBox1 = bb::make_focusable_id("test.focus.box1");
constexpr bb::FocusableId kGroup = bb::make_focusable_id("test.focus.group");
constexpr bb::FocusableId kUnregistered = bb::make_focusable_id("test.focus.absent");

// A registry mirroring a Z09 aggressor surface: two text boxes + one non-text stop.
br::FocusRegistry boxes_and_group() {
    br::FocusRegistry registry;
    registry.register_element(kBox0, br::FocusableEntry{.is_text_field = true});
    registry.register_element(kBox1, br::FocusableEntry{.is_text_field = true});
    registry.register_element(kGroup, br::FocusableEntry{.is_text_field = false});
    return registry;
}

}  // namespace

// ----- Registry -----

TEST(FocusRegistry, FindReturnsRegisteredEntryAndNullForAbsent) {
    const br::FocusRegistry registry = boxes_and_group();
    ASSERT_NE(registry.find(kBox0), nullptr);
    EXPECT_TRUE(registry.find(kBox0)->is_text_field);
    EXPECT_EQ(registry.find(kUnregistered), nullptr);
}

TEST(FocusRegistry, IsTextFieldTrueForTextFalseForGroupAndAbsent) {
    const br::FocusRegistry registry = boxes_and_group();
    EXPECT_TRUE(registry.is_text_field(kBox0));
    EXPECT_FALSE(registry.is_text_field(kGroup));        // registered non-text stop
    EXPECT_FALSE(registry.is_text_field(kUnregistered));  // absent => not a text field
}

TEST(FocusRegistry, RegisterReplacesExistingEntry) {
    br::FocusRegistry registry;
    registry.register_element(kBox0, br::FocusableEntry{.is_text_field = true});
    registry.register_element(kBox0, br::FocusableEntry{.is_text_field = false});  // replace
    EXPECT_FALSE(registry.is_text_field(kBox0));
}

TEST(FocusRegistry, ClearDropsAllEntries) {
    br::FocusRegistry registry = boxes_and_group();
    registry.clear();
    EXPECT_EQ(registry.find(kBox0), nullptr);
    EXPECT_EQ(registry.find(kGroup), nullptr);
}

// ----- Reconcile decision (pure; ported from Z09's reconcile_imgui_focus) -----

TEST(DecideReconcile, NavToTextBoxRequestsImGuiKeyboardFocus) {
    const br::FocusRegistry registry = boxes_and_group();
    // Outline moved onto a text box this frame -> steer ImGui's text focus there.
    const br::FocusReconcile r = br::decide_focus_reconcile(registry, bb::kNoFocus, kBox0);
    EXPECT_EQ(r.action, br::ImGuiFocusAction::FocusTextBox);
    EXPECT_EQ(r.target, kBox0);
}

TEST(DecideReconcile, NavToNonTextStopYieldsKeyboardCapture) {
    const br::FocusRegistry registry = boxes_and_group();
    // Outline moved onto the non-text stop -> release any active box so
    // WantCaptureKeyboard goes false (digits then reach the stop's own handler).
    const br::FocusReconcile r = br::decide_focus_reconcile(registry, kBox0, kGroup);
    EXPECT_EQ(r.action, br::ImGuiFocusAction::YieldKeyboard);
}

TEST(DecideReconcile, NonTextStopYieldsEveryFrameNotJustOnChange) {
    // Bug B: a pending SetKeyboardFocusHere from the box just left re-activates that
    // box AFTER a one-shot ClearActiveID. The yield must fire EVERY frame the stop
    // holds focus -- including when focus is unchanged -- so it is decided before
    // the unchanged early-out below.
    const br::FocusRegistry registry = boxes_and_group();
    EXPECT_EQ(br::decide_focus_reconcile(registry, kGroup, kGroup).action,
              br::ImGuiFocusAction::YieldKeyboard);
}

TEST(DecideReconcile, UnchangedTextBoxFocusIsNoOp) {
    const br::FocusRegistry registry = boxes_and_group();
    // Same text box both frames -> never re-grab (would trap the text caret).
    EXPECT_EQ(br::decide_focus_reconcile(registry, kBox0, kBox0).action,
              br::ImGuiFocusAction::None);
}

TEST(DecideReconcile, FocusOnUnownedElementLeavesImGuiAlone) {
    const br::FocusRegistry registry = boxes_and_group();
    // An element this surface does not own (absent from the registry) -> None.
    const br::FocusReconcile r = br::decide_focus_reconcile(registry, kGroup, kUnregistered);
    EXPECT_EQ(r.action, br::ImGuiFocusAction::None);
}

// ----- Dispatch (Space/Enter -> activate; arrows -> adjust) -----

namespace {

bb::KeyCode kc(bb::KeyCode code) { return code; }  // readability shim

}  // namespace

TEST(DispatchFocusKey, SpaceAndEnterRunActivate) {
    int activated = 0;
    br::FocusRegistry registry;
    registry.register_element(kBox0, br::FocusableEntry{.is_text_field = false,
                                                        .activate = [&] { ++activated; }});
    EXPECT_TRUE(br::dispatch_focus_key(registry, kBox0, kc(bb::KeyCode::Enter)));
    EXPECT_TRUE(br::dispatch_focus_key(registry, kBox0, kc(bb::KeyCode::Space)));
    EXPECT_EQ(activated, 2);
}

TEST(DispatchFocusKey, ArrowsRunAdjustWithSignedDelta) {
    int last_delta = 0;
    br::FocusRegistry registry;
    registry.register_element(
        kBox0, br::FocusableEntry{.is_text_field = false, .adjust = [&](int d) { last_delta = d; }});
    EXPECT_TRUE(br::dispatch_focus_key(registry, kBox0, kc(bb::KeyCode::ArrowUp)));
    EXPECT_EQ(last_delta, +1);
    EXPECT_TRUE(br::dispatch_focus_key(registry, kBox0, kc(bb::KeyCode::ArrowRight)));
    EXPECT_EQ(last_delta, +1);
    EXPECT_TRUE(br::dispatch_focus_key(registry, kBox0, kc(bb::KeyCode::ArrowDown)));
    EXPECT_EQ(last_delta, -1);
    EXPECT_TRUE(br::dispatch_focus_key(registry, kBox0, kc(bb::KeyCode::ArrowLeft)));
    EXPECT_EQ(last_delta, -1);
}

TEST(DispatchFocusKey, UnregisteredFocusedElementIsNotConsumed) {
    const br::FocusRegistry registry = boxes_and_group();
    EXPECT_FALSE(br::dispatch_focus_key(registry, kUnregistered, kc(bb::KeyCode::Enter)));
}

TEST(DispatchFocusKey, MissingHookIsNotConsumed) {
    // A text box with no activate/adjust hooks (the Z09 case): dispatch is a no-op.
    br::FocusRegistry registry;
    registry.register_element(kBox0, br::FocusableEntry{.is_text_field = true});
    EXPECT_FALSE(br::dispatch_focus_key(registry, kBox0, kc(bb::KeyCode::Enter)));
    EXPECT_FALSE(br::dispatch_focus_key(registry, kBox0, kc(bb::KeyCode::ArrowUp)));
}

TEST(DispatchFocusKey, NonDispatchKeyIsNotConsumed) {
    int touched = 0;
    br::FocusRegistry registry;
    registry.register_element(kBox0,
                              br::FocusableEntry{.is_text_field = false,
                                                 .activate = [&] { ++touched; },
                                                 .adjust = [&](int) { ++touched; }});
    EXPECT_FALSE(br::dispatch_focus_key(registry, kBox0, kc(bb::KeyCode::Tab)));
    EXPECT_FALSE(br::dispatch_focus_key(registry, kBox0, kc(bb::KeyCode::Digit1)));
    EXPECT_EQ(touched, 0);
}
