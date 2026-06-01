#include "bridge/canvas_sizing.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

namespace poker_trainer::bridge {

namespace {

[[nodiscard]] constexpr char to_lower_ascii(char c) noexcept {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<char>(c - 'A' + 'a');
    }
    return c;
}

// Case-insensitive ASCII substring test. `needle` is expected lowercase.
[[nodiscard]] bool contains_ci(std::string_view haystack,
                               std::string_view needle) noexcept {
    if (needle.empty()) {
        return true;
    }
    if (needle.size() > haystack.size()) {
        return false;
    }
    const std::size_t last = haystack.size() - needle.size();
    for (std::size_t i = 0; i <= last; ++i) {
        bool match = true;
        for (std::size_t j = 0; j < needle.size(); ++j) {
            if (to_lower_ascii(haystack[i + j]) != needle[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }
    return false;
}

// Tokens that mark a User-Agent as a mobile browser. All lowercase so the
// case-insensitive match keys off `haystack` casing only.
constexpr std::array<std::string_view, 11> kMobileTokens{
    "android", "iphone",   "ipad",      "ipod",      "mobile",     "windows phone",
    "blackberry", "iemobile", "opera mini", "webos",  "silk",
};

}  // namespace

CanvasDims canvas_dims_from_viewport(int viewport_w, int viewport_h) noexcept {
    return CanvasDims{std::max(0, viewport_w), std::max(0, viewport_h)};
}

bool is_mobile_user_agent(std::string_view user_agent) noexcept {
    for (const std::string_view token : kMobileTokens) {
        if (contains_ci(user_agent, token)) {
            return true;
        }
    }
    return false;
}

DisplayMode resolve_display_mode(bool is_mobile, int canvas_height) noexcept {
    if (is_mobile) {
        return DisplayMode::Mobile;
    }
    if (is_below_min_size(canvas_height)) {
        return DisplayMode::TooSmall;
    }
    return DisplayMode::Normal;
}

}  // namespace poker_trainer::bridge
