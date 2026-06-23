#pragma once

#include <string>
#include <string_view>

// Zone 12 — client-side username denylist (ARCHITECTURE "Account Creation Flow",
// "Username filtering" / "Username denylist enforcement order").
//
// Display names are validated against a profanity + slur denylist BEFORE the Sign Up
// form submits to Auth0; a match rejects the form without ever reaching Auth0. The
// match is case-insensitive and normalizes common letter substitutions / leetspeak
// (so "b4dw0rd" still matches "badword") before a substring test against the list.
//
// This is the PURE core (no ImGui): normalize_for_denylist + is_username_denylisted
// are unit-tested directly. The render glue that calls them lives in account_modal.cpp.
//
// SEAM(denylist source): the compiled-in list below is a small INNOCUOUS placeholder
// (deliberately not real slurs — those are not committed here). The launch list must
// be the publicly maintained LDNOOBW English list (or equivalent), vendored by Dao and
// dropped in over kDenylistTerms (see username_denylist.cpp). The matching mechanism is
// final; only the word data is pending.

namespace poker_trainer::settings {

// Canonicalize a name for matching: lowercased, with common leetspeak digits/symbols
// folded to their letter (0->o, 1->i, 3->e, 4->a, 5->s, 7->t, @->a, $->s, !->i, +->t),
// and every non-alphanumeric character stripped (spaces, underscores, punctuation).
// The denylist terms are normalized by the same function so the comparison is apples
// to apples. Returns the normalized string (may be empty if `raw` held no alphanumerics).
[[nodiscard]] std::string normalize_for_denylist(std::string_view raw);

// True when the normalized `username` CONTAINS any denylisted term as a substring
// (the spec rejects names "containing matches"). Empty / all-symbol names normalize to
// empty and never match (they are caught by the form's empty-field validation instead).
[[nodiscard]] bool is_username_denylisted(std::string_view username);

}  // namespace poker_trainer::settings
