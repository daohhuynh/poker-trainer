#include "settings/search.hpp"

namespace poker_trainer::settings {

namespace {

[[nodiscard]] char to_lower(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
}

// Case-insensitive substring test: is `needle` contained contiguously in `haystack`?
[[nodiscard]] bool contains_ci(std::string_view haystack, std::string_view needle) noexcept {
    if (needle.empty()) {
        return true;
    }
    if (needle.size() > haystack.size()) {
        return false;
    }
    const std::size_t last = haystack.size() - needle.size();
    for (std::size_t i = 0; i <= last; ++i) {
        std::size_t j = 0;
        for (; j < needle.size(); ++j) {
            if (to_lower(haystack[i + j]) != to_lower(needle[j])) {
                break;
            }
        }
        if (j == needle.size()) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] const SettingEntry* find_entry(SettingId id) noexcept {
    for (const SettingEntry& e : kSettingCatalog) {
        if (e.id == id) {
            return &e;
        }
    }
    return nullptr;
}

}  // namespace

std::string_view section_label(SettingsSection s) noexcept {
    switch (s) {
        case SettingsSection::Gameplay:
            return "Gameplay";
        case SettingsSection::Units:
            return "Units";
        case SettingsSection::Display:
            return "Display";
        case SettingsSection::Audio:
            return "Audio";
        case SettingsSection::Recap:
            return "Recap";
        case SettingsSection::Tomatoes:
            return "Tomatoes";
        case SettingsSection::Account:
            return "Account";
        case SettingsSection::General:
            return "General";
        case SettingsSection::Legal:
            return "Legal";
    }
    return "";
}

std::string_view setting_name(SettingId id) noexcept {
    const SettingEntry* e = find_entry(id);
    return e != nullptr ? e->name : std::string_view{};
}

std::string_view setting_keywords(SettingId id) noexcept {
    const SettingEntry* e = find_entry(id);
    return e != nullptr ? e->keywords : std::string_view{};
}

bool keyword_match(std::string_view keywords, std::string_view query) noexcept {
    return contains_ci(keywords, query);
}

bool setting_visible(SettingId id, std::string_view query) noexcept {
    return keyword_match(setting_keywords(id), query);
}

bool section_has_match(SettingsSection section, std::string_view query) noexcept {
    for (const SettingEntry& e : kSettingCatalog) {
        if (e.section == section && keyword_match(e.keywords, query)) {
            return true;
        }
    }
    return false;
}

bool search_is_empty_result(std::string_view query) noexcept {
    if (query.empty()) {
        return false;
    }
    for (const SettingEntry& e : kSettingCatalog) {
        if (keyword_match(e.keywords, query)) {
            return false;
        }
    }
    return true;
}

}  // namespace poker_trainer::settings
