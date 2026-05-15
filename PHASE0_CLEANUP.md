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
focus_manager.hpp <cstddef> defect — PHASE0.md §14 Contents block was
missing #include <cstddef> despite declaring `[[nodiscard]] std::size_t
context_depth() noexcept;`. Same shape as the prior §16 and §12 amendments.
Amended in commit 665233c. Source/spec parity restored.
reset_for_testing symbol collision — each of the 6 backbone primitives
(animation_clock, modal_state, screen_state, scenario_events, event_router,
focus_manager) originally defined `void poker_trainer::backbone::reset_for_testing()
noexcept` at namespace scope. Compiling each .cpp in isolation passed, but
linking all 6 together produced 5 duplicate-symbol errors at wasm-ld. The
integration test (prompt 11) was the first session to attempt a combined
link and surfaced the collision. Fixed in commit 958e0bc by renaming each
function per primitive: reset_animation_clock_for_testing,
reset_event_router_for_testing, reset_focus_manager_for_testing,
reset_modal_state_for_testing, reset_scenario_events_for_testing,
reset_screen_state_for_testing. PHASE0.md §17 spec text still references
the original combined `backbone::reset_for_testing()` name and is amended
in PART 3 of this session.
scenario_id.cpp missing — PHASE0.md §1 declared `parse_scenario_id` and
`format_scenario_id` in engine/scenario_id.hpp but no scenario_id.cpp was
ever generated in any prior prompt. The functions are not constexpr/inline-able
(parse does std::from_chars validation, format allocates a std::string), so
they need a translation unit. Symbols were undefined at link time when the
integration test attempted to call them. Fixed in commit 443e158 by adding
src/engine/scenario_id.cpp with std::from_chars-based parse_scenario_id
(returns std::optional<ScenarioId>, std::nullopt on parse failure or
zero/sentinel result) and std::to_string-based format_scenario_id (returns
std::string). The functions are now linkable for all downstream zones
(Module 7's URL parsing, the Copy/Share buttons on Post-Round, IDBFS replay).

## Process lessons (for future contract-generation workflows)

Two of the three defects above (reset_for_testing collision in commit 958e0bc,
missing scenario_id.cpp in commit 443e158) escaped single-file compile
verification because they're link-time failures, not compile-time failures.
Every prior generation session ran `emcc -fsyntax-only` on headers and
`emcc -c` on .cpps in isolation, which proved each translation unit was
internally consistent but said nothing about whether all the TUs could link
together. The integration test in prompt 11 was the first session to attempt
a combined link of every Phase 0 .cpp, which is why these defects surfaced
there and not earlier.

For any future Phase-N-style contract-generation workflow, run a combined
link check after every backbone .hpp + .cpp pair commit, not just at the
final integration test. The shape of the check:

   source /Users/dao/emsdk/emsdk_env.sh >/dev/null 2>&1 && \
     emcc -std=c++23 -Isrc -c src/**/*.cpp -o /tmp/combined_check.o; \
     echo "combined-link-exit=$?"

If combined-link-exit is non-zero, the most recent .cpp introduces either a
duplicate symbol (collision with an existing TU) or an undefined-reference
(calls a function nobody has defined). Catching either incrementally is much
cheaper than catching them at sign-off when 11 prompts of files are already
in play.

A third defect (the <cstddef> shape) was caught earlier because it manifested
as a compile error rather than a link error — single-file verification was
sufficient. That's the easier defect class. The link-error class is the one
to guard against in the future.
