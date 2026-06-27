#include "settings/username_denylist.hpp"

#include "settings/username_denylist_data.hpp"

#include <cctype>
#include <string>
#include <string_view>

namespace poker_trainer::settings {

namespace {

// Fold a single character to its denylist-canonical form, or '\0' to drop it. Letters
// lowercase; common leetspeak digits/symbols map to their letter; everything else is
// dropped so separators and punctuation cannot hide a match ("b a d" -> "bad").
[[nodiscard]] char fold_char(char c) noexcept {
    switch (c) {
        case '0': return 'o';
        case '1': return 'i';
        case '3': return 'e';
        case '4': return 'a';
        case '5': return 's';
        case '7': return 't';
        case '@': return 'a';
        case '$': return 's';
        case '!': return 'i';
        case '+': return 't';
        default: break;
    }
    const unsigned char uc = static_cast<unsigned char>(c);
    if (std::isalpha(uc) != 0) {
        return static_cast<char>(std::tolower(uc));
    }
    if (std::isdigit(uc) != 0) {
        return c;  // a digit with no leet meaning still counts as content (e.g. "user2")
    }
    return '\0';  // drop separators / punctuation / control
}

// The denylist terms live in the generated header username_denylist_data.hpp
// (detail::kDenylistTermsData), compiled from en.txt (the LDNOOBW English list) and
// already normalize_for_denylist()-folded so the substring test below is a straight
// comparison. The matching mechanism is unchanged from the placeholder era — only the
// data source moved from an inline array to the generated header.

}  // namespace

std::string normalize_for_denylist(std::string_view raw) {
    std::string out;
    out.reserve(raw.size());
    for (const char c : raw) {
        const char folded = fold_char(c);
        if (folded != '\0') {
            out.push_back(folded);
        }
    }
    return out;
}

bool is_username_denylisted(std::string_view username) {
    const std::string normalized = normalize_for_denylist(username);
    if (normalized.empty()) {
        return false;
    }
    for (const std::string_view term : detail::kDenylistTermsData) {
        if (!term.empty() && normalized.find(term) != std::string::npos) {
            return true;
        }
    }
    return false;
}

}  // namespace poker_trainer::settings
