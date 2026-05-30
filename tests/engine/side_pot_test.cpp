// Side Pot Engine tests: the trigger honors the configured frequency, the
// extremes are exact, and the roll is deterministic from the seed.

#include "engine/side_pot.hpp"

#include "engine/rng_seed.hpp"
#include "engine/scenario_id.hpp"

#include <cstdint>

#include <gtest/gtest.h>

namespace pe = poker_trainer::engine;

namespace {

double observed_rate(float frequency, std::uint64_t n) {
    int hits = 0;
    for (std::uint64_t id = 1; id <= n; ++id) {
        pe::RngSeed seed{pe::ScenarioId{id}};
        if (pe::roll_side_pot(seed.engine(), frequency)) {
            ++hits;
        }
    }
    return static_cast<double>(hits) / static_cast<double>(n);
}

}  // namespace

TEST(SidePot, DefaultFrequencyIsAboutTenPercent) {
    EXPECT_NEAR(observed_rate(0.10f, 20000), 0.10, 0.02);
}

TEST(SidePot, FrequencyZeroNeverTriggers) {
    EXPECT_EQ(observed_rate(0.0f, 5000), 0.0);
}

TEST(SidePot, FrequencyOneAlwaysTriggers) {
    EXPECT_EQ(observed_rate(1.0f, 5000), 1.0);
}

TEST(SidePot, HigherFrequencyTriggersMoreOften) {
    EXPECT_LT(observed_rate(0.10f, 20000), observed_rate(0.30f, 20000));
}

TEST(SidePot, RollIsDeterministic) {
    pe::RngSeed a{pe::ScenarioId{4242}};
    pe::RngSeed b{pe::ScenarioId{4242}};
    EXPECT_EQ(pe::roll_side_pot(a.engine(), 0.5f), pe::roll_side_pot(b.engine(), 0.5f));
}
