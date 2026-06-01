#include "bridge/shared_scenario.hpp"

#include "engine/scenario_id.hpp"

#include <gtest/gtest.h>

namespace br = poker_trainer::bridge;
namespace eng = poker_trainer::engine;

TEST(SharedScenario, ValidIdParses) {
    const auto id = br::parse_shared_scenario("scenario=4729183746281");
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(*id, eng::ScenarioId{4729183746281ULL});
}

TEST(SharedScenario, MaxUint64Parses) {
    const auto id = br::parse_shared_scenario("scenario=18446744073709551615");
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(id->value, 18446744073709551615ULL);
}

TEST(SharedScenario, NonNumericIsAbsent) {
    EXPECT_FALSE(br::parse_shared_scenario("scenario=abc").has_value());
    EXPECT_FALSE(br::parse_shared_scenario("scenario=1.5").has_value());
    EXPECT_FALSE(br::parse_shared_scenario("scenario=-5").has_value());
}

TEST(SharedScenario, EmptyValueIsAbsent) {
    EXPECT_FALSE(br::parse_shared_scenario("scenario=").has_value());
}

TEST(SharedScenario, ZeroIsAbsent) {
    // 0 is the reserved invalid id.
    EXPECT_FALSE(br::parse_shared_scenario("scenario=0").has_value());
}

TEST(SharedScenario, OverflowIsAbsent) {
    // One past UINT64_MAX.
    EXPECT_FALSE(
        br::parse_shared_scenario("scenario=18446744073709551616").has_value());
}

TEST(SharedScenario, MissingParamIsAbsent) {
    EXPECT_FALSE(br::parse_shared_scenario("").has_value());
    EXPECT_FALSE(br::parse_shared_scenario("foo=1&bar=2").has_value());
}

TEST(SharedScenario, LeadingQuestionMarkAndOtherParams) {
    const auto id = br::parse_shared_scenario("?foo=1&scenario=42&bar=2");
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(*id, eng::ScenarioId{42});
}

TEST(ExtractQueryParam, FindsValueAndHandlesAbsence) {
    EXPECT_EQ(br::extract_query_param("scenario=42", "scenario"),
              std::optional<std::string_view>{"42"});
    EXPECT_EQ(br::extract_query_param("?a=1&scenario=42", "scenario"),
              std::optional<std::string_view>{"42"});
    EXPECT_EQ(br::extract_query_param("scenario=", "scenario"),
              std::optional<std::string_view>{""});
    EXPECT_FALSE(br::extract_query_param("a=1", "scenario").has_value());
    // A key that is a prefix of another key must not match the longer key.
    EXPECT_FALSE(br::extract_query_param("scenarios=42", "scenario").has_value());
}

TEST(BootRoute, ResolvesFromParsedId) {
    EXPECT_EQ(br::resolve_boot_route(eng::ScenarioId{42}),
              br::BootRoute::SharedScenario);
    EXPECT_EQ(br::resolve_boot_route(std::nullopt), br::BootRoute::NormalRoot);
}
