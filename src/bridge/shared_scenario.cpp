#include "bridge/shared_scenario.hpp"

#include "engine/scenario_id.hpp"

#include <cstddef>
#include <optional>
#include <string_view>

namespace poker_trainer::bridge {

std::optional<std::string_view> extract_query_param(std::string_view query,
                                                    std::string_view key) noexcept {
    if (!query.empty() && query.front() == '?') {
        query.remove_prefix(1);
    }
    while (!query.empty()) {
        std::string_view pair;
        const std::size_t amp = query.find('&');
        if (amp == std::string_view::npos) {
            pair = query;
            query = std::string_view{};
        } else {
            pair = query.substr(0, amp);
            query.remove_prefix(amp + 1);
        }
        const std::size_t eq = pair.find('=');
        const std::string_view k =
            (eq == std::string_view::npos) ? pair : pair.substr(0, eq);
        if (k == key) {
            return (eq == std::string_view::npos) ? std::string_view{}
                                                  : pair.substr(eq + 1);
        }
    }
    return std::nullopt;
}

std::optional<engine::ScenarioId> parse_shared_scenario(
    std::string_view query) noexcept {
    const std::optional<std::string_view> value =
        extract_query_param(query, "scenario");
    if (!value.has_value()) {
        return std::nullopt;
    }
    // engine::parse_scenario_id rejects empty / non-numeric / zero / overflow,
    // each of which falls through to the normal Root flow with no error.
    return engine::parse_scenario_id(*value);
}

}  // namespace poker_trainer::bridge
