#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace poker_trainer::engine {

// A strong type wrapper around a 64-bit unsigned integer that identifies
// a scenario. Defined as a struct (not a typedef or using-alias) to prevent
// accidental conversion to/from raw uint64_t.
struct ScenarioId {
    std::uint64_t value{};

    // Equality and ordering. Defaulted spaceship gives total ordering so
    // ScenarioIds may be used as keys in std::map and std::set.
    constexpr bool operator==(const ScenarioId&) const noexcept = default;
    constexpr auto operator<=>(const ScenarioId&) const noexcept = default;
};

// Sentinel value representing an uninitialized or invalid Scenario ID.
// The engine never generates an ID with value 0. Saved-scenario records
// that contain value 0 are treated as corrupt entries.
inline constexpr ScenarioId kInvalidScenarioId{0};

// The minimum valid Scenario ID. ID 0 is reserved as invalid.
inline constexpr std::uint64_t kMinScenarioIdValue = 1;

// The maximum valid Scenario ID. All 64 bits except 0 are valid.
inline constexpr std::uint64_t kMaxScenarioIdValue = ~std::uint64_t{0};

// Parse a Scenario ID from a string (typically a URL query parameter or
// clipboard paste). Returns std::nullopt if the input is not a valid 64-bit
// unsigned integer, or if it represents 0 (the reserved invalid value).
//
// The expected input format is a decimal number with no separators, no
// leading sign, optionally surrounded by whitespace. Examples:
//   "4729183746281"  -> ScenarioId{4729183746281}
//   "  42  "         -> ScenarioId{42}
//   "0"              -> std::nullopt (reserved invalid)
//   "abc"            -> std::nullopt
//   "-5"             -> std::nullopt
//   "1.5"            -> std::nullopt
//   ""               -> std::nullopt
//   "18446744073709551616" -> std::nullopt (overflow past UINT64_MAX)
std::optional<ScenarioId> parse_scenario_id(std::string_view input) noexcept;

// Format a Scenario ID as its decimal string representation, suitable for
// display in the Post-Round Screen or for clipboard copy.
//
// The returned string contains only ASCII digits, no padding, no separators.
// Example: ScenarioId{4729183746281} -> "4729183746281".
//
// Returns the string "0" if called on kInvalidScenarioId. Callers should
// not pass invalid IDs; this behavior exists only to avoid undefined output.
std::string format_scenario_id(ScenarioId id);

// Returns true if the given ID is non-zero (and therefore valid).
[[nodiscard]] constexpr bool is_valid(ScenarioId id) noexcept {
    return id.value >= kMinScenarioIdValue;
}

}  // namespace poker_trainer::engine
