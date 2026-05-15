Phase 0 sign-off cleanup list
This file tracks items that must be resolved before Phase 0 is signed off and
the Phase 0 contract becomes immutable. Each item documents the issue, the
files affected, and the resolution options.
A. CLAUDE.md §10 vs seqlock retry-loop idiom
Affected files: src/persistence/sync_state.cpp, src/backbone/screen_state.cpp
Issue: Both files implement a seqlock-style read protocol using
while (true) { ... return result; }. The loop exits via return rather than
via a commented break. CLAUDE.md §10 prohibits while (true) without an
explicit break inside a comment-explained condition.
Why this exists: PHASE0.md §3 (sync_state) and §12 (screen_state) Notes
contain verbatim code that uses this shape. The verbatim-spec rule for Phase 0
takes precedence over the lint rule, so the code was committed as-is and the
tension was deferred to sign-off.
Resolution options (choose one before sign-off):

(a) Amend CLAUDE.md §10 to carve out seqlock retry-loop idioms (e.g., "loops
that consist of a single retry block exiting via return on consistent
read are permitted, provided the retry shape is commented at the top of the
function").
(b) Restructure both files to use a flag-based exit:

  ScreenStateSnapshot result;
  bool consistent = false;
  while (!consistent) {
      const auto v1 = storage().version.load(std::memory_order_acquire);
      if (v1 & 1) continue;  // Write in progress, retry.
      result = storage().snapshot;
      const auto v2 = storage().version.load(std::memory_order_acquire);
      if (v1 == v2) consistent = true;
  }
  return result;
Apply to both sync_state.cpp and screen_state.cpp.
Either resolution is acceptable. User decides at sign-off.
B. theme_tokens forward-declaration + std::array
Affected files: src/theme/theme_tokens.hpp
Issue: theme_tokens.hpp forward-declares the per-theme token bundle and
exposes it via a std::array indexed by theme ID. The forward declaration is
resolved by the actual theme definitions, which live in Z06 (Theme zone) and
do not yet exist.
Why this exists: Phase 0 defines the contract surface (the array, the
token structure, the access functions); Z06 fills in the values. This is
correct by design — the cleanup item is only that the contract surface can't
be exercised in an integration test until Z06 lands.
Resolution: None required for Phase 0 sign-off as long as the integration
test in tests/all_headers_test.cpp only verifies that theme_tokens.hpp
includes cleanly. When Z06 lands (post-Phase-0), the test can be extended to
verify the array population and the access functions return expected values.
Items resolved during Phase 0 generation (for reference)

modal_state.hpp <cstddef> defect — PHASE0.md §16 Contents block was
missing #include <cstddef> despite using std::size_t. Amended in commit
af9bb0d. Source/spec parity restored.
screen_state.hpp <cstddef> defect — same shape as above, PHASE0.md §12.
Amended in commit bc41dfe. Source/spec parity restored.
Settings consumers prose — PHASE0.md §10 consumers list used
time_pressure_seconds (old name) instead of time_pressure_custom_seconds
(current name). Amended in commit 2455634. Source/spec parity restored.
