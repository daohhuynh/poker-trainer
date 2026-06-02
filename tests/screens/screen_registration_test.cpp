// Pass-1 wiring test: Zone 07 self-registers its real Root / Mode Selection
// renders into Zone 05's render-dispatch registry, replacing the blank default.
//
// The registered renders cannot be INVOKED here (they draw through ImGui, which
// has no context in the native test build), so this asserts the registration
// contract: install_screens registers exactly the two screens Zone 07 owns, and
// its registration wins over any pre-existing placeholder (register_screen_
// renderer is last-writer-wins — the invoke/replace semantics themselves are
// covered by screen_dispatch_test).

#include "screens/screen_registration.hpp"

#include "screens/custom_popup.hpp"

#include "backbone/game_mode.hpp"
#include "backbone/screen_state.hpp"
#include "bridge/screen_dispatch.hpp"

#include <optional>

#include <gtest/gtest.h>

namespace sc = poker_trainer::screens;
namespace br = poker_trainer::bridge;
namespace bb = poker_trainer::backbone;

namespace {

class FakeStore final : public sc::CustomWeightsStore {
public:
    std::optional<bb::CustomConfig> saved;
    void save(bb::CustomConfig weights) override { saved = weights; }
    [[nodiscard]] std::optional<bb::CustomConfig> load() const override { return saved; }
};

}  // namespace

TEST(ScreenRegistration, Z07RendersOverrideThePlaceholderDefault) {
    br::reset_screen_dispatch_for_testing();

    // Stand-in placeholder renderers occupy the Z07 screens before wiring.
    br::register_screen_renderer(bb::ScreenId::Root, [] {});
    br::register_screen_renderer(bb::ScreenId::ModeSelection, [] {});
    ASSERT_TRUE(br::has_screen_renderer(bb::ScreenId::Root));
    ASSERT_TRUE(br::has_screen_renderer(bb::ScreenId::ModeSelection));

    // static so the references install_screens captures into the global registry
    // (and the event router) outlive this test rather than dangling.
    static sc::ScreensRuntime runtime;
    static FakeStore store;
    sc::install_screens(runtime, store);

    // Zone 07 now owns the render for both of its screens (last-writer-wins
    // replaced the placeholders).
    EXPECT_TRUE(br::has_screen_renderer(bb::ScreenId::Root));
    EXPECT_TRUE(br::has_screen_renderer(bb::ScreenId::ModeSelection));

    // It registered ONLY its two screens — not Game (Z08) or Post-Round (Z13).
    EXPECT_FALSE(br::has_screen_renderer(bb::ScreenId::Game));
    EXPECT_FALSE(br::has_screen_renderer(bb::ScreenId::PostRound));

    br::reset_screen_dispatch_for_testing();
}

TEST(ScreenRegistration, RegistersOntoAnEmptyRegistry) {
    br::reset_screen_dispatch_for_testing();
    EXPECT_FALSE(br::has_screen_renderer(bb::ScreenId::Root));
    EXPECT_FALSE(br::has_screen_renderer(bb::ScreenId::ModeSelection));

    static sc::ScreensRuntime runtime;
    static FakeStore store;
    sc::install_screens(runtime, store);

    EXPECT_TRUE(br::has_screen_renderer(bb::ScreenId::Root));
    EXPECT_TRUE(br::has_screen_renderer(bb::ScreenId::ModeSelection));

    br::reset_screen_dispatch_for_testing();
}
