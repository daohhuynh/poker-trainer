// Zone 05 — per-frame tick registry: registration-order invocation, per-frame
// re-invocation, and clear. The clock-before-callbacks invariant is owned by the
// main loop (frame() calls backbone::tick before run_frame_ticks) and is not
// reachable here without the wasm loop, so this covers only the registry's pure
// logic.

#include "bridge/frame_tick.hpp"

#include <vector>

#include <gtest/gtest.h>

namespace br = poker_trainer::bridge;

namespace {

class FrameTickTest : public ::testing::Test {
protected:
    void SetUp() override { br::clear_frame_ticks(); }
    void TearDown() override { br::clear_frame_ticks(); }
};

TEST_F(FrameTickTest, RegisteredCallbackRunsOnEachFrame) {
    int calls = 0;
    br::register_frame_tick([&calls] { ++calls; });

    br::run_frame_ticks();
    EXPECT_EQ(calls, 1);

    br::run_frame_ticks();
    br::run_frame_ticks();
    EXPECT_EQ(calls, 3);  // once per frame, not once total
}

TEST_F(FrameTickTest, CallbacksRunInRegistrationOrder) {
    std::vector<int> order;
    br::register_frame_tick([&order] { order.push_back(1); });
    br::register_frame_tick([&order] { order.push_back(2); });
    br::register_frame_tick([&order] { order.push_back(3); });

    br::run_frame_ticks();

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

TEST_F(FrameTickTest, NoTicksIsANoOp) {
    br::run_frame_ticks();  // empty registry: must not crash
    SUCCEED();
}

TEST_F(FrameTickTest, ClearRemovesAllTicks) {
    int calls = 0;
    br::register_frame_tick([&calls] { ++calls; });
    br::clear_frame_ticks();

    br::run_frame_ticks();
    EXPECT_EQ(calls, 0);
}

TEST_F(FrameTickTest, EmptyCallbackIsIgnored) {
    br::register_frame_tick(br::FrameTickFn{});  // null target: dropped, not invoked
    br::run_frame_ticks();                       // must not call through a null std::function
    SUCCEED();
}

}  // namespace
