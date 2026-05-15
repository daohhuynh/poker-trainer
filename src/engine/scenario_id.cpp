#include "engine/scenario_id.hpp"

#include <charconv>
#include <string>
#include <string_view>
#include <system_error>

namespace poker_trainer::engine {

namespace {

constexpr bool is_ws(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r'
        || c == '\f' || c == '\v';
}

constexpr std::string_view trim(std::string_view s) noexcept {
    while (!s.empty() && is_ws(s.front())) s.remove_prefix(1);
    while (!s.empty() && is_ws(s.back())) s.remove_suffix(1);
    return s;
}

}  // namespace

std::optional<ScenarioId> parse_scenario_id(std::string_view input) noexcept {
    const std::string_view trimmed = trim(input);
    if (trimmed.empty()) return std::nullopt;

    // The spec explicitly rejects leading '-' and decimal points. std::from_chars
    // on an unsigned type already rejects '-', but checking up front gives a
    // clearer rejection point and matches the header's documented behavior.
    if (trimmed.front() == '-' || trimmed.front() == '+') return std::nullopt;

    std::uint64_t value = 0;
    const char* const first = trimmed.data();
    const char* const last = trimmed.data() + trimmed.size();
    const auto result = std::from_chars(first, last, value);
    if (result.ec != std::errc{}) return std::nullopt;
    // Reject trailing non-digit characters: "1.5", "5abc", "42 99" all fail here.
    if (result.ptr != last) return std::nullopt;
    if (value < kMinScenarioIdValue) return std::nullopt;
    return ScenarioId{value};
}

std::string format_scenario_id(ScenarioId id) {
    return std::to_string(id.value);
}

}  // namespace poker_trainer::engine
