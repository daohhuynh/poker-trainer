// Zone 12 — Settings search unit tests: the case-insensitive SUBSTRING matcher,
// per-control visibility, per-section match, the empty-result state, the Bug-B
// acceptance examples (hits + the cash->background non-match), and catalog
// completeness (every section / control / option / sub-label resolves).

#include "settings/search.hpp"

#include <gtest/gtest.h>

namespace st = poker_trainer::settings;

TEST(KeywordMatch, EmptyQueryMatchesEverything) {
    EXPECT_TRUE(st::keyword_match("audio volume", ""));
    EXPECT_TRUE(st::keyword_match("", ""));
}

TEST(KeywordMatch, ContiguousSubstringHits) {
    EXPECT_TRUE(st::keyword_match("audio volume loudness level", "volume"));   // whole word
    EXPECT_TRUE(st::keyword_match("audio volume loudness level", "vol"));      // prefix
    EXPECT_TRUE(st::keyword_match("audio volume loudness level", "lume lou")); // spanning infix
}

TEST(KeywordMatch, CaseInsensitive) {
    EXPECT_TRUE(st::keyword_match("display color colour theme", "THEME"));
    EXPECT_TRUE(st::keyword_match("DISPLAY COLOR THEME", "color"));
}

TEST(KeywordMatch, SubsequenceIsNotASubstring) {
    // The old subsequence matcher accepted "vlm" inside "volume"; substring does not.
    EXPECT_FALSE(st::keyword_match("audio volume", "vlm"));
    // And "cash" is a SUBSEQUENCE of "background atmospheric movement" but not a
    // substring — the exact false positive Bug B reported.
    EXPECT_FALSE(st::keyword_match("display background atmospheric movement ambient", "cash"));
}

TEST(Catalog, NamesLabelsAndKeywords) {
    EXPECT_EQ(st::setting_name(st::SettingId::Volume), "Volume");
    EXPECT_EQ(st::setting_name(st::SettingId::ResetAll), "Reset all settings");
    EXPECT_EQ(st::section_label(st::SettingsSection::Audio), "Audio");
    EXPECT_EQ(st::section_label(st::SettingsSection::Gameplay), "Gameplay");
    EXPECT_FALSE(st::setting_keywords(st::SettingId::Volume).empty());
}

// ----- Bug B acceptance examples -----

TEST(BugB, FlopReturnsStreetSplitWeights) {
    EXPECT_TRUE(st::setting_visible(st::SettingId::StreetWeights, "flop"));
    EXPECT_TRUE(st::section_has_match(st::SettingsSection::Gameplay, "flop"));
    EXPECT_FALSE(st::search_is_empty_result("flop"));
}

TEST(BugB, GameplaySectionTermHasParityWithAccount) {
    // "account" (a section term present in the catalog) hit before the fix; "gameplay"
    // (also a section term) missed. With section names in every blob, both resolve.
    EXPECT_TRUE(st::section_has_match(st::SettingsSection::Account, "account"));
    EXPECT_TRUE(st::section_has_match(st::SettingsSection::Gameplay, "gameplay"));
    // A representative Gameplay control is visible under the section query.
    EXPECT_TRUE(st::setting_visible(st::SettingId::ShowHud, "gameplay"));
}

TEST(BugB, FixedAndFixedChipDenominationsResolveChipMode) {
    EXPECT_TRUE(st::setting_visible(st::SettingId::ChipDenomination, "fixed"));
    EXPECT_TRUE(st::setting_visible(st::SettingId::ChipDenomination, "fixed chip denominations"));
    // "fixed" must not leak into unrelated entries.
    EXPECT_FALSE(st::setting_visible(st::SettingId::BackgroundMovement, "fixed"));
}

TEST(BugB, CashReturnsUnitsButNotBackgroundMovement) {
    EXPECT_TRUE(st::setting_visible(st::SettingId::UnitToggle, "cash"));
    EXPECT_FALSE(st::setting_visible(st::SettingId::BackgroundMovement, "cash"));  // no false positive
}

TEST(BugB, NonsenseQueryIsEmptyResult) {
    EXPECT_TRUE(st::search_is_empty_result("zzzzzqx"));
    EXPECT_FALSE(st::search_is_empty_result(""));        // empty query => full list
    EXPECT_FALSE(st::search_is_empty_result("volume"));  // a real hit
}

// ----- per-control / per-section helpers -----

TEST(SettingVisible, FollowsKeywordMatch) {
    EXPECT_TRUE(st::setting_visible(st::SettingId::Volume, "vol"));
    EXPECT_TRUE(st::setting_visible(st::SettingId::Volume, ""));
    EXPECT_FALSE(st::setting_visible(st::SettingId::Volume, "zzz"));
}

TEST(SectionHasMatch, AnyControlInSection) {
    EXPECT_TRUE(st::section_has_match(st::SettingsSection::Audio, "mute"));   // 3 mute controls
    EXPECT_TRUE(st::section_has_match(st::SettingsSection::Audio, ""));       // empty matches all
    EXPECT_FALSE(st::section_has_match(st::SettingsSection::Audio, "zzz"));
    EXPECT_FALSE(st::section_has_match(st::SettingsSection::Units, "particle"));  // particle is Display
}

// ----- catalog completeness -----

TEST(CatalogCompleteness, EverySectionNameResolvesToItsSection) {
    // No "account hits / gameplay misses" asymmetry: each section's display name must
    // match at least one control IN that section (its blobs all carry the section name).
    for (std::size_t i = 0; i < st::kSectionCount; ++i) {
        const auto section = static_cast<st::SettingsSection>(i);
        EXPECT_TRUE(st::section_has_match(section, st::section_label(section)))
            << "section " << st::section_label(section) << " name did not resolve";
    }
}

TEST(CatalogCompleteness, EveryControlCarriesItsSectionAndKeywords) {
    // Each control individually carries its section name in its keyword blob, so the
    // "account hits / gameplay misses" asymmetry cannot recur for any single control: a
    // section query resolves EVERY control in that section. (Display names are NOT
    // required to be verbatim substrings — the blob is a normalized keyword set with
    // punctuation stripped, synonyms added, and tokens reordered.)
    for (const st::SettingEntry& e : st::kSettingCatalog) {
        EXPECT_FALSE(e.keywords.empty()) << "control \"" << e.name << "\" has no keywords";
        EXPECT_TRUE(st::keyword_match(e.keywords, st::section_label(e.section)))
            << "control \"" << e.name << "\" does not carry its section name";
    }
}

TEST(CatalogCompleteness, OptionValuesAndSubLabelsResolve) {
    // Option values.
    EXPECT_TRUE(st::setting_visible(st::SettingId::UnitToggle, "big blinds"));
    EXPECT_TRUE(st::setting_visible(st::SettingId::ChipDenomination, "stake-scaled"));
    EXPECT_TRUE(st::setting_visible(st::SettingId::Theme, "slate"));
    EXPECT_TRUE(st::setting_visible(st::SettingId::Theme, "ocean"));
    EXPECT_TRUE(st::setting_visible(st::SettingId::MusicType, "bossa nova"));
    EXPECT_TRUE(st::setting_visible(st::SettingId::DefaultRecapTab, "summary"));
    // Street sub-labels.
    EXPECT_TRUE(st::setting_visible(st::SettingId::StreetWeights, "pre-flop"));
    EXPECT_TRUE(st::setting_visible(st::SettingId::StreetWeights, "turn"));
    EXPECT_TRUE(st::setting_visible(st::SettingId::StreetWeights, "river"));
}
