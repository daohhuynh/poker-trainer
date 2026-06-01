#pragma once

#include "engine/scenario_id.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

// Shared-scenario URL entry parsing and boot routing
// (ARCHITECTURE Notes — Shared Scenario URL Entry).
//
// When the app is opened with ?scenario=<id>, Tier 1 still loads normally, but
// after it completes the app bypasses Mode Selection and goes straight to Game
// with that id. A malformed value is ignored and the app falls through to the
// normal Root flow with no error. These pure helpers extract and validate the
// parameter; the emscripten side (boot) reads location.search and feeds it here.

namespace poker_trainer::bridge {

// Extract the raw value of a query parameter from a query string. Accepts an
// optional leading '?', '&'-separated key=value pairs. Returns the value
// substring (which may be empty for "key="), or std::nullopt if the key is
// absent. Matching is exact and case-sensitive on the key.
[[nodiscard]] std::optional<std::string_view> extract_query_param(
    std::string_view query, std::string_view key) noexcept;

// Parse the shared scenario id from a query string: extract the "scenario"
// parameter and validate it as a non-zero 64-bit unsigned integer via
// engine::parse_scenario_id. Returns std::nullopt when the parameter is absent,
// empty, non-numeric, zero, or overflows 64 bits — in every such case the
// caller falls through to the normal Root flow with no error shown.
[[nodiscard]] std::optional<engine::ScenarioId> parse_shared_scenario(
    std::string_view query) noexcept;

// Which entry flow boot should take after Tier 1 completes.
enum class BootRoute : std::uint8_t {
    // Normal flow: render Root, with the tutorial prompt subject to the usual
    // has_seen_tutorial_prompt rule.
    NormalRoot = 0,
    // Shared-scenario flow: bypass Mode Selection, force the Tier 3 load during
    // Tier 1, route toward Game with the shared id, and never show the tutorial
    // prompt for the rest of the session.
    SharedScenario = 1,
};

// Resolve the boot route from the parsed shared-scenario id (present ->
// SharedScenario, absent -> NormalRoot).
[[nodiscard]] constexpr BootRoute resolve_boot_route(
    const std::optional<engine::ScenarioId>& shared) noexcept {
    return shared.has_value() ? BootRoute::SharedScenario : BootRoute::NormalRoot;
}

}  // namespace poker_trainer::bridge
