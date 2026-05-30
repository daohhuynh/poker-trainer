# Phase 0 complete

Phase 0 contract generation is finished. Every backbone primitive, persistence header,
engine header, theme header, settings header, and audio/asset/tier/auth0 config header
has been written from the verbatim PHASE0.md spec (or in the case of prose-only sections,
from authoritative design specifications in the generation prompts). Each file compiles
cleanly under emcc with -std=c++23 -Werror and the full warning baseline.

The integration test at `tests/all_headers_test.cpp` exercises every Phase 0 contract:
each backbone primitive's reset_for_testing, representative state mutations, and the
FNV-1a hash correctness check. The test passes.

## Phase 0 contracts are now immutable.

Any change to a Phase 0 header — adding a function, changing a return type, renaming a
constant, modifying a struct field — requires a deliberate contract-amendment process,
not a casual edit. The zone implementations that come online during Phase 1+ build
against these contracts; changing them retroactively breaks every zone simultaneously.

## Remaining cleanup before sign-off seal

See PHASE0_CLEANUP.md for the running cleanup list. As of Phase 0 generation completion:
- Item A: CLAUDE.md §10 vs seqlock retry-loop idiom — affects sync_state.cpp and
  screen_state.cpp. Unresolved; resolution path documented.
- Item B: theme_tokens forward-declaration + std::array — RESOLVED (2026-05-30)
  when Z06 landed and backed the contract with concrete palettes + tests.

## Contract amendments (post-seal)

Phase 0 contracts are immutable except via the deliberate amendment process
described above. Amendments to date:
- 2026-05-30: appended `ColorToken::AccentSecondary` (value 61) to
  `theme_tokens.hpp`; `Count` moved 61 -> 62. Strictly additive — no
  pre-existing token renumbered, no other contract header touched,
  `kFixedAcrossThemeTokens` unchanged. ARCHITECTURE mandates accent_secondary
  and the Leaderboard (Z11/Z13) requires it. Z06 populates it in all four
  palettes. Full record in PHASE0_CLEANUP.md ("Post-seal contract amendments").

## Commit reference

Phase 0 generation spans commits from the initial scaffolding through this PHASE0_COMPLETE
marker. The integration test (`add phase 0 integration test`) is the last work commit
before this marker.

Add the §14 cstddef amendment (commit 665233c) to PHASE0_CLEANUP.md's "Items resolved
during Phase 0 generation" section in a follow-up housekeeping session. It is not on
the unresolved list — it was resolved during generation, just not yet documented in
the resolved-list section.
