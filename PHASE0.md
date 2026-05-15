# PHASE0.md

This document is the master inventory for Phase 0 of the Poker Trainer project. Every file produced during Phase 0 is specified here, with exact type signatures, struct fields, enum values, defaults, dependencies, and consumers. Claude Code reads this document when generating Phase 0 files; the prompts that invoke Claude Code are thin pointers to specific sections of this document.

This document is the authority on Phase 0 deliverables. If a question is about what's in a Phase 0 header, the answer is in this document. If this document is silent or ambiguous on something, ask the user — do not invent.

---

## Purpose

Phase 0 produces the foundational headers that every subsequent zone depends on. These headers define the *contracts* between zones: the type shapes, the interfaces, the constants. They are deliberately interface-heavy and implementation-light. The goal is to lock the cross-zone surface so that Waves 1 through 5 can be built in parallel within each wave without coordination overhead.

Phase 0 is structurally distinct from later waves in three ways:

- **Headers first.** Most Phase 0 deliverables are headers only. Implementations come later in the zone that owns them.
- **Contract immutability.** Once Phase 0 is signed off, the headers in this document are immutable. Changing a Phase 0 header is a breaking change to every dependent zone.
- **No business logic.** Phase 0 contains type definitions, enum values, constants, function signatures, and trivial accessors. It does not contain scenario generation logic, rendering logic, audio logic, or any other zone-specific behavior.

---

## Scope

### What Phase 0 produces

Sixteen header files plus one integration test:

1. `src/engine/scenario_id.hpp`
2. `src/engine/rng_seed.hpp`
3. `src/persistence/auth0_config.hpp`
4. `src/persistence/sync_state.hpp`
5. `src/audio/audio_paths.hpp`
6. `src/assets/tier_config.hpp`
7. `src/assets/asset_paths.hpp`
8. `src/theme/theme_tokens.hpp`
9. `src/persistence/persistence_schema.hpp`
10. `src/settings/settings.hpp`
11. `src/backbone/animation_clock.hpp` (interface only — implementation in Z05)
12. `src/backbone/screen_state.hpp`
13. `src/backbone/event_router.hpp`
14. `src/backbone/focus_manager.hpp`
15. `src/backbone/scenario_events.hpp`
16. `src/backbone/modal_state.hpp` (interface only — implementation in Z11)

Plus the integration test:

- `tests/all_headers_test.cpp`

### What Phase 0 does NOT produce

The following are explicitly deferred to later waves:

- Any `.cpp` implementation file for a Phase 0 header, with two exceptions:
  - Backbone primitives with implementations in Phase 0 (event router, focus manager, scenario events bus, screen state singleton) get `.cpp` files in Phase 0 with minimal stub implementations sufficient for the integration test to link. Full implementations are still in their owning zones (event router and focus manager logic is part of Z05's main loop; scenario events bus dispatch happens during normal frame processing).
  - Modal state observer (`modal_state.hpp`) and animation clock (`animation_clock.hpp`) are **interface only** in Phase 0. No `.cpp` files. Implementations live in Z11 and Z05 respectively.
- Zone-specific logic: scenario generation (Z01), asset loading (Z02), audio playback (Z03), persistence reading/writing (Z04), main loop driving (Z05), theme application (Z06), screen rendering (Z07-Z08, Z13), math interrogator behavior (Z09), temporal tracking (Z10), modal rendering (Z11), settings UI (Z12), tutorial overlay (Z14).
- Asset binary files (PNG, audio). Path constants are produced in Phase 0; the actual binary files are sourced separately.

---

## Build order within Phase 0

Phase 0 files depend on each other through includes. The order below is the safe generation order — each file depends only on files generated before it in this list.

1. `scenario_id.hpp` (no dependencies)
2. `rng_seed.hpp` (depends on scenario_id.hpp)
3. `auth0_config.hpp` (no dependencies)
4. `sync_state.hpp` (no dependencies)
5. `audio_paths.hpp` (no dependencies)
6. `tier_config.hpp` (no dependencies)
7. `asset_paths.hpp` (depends on tier_config.hpp)
8. `theme_tokens.hpp` (no dependencies)
9. `persistence_schema.hpp` (depends on scenario_id.hpp, sync_state.hpp)
10. `settings.hpp` (depends on theme_tokens.hpp)
11. `animation_clock.hpp` (no dependencies)
12. `screen_state.hpp` (depends on scenario_id.hpp)
13. `event_router.hpp` (depends on screen_state.hpp)
14. `focus_manager.hpp` (depends on event_router.hpp)
15. `scenario_events.hpp` (depends on scenario_id.hpp)
16. `modal_state.hpp` (no dependencies beyond standard library)
17. `tests/all_headers_test.cpp` (depends on all of the above)

Files in this list with no dependencies on prior list entries can be generated in any order or in parallel. Files with dependencies must be generated after their dependencies exist.

---

## Conventions reference

All Phase 0 files follow the coding conventions documented in CLAUDE.md section 4:

- C++23 strict
- `.hpp` extension for headers, `#pragma once` for include guards
- snake_case for variables, functions, file names
- PascalCase for types, structs, enums, enum classes
- kPascalCase for constants and constexpr values
- Forward-declare aggressively
- `enum class` with explicit underlying type, never plain `enum`
- `std::optional`, `std::expected`, `std::string_view`, `std::span` preferred over older equivalents
- Includes ordered: own header, same-zone headers, backbone headers, std library, third-party, other zones

The architectural rules in CLAUDE.md section 5 apply throughout. No hardcoded color values (theme_tokens.hpp defines the enum; palette implementations come later in Z06). No global mutable state outside the backbone primitives that explicitly own it.

---

## Sign-off criteria

Phase 0 is complete when all of the following pass:

1. All 16 header files exist at the paths listed above with the contents specified in this document.
2. The four backbone primitives that have Phase 0 implementations (event router, focus manager, scenario events bus, screen state singleton) have `.cpp` stub files in `src/backbone/` that compile and link.
3. The integration test `tests/all_headers_test.cpp` exists, includes every Phase 0 header, instantiates one of each declared type or accesses one of each declared constant, and compiles and links cleanly.
4. The integration test passes when run via `ctest --output-on-failure`.
5. A clean build from scratch (`rm -rf build && mkdir build && cd build && emcmake cmake .. && emmake make -j`) produces zero warnings and zero errors.
6. A native debug build with tests (`mkdir build-debug && cd build-debug && cmake -DENABLE_TESTS=ON .. && make -j`) builds, links, and passes ctest.

After sign-off, the headers in this document become immutable contracts. Changes require explicit user approval and a coordinated update across affected zones.

---

# File specifications

The remainder of this document specifies each Phase 0 file in detail. Each file section uses the following template:

- **File path** — exact path from repository root
- **Purpose** — one to two paragraphs on what the file is for and why it exists
- **Dependencies** — Phase 0 files this file includes
- **Consumers** — zones that depend on this file, with brief notes on what each consumer needs
- **Contents** — every type, enum, constant, struct, and function signature with exact details
- **Notes** — special considerations, invariants, defaults, interface-vs-implementation flags, anything subtle

---

## 1. src/engine/scenario_id.hpp

### Purpose

Defines the `ScenarioId` type: a strong 64-bit unsigned integer that identifies a scenario. The Scenario ID is the seed for all randomness that defines the fundamental identity of a scenario (hole cards, community board, position, scenario type, side-pot status, stack sizes). Given the same `ScenarioId`, the engine produces the same scenario every time.

The Scenario ID is the contract that enables three product features at near-zero storage cost: reproducibility for debugging, replay under different settings, and shareable scenarios via URL parameter. Every saved scenario in IDBFS is stored as a `ScenarioId` (8 bytes) plus minor metadata, so the format must remain stable across versions to preserve replay compatibility.

### Dependencies

None. This is the root of the engine type system.

### Consumers

- Z01 (Engine): uses `ScenarioId` as the seed for `RngSeed` (rng_seed.hpp). Generates new IDs at scenario start. Reconstructs scenario state from a given ID.
- Z04 (Persistence): stores `ScenarioId` values in IDBFS as part of saved scenario metadata.
- Z05 (Bridge): parses `ScenarioId` from the URL query string for shared-scenario entry.
- Z13 (Post-Round): displays `ScenarioId` as a 64-bit decimal number in the top-left of the Post-Round Screen, and copies/shares it via the Copy and Share buttons.

### Contents

```cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace poker_trainer::engine {

// A strong type wrapper around a 64-bit unsigned integer that identifies
// a scenario. Defined as a struct (not a typedef or using-alias) to prevent
// accidental conversion to/from raw uint64_t.
struct ScenarioId {
    std::uint64_t value{};

    // Equality and ordering. Defaulted spaceship gives total ordering so
    // ScenarioIds may be used as keys in std::map and std::set.
    constexpr bool operator==(const ScenarioId&) const noexcept = default;
    constexpr auto operator<=>(const ScenarioId&) const noexcept = default;
};

// Sentinel value representing an uninitialized or invalid Scenario ID.
// The engine never generates an ID with value 0. Saved-scenario records
// that contain value 0 are treated as corrupt entries.
inline constexpr ScenarioId kInvalidScenarioId{0};

// The minimum valid Scenario ID. ID 0 is reserved as invalid.
inline constexpr std::uint64_t kMinScenarioIdValue = 1;

// The maximum valid Scenario ID. All 64 bits except 0 are valid.
inline constexpr std::uint64_t kMaxScenarioIdValue = ~std::uint64_t{0};

// Parse a Scenario ID from a string (typically a URL query parameter or
// clipboard paste). Returns std::nullopt if the input is not a valid 64-bit
// unsigned integer, or if it represents 0 (the reserved invalid value).
//
// The expected input format is a decimal number with no separators, no
// leading sign, optionally surrounded by whitespace. Examples:
//   "4729183746281"  -> ScenarioId{4729183746281}
//   "  42  "         -> ScenarioId{42}
//   "0"              -> std::nullopt (reserved invalid)
//   "abc"            -> std::nullopt
//   "-5"             -> std::nullopt
//   "1.5"            -> std::nullopt
//   ""               -> std::nullopt
//   "18446744073709551616" -> std::nullopt (overflow past UINT64_MAX)
std::optional<ScenarioId> parse_scenario_id(std::string_view input) noexcept;

// Format a Scenario ID as its decimal string representation, suitable for
// display in the Post-Round Screen or for clipboard copy.
//
// The returned string contains only ASCII digits, no padding, no separators.
// Example: ScenarioId{4729183746281} -> "4729183746281".
//
// Returns the string "0" if called on kInvalidScenarioId. Callers should
// not pass invalid IDs; this behavior exists only to avoid undefined output.
std::string format_scenario_id(ScenarioId id);

// Returns true if the given ID is non-zero (and therefore valid).
[[nodiscard]] constexpr bool is_valid(ScenarioId id) noexcept {
    return id.value >= kMinScenarioIdValue;
}

}  // namespace poker_trainer::engine
```

### Notes

- `ScenarioId` is a struct with a single `value` field rather than a `using ScenarioId = std::uint64_t;` typedef. The struct wrapper prevents implicit conversion from arbitrary `uint64_t` values, which is the entire point — accidentally passing a raw integer where a `ScenarioId` is expected becomes a compile error.
- The defaulted spaceship operator (`<=>`) gives total ordering, which matters because Scenario IDs may be stored in `std::map` or `std::set` for deduplication of saved scenarios.
- `kInvalidScenarioId` is the only `ScenarioId` value reserved as invalid. The convention is that 0 is never generated by the engine; if a saved-scenario record has value 0, it's a corrupt entry and should be discarded by Z04.
- `parse_scenario_id` is `noexcept` and returns `std::optional<ScenarioId>`. It does not throw on parse failure. Z05 uses this to validate URL parameters; per the architecture spec, malformed scenario URL parameters are silently ignored and the app falls through to the standard Root screen entry flow.
- `format_scenario_id` returns a `std::string` (allocates). This is acceptable because formatting only happens at Post-Round entry, not in any hot path.
- The namespace `poker_trainer::engine` is the engine namespace. All Phase 0 engine headers live in this namespace.
- The locked properties of a Scenario ID (what the seed determines, what settings may modify at play time) are documented in ARCHITECTURE.md under "Scenario ID & Reproducibility System." This header does not enforce those properties — that is the engine generator's job in Z01. This header only defines the identifier type.

---

## 2. src/engine/rng_seed.hpp

### Purpose

Wraps `std::mt19937_64` (Mersenne Twister, 64-bit variant) seeded from a `ScenarioId`. This is the single random number generator used throughout the engine to deterministically reconstruct a scenario from its ID.

The wrapper exists to enforce a single source of randomness for scenario generation. Without it, different parts of the engine might inadvertently use different RNG instances, which would break the deterministic reconstruction contract. With it, the engine has exactly one RNG per scenario, seeded once from the `ScenarioId`, and every random draw flows through it.

The wrapper also exists as documentation: by naming it `RngSeed` and tying it to `ScenarioId`, it's structurally obvious that this RNG is the seeded RNG and not some general-purpose RNG that might be used for, say, animation timing jitter or audio shuffle pools.

### Dependencies

- `src/engine/scenario_id.hpp` (uses `ScenarioId` as seed source)

### Consumers

- Z01 (Engine): the sole consumer. Creates one `RngSeed` per scenario in the scenario generator, then pulls all random values from it for hole cards, community board, position, scenario type, side-pot status, stack sizes, and the F value for the deterministic fold function.

### Contents

```cpp
#pragma once

#include "engine/scenario_id.hpp"

#include <cstdint>
#include <random>

namespace poker_trainer::engine {

// The underlying engine type. Mersenne Twister 64-bit variant, chosen
// because the Scenario ID is 64 bits and a 64-bit engine consumes it
// without truncation. Mersenne Twister is locked as the algorithm for
// scenario generation across all versions of the trainer; changing it
// would break reproducibility for every saved or shared scenario.
using RngEngine = std::mt19937_64;

// A seeded RNG wrapper. Holds a Mersenne Twister 64-bit engine seeded
// from a ScenarioId. Provides direct access to the engine for use with
// std::uniform_int_distribution, std::shuffle, etc.
class RngSeed {
public:
    // Construct from a ScenarioId. The id must be valid (non-zero);
    // passing kInvalidScenarioId is a contract violation. In debug
    // builds this is asserted; in release builds the behavior is to
    // seed with the zero value, which is still a valid Mersenne
    // Twister seed but produces a sequence the engine considers
    // unreachable from any legitimate scenario.
    explicit RngSeed(ScenarioId id) noexcept;

    // Non-copyable. Copying an RNG produces two RNGs that yield the
    // same sequence, which is rarely the intent and almost always a
    // bug. Force callers to make this explicit.
    RngSeed(const RngSeed&) = delete;
    RngSeed& operator=(const RngSeed&) = delete;

    // Movable. Useful for transferring ownership of the RNG into a
    // scenario state object.
    RngSeed(RngSeed&&) noexcept = default;
    RngSeed& operator=(RngSeed&&) noexcept = default;

    // Access the underlying engine. Returned by reference so callers
    // can pass it to standard distributions and algorithms.
    [[nodiscard]] RngEngine& engine() noexcept { return engine_; }
    [[nodiscard]] const RngEngine& engine() const noexcept { return engine_; }

    // The ScenarioId this RNG was seeded from. Useful for logging
    // and debugging.
    [[nodiscard]] ScenarioId seed_id() const noexcept { return seed_id_; }

private:
    RngEngine engine_;
    ScenarioId seed_id_;
};

}  // namespace poker_trainer::engine
```

### Notes

- `std::mt19937_64` is the locked algorithm. Mersenne Twister 64-bit. This must not be replaced with PCG, Xorshift, or any other generator without a scenario format version bump. The architecture specification (ARCHITECTURE.md, "Scenario ID & Reproducibility System") names Mersenne Twister explicitly.
- The class is non-copyable because copying an RNG mid-generation produces two RNGs that would yield the same sequence. Almost every case where someone might want to copy an RNG is a bug. Move construction is allowed because transferring ownership into a scenario state object is a legitimate use.
- The class holds the `ScenarioId` it was seeded from. This is for debugging and logging only; the engine should not use `seed_id()` to re-seed or branch behavior. The seed-once contract is what makes scenario generation deterministic.
- `seed_id_` is stored after `engine_` in declaration order, but for a class this small, member layout doesn't materially matter. Both fields are trivially copyable so move operations are essentially member-wise copies.
- This header declares the class but does not provide the constructor implementation. The implementation is a one-line seed call: `RngSeed::RngSeed(ScenarioId id) noexcept : engine_(id.value), seed_id_(id) {}`. This goes in `src/engine/rng_seed.cpp`, which is generated as a Phase 0 implementation file (one of the small `.cpp` files Phase 0 produces — see the Sign-off criteria section above; the constructor is trivial enough that "no business logic" still holds).
- The engine asserts on `is_valid(id)` in debug builds via the standard `assert` macro. The architecture treats this as a programmer-error condition, not a runtime-recoverable condition.
- A Phase 0 implementation file is needed for this header. The implementation file path is `src/engine/rng_seed.cpp` and its contents are:

```cpp
#include "engine/rng_seed.hpp"

#include <cassert>

namespace poker_trainer::engine {

RngSeed::RngSeed(ScenarioId id) noexcept
    : engine_(id.value), seed_id_(id) {
    assert(is_valid(id) && "RngSeed constructed with invalid ScenarioId");
}

}  // namespace poker_trainer::engine
```

---

End of Part 1. Part 2 covers `auth0_config.hpp`, `sync_state.hpp`, `audio_paths.hpp`, `tier_config.hpp`, and `asset_paths.hpp`.

---

## 3. src/persistence/auth0_config.hpp

### Purpose

Defines the compile-time configuration constants for the Auth0 SDK integration. The trainer uses Auth0 as its identity provider for account-based features (sign in, sign up, password reset, account deletion, cloud-synced state). The Auth0 SDK is loaded via a JavaScript bridge at app boot; the C++ side needs these constants to construct the auth requests and validate the configuration is present.

This header is configuration-only. It declares the constants Auth0 needs (domain, client ID, redirect URI, audience) but does not perform any authentication. Authentication flows live in Z04 (`src/persistence/auth.cpp`).

### Dependencies

None.

### Consumers

- Z04 (Persistence): reads these constants when initializing the Auth0 SDK and when constructing auth requests. Performs the Auth0 health check using `kAuth0HealthCheckUrl`.
- Z05 (Bridge): the bridge layer reads `kAuth0Domain` when registering the redirect URI handler with the browser.

### Contents

    #pragma once

    #include <chrono>
    #include <string_view>

    namespace poker_trainer::persistence {

    // The Auth0 tenant domain. Set at compile time so the auth bridge
    // never has to query for it. Tenant migrations require a rebuild.
    inline constexpr std::string_view kAuth0Domain =
        "poker-trainer.us.auth0.com";

    // The Auth0 client ID for the trainer's Single Page Application. Tied
    // to the Auth0 tenant above. Public — Auth0 client IDs are not secrets
    // in SPA flows; the security model relies on the redirect URI allowlist
    // configured in the Auth0 dashboard.
    inline constexpr std::string_view kAuth0ClientId =
        "REPLACE_WITH_ACTUAL_CLIENT_ID";

    // The redirect URI Auth0 sends the user back to after authentication.
    // Must exactly match an entry in the Auth0 application's "Allowed
    // Callback URLs" setting. The production value is the CDN deployment
    // URL; for local development, a separate Auth0 application with a
    // localhost callback may be used (out of scope for Phase 0).
    inline constexpr std::string_view kAuth0RedirectUri =
        "https://app.poker-trainer.com/callback";

    // The audience identifier for the trainer's backend API. Auth0 issues
    // access tokens with this audience claim so the backend can validate
    // they were minted for this application. The leaderboard backend
    // validates the audience claim before accepting any request.
    inline constexpr std::string_view kAuth0Audience =
        "https://api.poker-trainer.com";

    // The scopes requested at sign-in. "openid" enables OIDC, "profile"
    // gives access to user display name, "email" gives access to email
    // address. No other scopes are requested; the trainer does not need
    // elevated permissions.
    inline constexpr std::string_view kAuth0Scopes = "openid profile email";

    // The URL hit by the Auth0 health check. Returns 200 when Auth0 is
    // reachable and operational. The check is fired before opening any
    // modal that depends on Auth0 (Sign In, Sign Up, Forgot Password,
    // Delete Account). On failure, the Service Outage Banner is triggered
    // instead of opening the modal.
    //
    // Using the well-known JWKS endpoint because it is publicly cacheable
    // and a fast 200 response indicates the tenant is reachable.
    inline constexpr std::string_view kAuth0HealthCheckUrl =
        "https://poker-trainer.us.auth0.com/.well-known/jwks.json";

    // The timeout for the health check request. If the health check has
    // not returned within this duration, the check is considered failed
    // and the outage banner is triggered.
    inline constexpr std::chrono::milliseconds kAuth0HealthCheckTimeout{3000};

    // The duration the health check result is cached. After a successful
    // health check, subsequent auth-dependent modal opens within this
    // window skip the check and proceed directly to opening the modal.
    // This avoids hammering the health check endpoint when the user
    // opens multiple auth modals in quick succession.
    inline constexpr std::chrono::seconds kAuth0HealthCheckCacheTtl{30};

    }  // namespace poker_trainer::persistence

### Notes

- The actual Auth0 client ID is a placeholder (`REPLACE_WITH_ACTUAL_CLIENT_ID`). The real value is provisioned during deployment setup, not committed in source. A pre-build step or a separate `auth0_config_secrets.hpp` (gitignored) would normally inject the real value; for Phase 0 the placeholder is acceptable, since no auth flows are exercised until Z04 is built.
- All constants are `inline constexpr` `std::string_view` so they have no runtime construction cost and can be used in compile-time contexts.
- The health check uses the JWKS endpoint rather than a custom `/health` route because the JWKS endpoint exists on every Auth0 tenant, is intended to be hit by clients, and a 200 response with non-empty body is a strong signal the tenant is up.
- The cache TTL of 30 seconds is chosen to balance two failure modes: too short and the user gets unnecessary health checks across multiple modal opens in the same session; too long and a real outage that starts mid-session might be hidden for the full TTL window. 30 seconds is short enough that any sustained outage is caught quickly, long enough that opening Sign In then Sign Up immediately does not double-check.
- The timeout of 3000ms is chosen because Auth0's JWKS endpoint typically responds in under 200ms; 3000ms gives generous slack for slow networks before declaring the service unavailable. The architecture spec (Notes — Service Outage Banner) does not pin a specific timeout, so this value is a Phase 0 decision that can be tuned later if production data suggests a different number.

---

## 4. src/persistence/sync_state.hpp

### Purpose

Defines the sync state primitive used by the offline indicator. The trainer continuously attempts to sync local state (settings, Tomatoes, account-linked progress) to the server. When sync is failing — network is offline, server is unreachable, retry backoff is in progress — the offline indicator appears to the left of the persistent cluster on every screen.

The sync state lives in Phase 0 (not in Z04, the persistence layer) because Z11 (Modal Infrastructure) needs to read the sync state to render the offline indicator, and Z11 must not directly depend on Z04. By lifting the state primitive into the backbone, both Z04 (which writes it) and Z11 (which reads it) depend on this Phase 0 header, not on each other.

This header defines the state shape and a thread-safe accessor. It does not define how syncs are performed, when they retry, or what triggers a failure transition. That logic lives in Z04.

### Dependencies

None beyond the standard library.

### Consumers

- Z04 (Persistence): writes the sync state. Transitions to `SyncFailing` when a sync attempt fails or when entering retry backoff. Transitions to `SyncOk` when a sync succeeds. Transitions to `SyncInProgress` while a sync is in flight.
- Z11 (Modal Infrastructure): reads the sync state every frame. Renders the offline indicator to the left of the persistent cluster when state is `SyncFailing`. Hides the indicator when state is `SyncOk` or `SyncInProgress` (in-progress is treated as "not yet a failure," consistent with how typical productivity apps signal sync status).

### Contents

    #pragma once

    #include <atomic>
    #include <chrono>
    #include <cstdint>

    namespace poker_trainer::persistence {

    // The state of the sync subsystem at any given moment.
    enum class SyncStatus : std::uint8_t {
        // No sync attempts have been made yet this session. Default at
        // app boot before the first sync is initiated. Indicator is hidden.
        Idle = 0,

        // A sync is currently in flight. Indicator is hidden (treating
        // in-flight as "not yet a failure"). Transitioning to SyncOk or
        // SyncFailing depending on the outcome.
        SyncInProgress = 1,

        // The most recent sync completed successfully. Indicator is
        // hidden. Z04 transitions to SyncInProgress when initiating the
        // next sync.
        SyncOk = 2,

        // The most recent sync failed, or the system is in retry backoff
        // between sync attempts. Indicator is visible. Z04 transitions
        // back to SyncInProgress when initiating a retry; transitions to
        // SyncOk if the retry succeeds.
        SyncFailing = 3,
    };

    // Snapshot of the sync state at the moment of read. Returned by value
    // from the read API so the consumer gets a coherent snapshot, not a
    // torn read across atomic fields.
    struct SyncStateSnapshot {
        SyncStatus status{SyncStatus::Idle};

        // Steady-clock timestamp of the last successful sync, or
        // std::chrono::steady_clock::time_point{} (epoch) if no sync has
        // succeeded this session.
        std::chrono::steady_clock::time_point last_success{};

        // Steady-clock timestamp of the last failed sync, or epoch if no
        // sync has failed this session.
        std::chrono::steady_clock::time_point last_failure{};

        // Number of consecutive failures since the last success. Reset
        // to 0 on every successful sync. Used by Z04 to compute exponential
        // backoff between retry attempts.
        std::uint32_t consecutive_failures{0};
    };

    // Read the current sync state. Safe to call from any thread. Returns
    // a coherent snapshot — fields are not torn across reads.
    [[nodiscard]] SyncStateSnapshot read_sync_state() noexcept;

    // Write the sync state. Z04 is the only legitimate writer. Calling
    // from any other zone is a contract violation. The write is atomic
    // from the reader's perspective: a single call to read_sync_state
    // after this returns will observe all fields from this write
    // atomically (no torn reads).
    void write_sync_state(const SyncStateSnapshot& snapshot) noexcept;

    }  // namespace poker_trainer::persistence

### Notes

- This file is a header. The Phase 0 implementation file `src/persistence/sync_state.cpp` provides the storage backing the read/write API:

      #include "persistence/sync_state.hpp"

      #include <atomic>

      namespace poker_trainer::persistence {

      namespace {

      // Single instance of the snapshot, guarded by a mutex-free protocol:
      // reads and writes are serialized by std::atomic ordering on the
      // `version` counter. Reader spins reading the version, the snapshot,
      // and the version again; if the version matches, the read is coherent.
      // Writer increments the version twice — once to mark the write as
      // in-progress, once to mark it as complete.
      //
      // This is a seqlock pattern. Appropriate because writes are rare
      // (one per sync attempt, which is on the order of seconds) and reads
      // are frequent (every frame from Z11).

      struct SyncStateStorage {
          std::atomic<std::uint64_t> version{0};
          SyncStateSnapshot snapshot{};
      };

      SyncStateStorage& storage() {
          static SyncStateStorage s;
          return s;
      }

      }  // namespace

      SyncStateSnapshot read_sync_state() noexcept {
          auto& s = storage();
          while (true) {
              const std::uint64_t v1 = s.version.load(std::memory_order_acquire);
              if (v1 & 1) {
                  // Write in progress, retry.
                  continue;
              }
              const SyncStateSnapshot result = s.snapshot;
              const std::uint64_t v2 = s.version.load(std::memory_order_acquire);
              if (v1 == v2) {
                  return result;
              }
              // Write happened during read, retry.
          }
      }

      void write_sync_state(const SyncStateSnapshot& snapshot) noexcept {
          auto& s = storage();
          const std::uint64_t v = s.version.load(std::memory_order_relaxed);
          s.version.store(v + 1, std::memory_order_release);  // mark in-progress
          s.snapshot = snapshot;
          s.version.store(v + 2, std::memory_order_release);  // mark complete
      }

      }  // namespace poker_trainer::persistence

- The seqlock pattern is appropriate because in this codebase there is exactly one writer (Z04's sync subsystem; in the wasm build, currently single-threaded but the pattern is correct in either case) and many readers (Z11 reads every frame). The pattern degrades gracefully to a simple atomic if there's only one thread.
- The state is process-global, intentionally. There is exactly one sync subsystem per app session. The architectural rules in CLAUDE.md permit the backbone primitives to own global state; this header sits in `src/persistence/` but it is morally a backbone primitive — it's the contract that lets Z04 and Z11 coordinate without depending on each other. The placement under `src/persistence/` is per ZONES.md's Phase 0 Owns list.
- The four-state enum (`Idle`, `SyncInProgress`, `SyncOk`, `SyncFailing`) is exhaustive. Any sync state transition must move between these four; there is no fifth state. If product needs an additional state in the future (e.g., "SyncDisabled" for offline-by-user-choice mode), it must be added here, and the indicator behavior in Z11 must be updated to handle it.
- The timestamps use `std::chrono::steady_clock` because they are used for relative comparisons (how long since last failure, how long until next retry). A wall-clock change during the session must not affect retry timing.

---

## 5. src/audio/audio_paths.hpp

### Purpose

Defines the path manifest for every audio asset shipped with the trainer. There are two categories of audio: sound effects (SFX) and music tracks.

- **SFX** — short samples played in response to events (chip movements, button clicks, modal open/close, side pot split, scenario spawn). There are 9 SFX samples total.
- **Music** — longer streamed tracks played in shuffle pools per genre. 4 genres (Lounge Jazz, Classical, Bossa Nova, Ambient), 3 tracks per genre by default = 12 tracks shipped. Each genre has 1 starter track free and 2 unlockable tracks at $25 each in the Shop.

This header enumerates the path constants. It does not load, decode, play, or manage any audio. Z03 (Audio Engine) is the consumer that loads from these paths.

### Dependencies

None.

### Consumers

- Z03 (Audio Engine): loads audio files from these paths. Maps SFX enum values to the correct sample. Maps music track IDs to the correct streaming file. Manages shuffle pools per genre.
- Z02 (Asset Pipeline): includes SFX assets in the Tier 2/3 load schedule (per `tier_config.hpp`; the side-pot split SFX is Tier 3 because it's used only in side-pot scenarios; the rest of SFX is Tier 2 because they're used in every scenario).

### Contents

    #pragma once

    #include <array>
    #include <cstddef>
    #include <cstdint>
    #include <string_view>

    namespace poker_trainer::audio {

    // Identifiers for each sound effect in the trainer. The enum value
    // is also the index into kSfxPaths below.
    enum class SfxId : std::uint8_t {
        // Chip slide / push (Caller scenarios when the active opponent
        // pushes chips toward the pot).
        ChipPush = 0,

        // Chip stack landing in the pot (used at scenario resolution
        // when chips animate from their source to the pot).
        ChipLand = 1,

        // Side pot split (used when a side pot resolves in a multi-way
        // all-in scenario).
        SidePotSplit = 2,

        // Modal open (any modal entering the screen).
        ModalSwooshOpen = 3,

        // Modal close (any modal leaving the screen).
        ModalSwooshClose = 4,

        // Scenario spawn (used at the start of every scenario; played
        // as part of the audio choreography sequence).
        ScenarioSpawn = 5,

        // Pass result (played when the user passes a scenario, on the
        // Post-Round Screen entry).
        Pass = 6,

        // Fail result (played when the user fails a scenario, on the
        // Post-Round Screen entry).
        Fail = 7,

        // Frog easter egg toggle click (played when the user clicks
        // the dealer asset the trigger number of times to toggle to
        // the Frog).
        FrogToggle = 8,
    };

    // The total number of SFX samples. Used to size arrays.
    inline constexpr std::size_t kSfxCount = 9;

    // Path to each SFX file, indexed by SfxId. All paths are relative
    // to the asset root, which is the directory served as the CDN root
    // in production builds.
    inline constexpr std::array<std::string_view, kSfxCount> kSfxPaths = {
        "assets/audio/sfx/chip_push.ogg",
        "assets/audio/sfx/chip_land.ogg",
        "assets/audio/sfx/side_pot_split.ogg",
        "assets/audio/sfx/modal_swoosh_open.ogg",
        "assets/audio/sfx/modal_swoosh_close.ogg",
        "assets/audio/sfx/scenario_spawn.ogg",
        "assets/audio/sfx/pass.ogg",
        "assets/audio/sfx/fail.ogg",
        "assets/audio/sfx/frog_toggle.ogg",
    };

    // Helper to look up the path for a given SfxId.
    [[nodiscard]] constexpr std::string_view sfx_path(SfxId id) noexcept {
        return kSfxPaths[static_cast<std::size_t>(id)];
    }

    // Identifiers for each music genre. The enum value is also the index
    // into kMusicGenreNames below.
    enum class MusicGenre : std::uint8_t {
        LoungeJazz = 0,
        Classical = 1,
        BossaNova = 2,
        Ambient = 3,
    };

    inline constexpr std::size_t kMusicGenreCount = 4;

    // Human-readable genre names, used for Shop and Settings display.
    inline constexpr std::array<std::string_view, kMusicGenreCount> kMusicGenreNames = {
        "Lounge Jazz",
        "Classical",
        "Bossa Nova",
        "Ambient",
    };

    // Identifiers for each music track. Each genre has 3 tracks; track 0
    // in each genre is the free starter, tracks 1 and 2 are paid unlocks.
    // Track IDs are globally unique across genres.
    enum class MusicTrackId : std::uint8_t {
        // Lounge Jazz
        LoungeJazz_Starter = 0,
        LoungeJazz_Track2 = 1,
        LoungeJazz_Track3 = 2,

        // Classical
        Classical_Starter = 3,
        Classical_Track2 = 4,
        Classical_Track3 = 5,

        // Bossa Nova
        BossaNova_Starter = 6,
        BossaNova_Track2 = 7,
        BossaNova_Track3 = 8,

        // Ambient
        Ambient_Starter = 9,
        Ambient_Track2 = 10,
        Ambient_Track3 = 11,
    };

    inline constexpr std::size_t kMusicTrackCount = 12;

    // Metadata for a music track.
    struct MusicTrackInfo {
        std::string_view display_name;
        std::string_view path;
        MusicGenre genre;
        bool is_starter;            // true if free / unlocked by default
        std::uint32_t price_cents;  // 0 for starters, 2500 ($25) for paid unlocks
    };

    // Per-track metadata indexed by MusicTrackId. The path field gives
    // the asset-root-relative path to the streaming file. The display_name
    // field is used for Shop and Settings rendering.
    inline constexpr std::array<MusicTrackInfo, kMusicTrackCount> kMusicTracks = {{
        // Lounge Jazz
        {"After Hours",       "assets/audio/music/lounge_jazz/after_hours.ogg",
            MusicGenre::LoungeJazz, true,  0},
        {"Smoke and Mirrors", "assets/audio/music/lounge_jazz/smoke_and_mirrors.ogg",
            MusicGenre::LoungeJazz, false, 2500},
        {"Penthouse Suite",   "assets/audio/music/lounge_jazz/penthouse_suite.ogg",
            MusicGenre::LoungeJazz, false, 2500},

        // Classical
        {"Nocturne",          "assets/audio/music/classical/nocturne.ogg",
            MusicGenre::Classical, true,  0},
        {"Counterpoint",      "assets/audio/music/classical/counterpoint.ogg",
            MusicGenre::Classical, false, 2500},
        {"Adagio",            "assets/audio/music/classical/adagio.ogg",
            MusicGenre::Classical, false, 2500},

        // Bossa Nova
        {"Ipanema Night",     "assets/audio/music/bossa_nova/ipanema_night.ogg",
            MusicGenre::BossaNova, true,  0},
        {"Sao Paulo",         "assets/audio/music/bossa_nova/sao_paulo.ogg",
            MusicGenre::BossaNova, false, 2500},
        {"Copacabana",        "assets/audio/music/bossa_nova/copacabana.ogg",
            MusicGenre::BossaNova, false, 2500},

        // Ambient
        {"Velvet Room",       "assets/audio/music/ambient/velvet_room.ogg",
            MusicGenre::Ambient,   true,  0},
        {"Slow Tide",         "assets/audio/music/ambient/slow_tide.ogg",
            MusicGenre::Ambient,   false, 2500},
        {"Distant Lights",    "assets/audio/music/ambient/distant_lights.ogg",
            MusicGenre::Ambient,   false, 2500},
    }};

    // Helper to look up info for a given track.
    [[nodiscard]] constexpr const MusicTrackInfo& music_track_info(MusicTrackId id) noexcept {
        return kMusicTracks[static_cast<std::size_t>(id)];
    }

    }  // namespace poker_trainer::audio

### Notes

- All audio assets use the `.ogg` container with Vorbis encoding. Ogg is chosen over MP3 because it's patent-free, well-supported in browsers, and produces smaller files at equivalent quality. The audio decoder library (miniaudio) supports Ogg natively.
- The track display names ("After Hours", "Nocturne", etc.) are placeholders. Final names come from the music licensing decisions made before audio asset finalization. The header structure is correct; the strings are content that may change before launch but the structure is locked.
- The starter track for each genre is at index 0 within the genre's three tracks (`LoungeJazz_Starter`, `Classical_Starter`, `BossaNova_Starter`, `Ambient_Starter`). The starter is free; the other two tracks are paid unlocks at 2500 cents ($25) each. The price is hardcoded in the metadata table because this is the per-track price across the entire trainer per the architecture spec; if pricing tiers change, this table updates with the change.
- `price_cents` uses `std::uint32_t` and stores cents rather than dollars. Integer-cent representation avoids any floating-point money handling. The Shop display layer converts to a formatted dollar string for the user.
- The `MusicTrackInfo` struct is `constexpr`-friendly (all members are trivially constructible). The `std::array<MusicTrackInfo, 12>` is `inline constexpr` and lives in the binary's read-only data section.
- The actual `.ogg` files are sourced separately and placed at the paths listed. Phase 0 does not produce binary audio files; it only declares the paths the audio engine will read.

---

## 6. src/assets/tier_config.hpp

### Purpose

Defines the four asset loading tiers and the per-tier loading behavior. The architecture splits all assets across four tiers based on when they're needed:

- **Tier 1** — synchronously loaded before the app renders its first frame. Required for the loading screen and the Root screen.
- **Tier 2** — asynchronously loaded after Root screen renders. Required before the user can transition into the Game screen.
- **Tier 3** — loaded when the user clicks from Root to Mode Selection. Required before scenarios begin.
- **Tier 4** — on-demand loaded when a specific user action requires the asset (e.g., Frog easter egg toggle, Post-Round Screen dealer assets).

Each tier has different failure handling: Tier 1 and 2 failures show the Error Screen (Z05); Tier 3 and 4 failures silently skip the asset and continue.

This header defines the tier enumeration and the per-tier behavior constants. Z02 (Asset Pipeline) is the consumer that implements the tier loading orchestration.

### Dependencies

None.

### Consumers

- Z02 (Asset Pipeline): drives the tier loading. Per-tier retry policy. Per-tier fatal-failure handling.
- Z05 (Bridge): renders the Error Screen when Z02 reports a Tier 1 or 2 fatal failure. Triggers Tier 1 load at app start; Tier 2 load after Root screen first paint.
- `asset_paths.hpp` (this same Phase 0 batch): tags each asset with its tier via the `AssetEntry::tier` field.

### Contents

    #pragma once

    #include <chrono>
    #include <cstdint>

    namespace poker_trainer::assets {

    // Asset loading tiers. Defines when an asset is fetched and how
    // failures are handled.
    enum class AssetTier : std::uint8_t {
        // Synchronously loaded before the app renders. Required for
        // the loading screen and the Root screen. Failure shows the
        // Error Screen with a Retry button (Z05).
        Tier1 = 1,

        // Asynchronously loaded after Root screen renders. Required
        // before scenarios begin. Failure shows the Error Screen with
        // a Retry button (Z05).
        Tier2 = 2,

        // Loaded when the user clicks from Root to Mode Selection.
        // Required for scenarios but not for the Root screen itself.
        // Failure is silent — the asset is marked unavailable and the
        // feature degrades gracefully (e.g., a missing SFX simply plays
        // no sound; the scenario still runs).
        Tier3 = 3,

        // On-demand loaded when a triggering user action occurs.
        // Used for the Frog easter egg assets and the front-facing
        // dealer assets that appear on the Post-Round Screen. Failure
        // is silent.
        Tier4 = 4,
    };

    // Loading behavior parameters per tier.
    struct TierConfig {
        // Number of retry attempts per asset within this tier before
        // declaring the asset fatally failed (Tier 1/2) or skipping
        // it (Tier 3/4).
        std::uint8_t max_retries;

        // Initial delay before the first retry. Subsequent retries use
        // exponential backoff: this delay, then 2x, then 4x.
        std::chrono::milliseconds initial_retry_delay;

        // If true, a fatal failure in this tier triggers the Error
        // Screen (Z05). If false, the failure is silent and the asset
        // is marked unavailable.
        bool fatal_failure_shows_error_screen;
    };

    inline constexpr TierConfig kTier1Config{
        .max_retries = 3,
        .initial_retry_delay = std::chrono::milliseconds{500},
        .fatal_failure_shows_error_screen = true,
    };

    inline constexpr TierConfig kTier2Config{
        .max_retries = 3,
        .initial_retry_delay = std::chrono::milliseconds{500},
        .fatal_failure_shows_error_screen = true,
    };

    inline constexpr TierConfig kTier3Config{
        .max_retries = 3,
        .initial_retry_delay = std::chrono::milliseconds{500},
        .fatal_failure_shows_error_screen = false,
    };

    inline constexpr TierConfig kTier4Config{
        .max_retries = 3,
        .initial_retry_delay = std::chrono::milliseconds{500},
        .fatal_failure_shows_error_screen = false,
    };

    // Helper to look up the config for a given tier.
    [[nodiscard]] constexpr const TierConfig& tier_config(AssetTier tier) noexcept {
        switch (tier) {
            case AssetTier::Tier1: return kTier1Config;
            case AssetTier::Tier2: return kTier2Config;
            case AssetTier::Tier3: return kTier3Config;
            case AssetTier::Tier4: return kTier4Config;
        }
        // Unreachable; the switch is exhaustive.
        return kTier1Config;
    }

    }  // namespace poker_trainer::assets

### Notes

- The four tiers all share the same retry policy (3 retries with exponential backoff starting at 500ms). This is intentional: the policy is a property of "how aggressively do we retry asset fetches," not a property of "what tier is this asset in." If product later decides Tier 1 should retry more aggressively than Tier 4, the configs diverge then.
- The difference between tiers is in *what happens after exhausting retries*, not in the retry count itself. Tier 1/2 trigger the Error Screen because the app cannot proceed without these assets; Tier 3/4 silently skip because the app can proceed (with degraded behavior).
- Exponential backoff is 500ms → 1000ms → 2000ms across the 3 retries. Z02 implements the backoff; this header only declares the initial delay.
- The `tier_config()` helper returns by const reference because `TierConfig` is small but the reference avoids a copy and emphasizes that the configs are read-only.
- The unreachable `return kTier1Config;` at the end of the switch is necessary because some compilers warn about non-void functions without a return at the end, even when the switch is exhaustive. This is a known C++ pattern.

---

## 7. src/assets/asset_paths.hpp

### Purpose

Defines the path manifest for every PNG asset shipped with the trainer. Each asset is tagged with its loading tier (per `tier_config.hpp`) and an identifier the rest of the codebase uses to refer to it.

The trainer ships approximately 50 PNG assets across four tiers:

- **Tier 1** assets: application logo, blurred Root background variants for each theme (4), the dealer button icon.
- **Tier 2** assets: side-profile Butler asset, hole card faces (52 cards) and card back, table felt overlay, chip face PNGs (8 denominations), cluster icons (Shop, Help, Settings, Home, X), front-facing Butler assets (neutral and raised).
- **Tier 3** assets: scenario-specific decorative assets (position indicators UTG/HJ/CO/BTN/SB/BB, side-pot all-in marker, dealer button asset in profile, opponent seat markers).
- **Tier 4** assets: Frog easter egg dealer assets (side-profile Frog, front-facing Frog neutral/raised, four Frog expression overlays).

This header enumerates every asset by identifier, path, and tier. It does not load any asset; that is Z02's job.

### Dependencies

- `src/assets/tier_config.hpp`

### Consumers

- Z02 (Asset Pipeline): iterates `kAssetEntries` to drive the tier loaders. Resolves asset handles by `AssetId`.
- Z07, Z08, Z11, Z13: query the asset registry for rendered textures via `AssetId` (the actual texture handle comes from Z02; this header only provides the path lookup).
- Z14 (Tutorial): references specific asset IDs when rendering tutorial callouts that point at specific UI elements.

### Contents

    #pragma once

    #include "assets/tier_config.hpp"

    #include <array>
    #include <cstddef>
    #include <cstdint>
    #include <string_view>

    namespace poker_trainer::assets {

    // Identifiers for every PNG asset in the trainer. The enum value
    // is also the index into kAssetEntries below.
    enum class AssetId : std::uint16_t {
        // --- Tier 1 ---
        AppLogo = 0,
        RootBackgroundNoLimit = 1,
        RootBackgroundSlate = 2,
        RootBackgroundOcean = 3,
        RootBackgroundSage = 4,
        DealerButton = 5,

        // --- Tier 2: Butler (side profile, Game screen) ---
        ButlerSideProfile = 6,

        // --- Tier 2: Card faces (52 cards) ---
        // Order: Spades A-K, Hearts A-K, Diamonds A-K, Clubs A-K
        CardSpadeA = 7, CardSpade2 = 8, CardSpade3 = 9, CardSpade4 = 10,
        CardSpade5 = 11, CardSpade6 = 12, CardSpade7 = 13, CardSpade8 = 14,
        CardSpade9 = 15, CardSpadeT = 16, CardSpadeJ = 17, CardSpadeQ = 18,
        CardSpadeK = 19,
        CardHeartA = 20, CardHeart2 = 21, CardHeart3 = 22, CardHeart4 = 23,
        CardHeart5 = 24, CardHeart6 = 25, CardHeart7 = 26, CardHeart8 = 27,
        CardHeart9 = 28, CardHeartT = 29, CardHeartJ = 30, CardHeartQ = 31,
        CardHeartK = 32,
        CardDiamondA = 33, CardDiamond2 = 34, CardDiamond3 = 35, CardDiamond4 = 36,
        CardDiamond5 = 37, CardDiamond6 = 38, CardDiamond7 = 39, CardDiamond8 = 40,
        CardDiamond9 = 41, CardDiamondT = 42, CardDiamondJ = 43, CardDiamondQ = 44,
        CardDiamondK = 45,
        CardClubA = 46, CardClub2 = 47, CardClub3 = 48, CardClub4 = 49,
        CardClub5 = 50, CardClub6 = 51, CardClub7 = 52, CardClub8 = 53,
        CardClub9 = 54, CardClubT = 55, CardClubJ = 56, CardClubQ = 57,
        CardClubK = 58,

        // --- Tier 2: Card back ---
        CardBack = 59,

        // --- Tier 2: Table felt ---
        TableFelt = 60,

        // --- Tier 2: Chip denomination faces ---
        ChipWhite = 61,
        ChipRed = 62,
        ChipGreen = 63,
        ChipBlack = 64,
        ChipPurple = 65,
        ChipYellow = 66,
        ChipBrown = 67,
        ChipGold = 68,

        // --- Tier 2: Cluster icons ---
        IconShop = 69,
        IconHelp = 70,
        IconSettings = 71,
        IconHome = 72,
        IconClose = 73,  // The X icon

        // --- Tier 2: Front-facing Butler (Post-Round Screen) ---
        ButlerFrontNeutral = 74,
        ButlerFrontRaised = 75,

        // --- Tier 3: Position indicators ---
        PositionUTG = 76,
        PositionHJ = 77,
        PositionCO = 78,
        PositionBTN = 79,
        PositionSB = 80,
        PositionBB = 81,

        // --- Tier 3: Side pot all-in marker ---
        SidePotAllInMarker = 82,

        // --- Tier 4: Frog easter egg ---
        FrogSideProfile = 83,
        FrogFrontNeutral = 84,
        FrogFrontRaised = 85,
        FrogExpressionPass = 86,
        FrogExpressionFail = 87,
        FrogExpressionOvertime = 88,
        FrogExpressionPerfect = 89,
    };

    inline constexpr std::size_t kAssetCount = 90;

    // Metadata for a single asset.
    struct AssetEntry {
        std::string_view path;
        AssetTier tier;
    };

    // Per-asset metadata indexed by AssetId. The path field gives the
    // asset-root-relative path to the PNG file. The tier field tags
    // the asset for the tier loader.
    inline constexpr std::array<AssetEntry, kAssetCount> kAssetEntries = {{
        // --- Tier 1 ---
        {"assets/images/tier1/app_logo.png",                   AssetTier::Tier1},
        {"assets/images/tier1/root_bg_no_limit.png",           AssetTier::Tier1},
        {"assets/images/tier1/root_bg_slate.png",              AssetTier::Tier1},
        {"assets/images/tier1/root_bg_ocean.png",              AssetTier::Tier1},
        {"assets/images/tier1/root_bg_sage.png",               AssetTier::Tier1},
        {"assets/images/tier1/dealer_button.png",              AssetTier::Tier1},

        // --- Tier 2: Butler side profile ---
        {"assets/images/tier2/butler_side_profile.png",        AssetTier::Tier2},

        // --- Tier 2: Cards (52) ---
        {"assets/images/tier2/cards/spade_a.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/spade_2.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/spade_3.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/spade_4.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/spade_5.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/spade_6.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/spade_7.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/spade_8.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/spade_9.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/spade_t.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/spade_j.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/spade_q.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/spade_k.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/heart_a.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/heart_2.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/heart_3.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/heart_4.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/heart_5.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/heart_6.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/heart_7.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/heart_8.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/heart_9.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/heart_t.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/heart_j.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/heart_q.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/heart_k.png",   AssetTier::Tier2},
        {"assets/images/tier2/cards/diamond_a.png", AssetTier::Tier2},
        {"assets/images/tier2/cards/diamond_2.png", AssetTier::Tier2},
        {"assets/images/tier2/cards/diamond_3.png", AssetTier::Tier2},
        {"assets/images/tier2/cards/diamond_4.png", AssetTier::Tier2},
        {"assets/images/tier2/cards/diamond_5.png", AssetTier::Tier2},
        {"assets/images/tier2/cards/diamond_6.png", AssetTier::Tier2},
        {"assets/images/tier2/cards/diamond_7.png", AssetTier::Tier2},
        {"assets/images/tier2/cards/diamond_8.png", AssetTier::Tier2},
        {"assets/images/tier2/cards/diamond_9.png", AssetTier::Tier2},
        {"assets/images/tier2/cards/diamond_t.png", AssetTier::Tier2},
        {"assets/images/tier2/cards/diamond_j.png", AssetTier::Tier2},
        {"assets/images/tier2/cards/diamond_q.png", AssetTier::Tier2},
        {"assets/images/tier2/cards/diamond_k.png", AssetTier::Tier2},
        {"assets/images/tier2/cards/club_a.png",    AssetTier::Tier2},
        {"assets/images/tier2/cards/club_2.png",    AssetTier::Tier2},
        {"assets/images/tier2/cards/club_3.png",    AssetTier::Tier2},
        {"assets/images/tier2/cards/club_4.png",    AssetTier::Tier2},
        {"assets/images/tier2/cards/club_5.png",    AssetTier::Tier2},
        {"assets/images/tier2/cards/club_6.png",    AssetTier::Tier2},
        {"assets/images/tier2/cards/club_7.png",    AssetTier::Tier2},
        {"assets/images/tier2/cards/club_8.png",    AssetTier::Tier2},
        {"assets/images/tier2/cards/club_9.png",    AssetTier::Tier2},
        {"assets/images/tier2/cards/club_t.png",    AssetTier::Tier2},
        {"assets/images/tier2/cards/club_j.png",    AssetTier::Tier2},
        {"assets/images/tier2/cards/club_q.png",    AssetTier::Tier2},
        {"assets/images/tier2/cards/club_k.png",    AssetTier::Tier2},

        // --- Tier 2: Card back ---
        {"assets/images/tier2/cards/back.png",                 AssetTier::Tier2},

        // --- Tier 2: Table felt ---
        {"assets/images/tier2/table_felt.png",                 AssetTier::Tier2},

        // --- Tier 2: Chip faces ---
        {"assets/images/tier2/chips/chip_white.png",           AssetTier::Tier2},
        {"assets/images/tier2/chips/chip_red.png",             AssetTier::Tier2},
        {"assets/images/tier2/chips/chip_green.png",           AssetTier::Tier2},
        {"assets/images/tier2/chips/chip_black.png",           AssetTier::Tier2},
        {"assets/images/tier2/chips/chip_purple.png",          AssetTier::Tier2},
        {"assets/images/tier2/chips/chip_yellow.png",          AssetTier::Tier2},
        {"assets/images/tier2/chips/chip_brown.png",           AssetTier::Tier2},
        {"assets/images/tier2/chips/chip_gold.png",            AssetTier::Tier2},

        // --- Tier 2: Cluster icons ---
        {"assets/images/tier2/icons/shop.png",                 AssetTier::Tier2},
        {"assets/images/tier2/icons/help.png",                 AssetTier::Tier2},
        {"assets/images/tier2/icons/settings.png",             AssetTier::Tier2},
        {"assets/images/tier2/icons/home.png",                 AssetTier::Tier2},
        {"assets/images/tier2/icons/close.png",                AssetTier::Tier2},

        // --- Tier 2: Front-facing Butler ---
        {"assets/images/tier2/butler_front_neutral.png",       AssetTier::Tier2},
        {"assets/images/tier2/butler_front_raised.png",        AssetTier::Tier2},

        // --- Tier 3: Position indicators ---
        {"assets/images/tier3/positions/utg.png",              AssetTier::Tier3},
        {"assets/images/tier3/positions/hj.png",               AssetTier::Tier3},
        {"assets/images/tier3/positions/co.png",               AssetTier::Tier3},
        {"assets/images/tier3/positions/btn.png",              AssetTier::Tier3},
        {"assets/images/tier3/positions/sb.png",               AssetTier::Tier3},
        {"assets/images/tier3/positions/bb.png",               AssetTier::Tier3},

        // --- Tier 3: Side pot all-in marker ---
        {"assets/images/tier3/side_pot_all_in_marker.png",     AssetTier::Tier3},

        // --- Tier 4: Frog easter egg ---
        {"assets/images/tier4/frog_side_profile.png",          AssetTier::Tier4},
        {"assets/images/tier4/frog_front_neutral.png",         AssetTier::Tier4},
        {"assets/images/tier4/frog_front_raised.png",          AssetTier::Tier4},
        {"assets/images/tier4/frog_expression_pass.png",       AssetTier::Tier4},
        {"assets/images/tier4/frog_expression_fail.png",       AssetTier::Tier4},
        {"assets/images/tier4/frog_expression_overtime.png",   AssetTier::Tier4},
        {"assets/images/tier4/frog_expression_perfect.png",    AssetTier::Tier4},
    }};

    // Helper to look up the entry for a given AssetId.
    [[nodiscard]] constexpr const AssetEntry& asset_entry(AssetId id) noexcept {
        return kAssetEntries[static_cast<std::size_t>(id)];
    }

    // Helper to look up just the path.
    [[nodiscard]] constexpr std::string_view asset_path(AssetId id) noexcept {
        return kAssetEntries[static_cast<std::size_t>(id)].path;
    }

    // Helper to look up just the tier.
    [[nodiscard]] constexpr AssetTier asset_tier(AssetId id) noexcept {
        return kAssetEntries[static_cast<std::size_t>(id)].tier;
    }

    }  // namespace poker_trainer::assets

### Notes

- The card asset enumeration uses the order Spades, Hearts, Diamonds, Clubs and within each suit A, 2-9, T (ten), J, Q, K. This order is deliberate and locked: it matches the canonical bridge/poker rank-suit ordering used in most reference implementations and makes it easy to compute `AssetId::CardSpadeA + card_index` for translation from a numeric card index to an asset.
- The four Root background variants (one per theme) are at Tier 1 because the loading screen displays the heavily-blurred Root background, which requires the background asset to be loaded before the loading screen renders. The architecture spec (Notes — Home Screen) specifies this behavior.
- Chip faces are at Tier 2 because chips appear on the Game screen, which loads as Tier 2. The 8 denominations (white, red, green, black, purple, yellow, brown, gold) match the chip denomination set in the architecture spec.
- The 7 Frog easter egg assets are all at Tier 4 because they're loaded on-demand only after the user triggers the easter egg by clicking the dealer the trigger number of times. The 4 expression overlays (pass, fail, overtime, perfect) overlay onto the front-facing Frog on the Post-Round Screen.
- The asset count of 90 is computed: 6 (Tier 1) + 1 (Butler side) + 52 (cards) + 1 (card back) + 1 (table felt) + 8 (chips) + 5 (icons) + 2 (Butler front) + 6 (positions) + 1 (side pot marker) + 7 (Frog) = 90. This count is asserted in the integration test in Part 5.
- All paths assume a directory tree under `assets/` matching the structure shown in CLAUDE.md section 3. The directory structure must exist (as empty directories until binary assets are placed) before Z02 attempts to load anything.

---

End of Part 2. Part 3 covers `theme_tokens.hpp` and `persistence_schema.hpp`.

---

## 8. src/theme/theme_tokens.hpp

### Purpose

Defines the token-based color system that every UI element in the trainer renders against. Instead of any zone hardcoding a color value (e.g., `ImVec4{0.0f, 0.4f, 0.2f, 1.0f}` for "table green"), every color-bearing element references a token from this enum. The active theme's palette maps tokens to actual `ImVec4` values; switching themes is a single palette swap with no code changes in any consumer zone.

The trainer ships with four themes: No Limit (default), Slate, Ocean, and Sage. All themes share the same token set; only the values bound to each token differ between themes. A handful of tokens are **fixed across all themes** (chip face colors, dealer button blue/green) — these tokens exist in the enum so the consumer API is uniform, but their palette values are identical across all four themes.

This header defines:

- The complete token enum (every color used anywhere in the trainer).
- The `Theme` struct shape that holds the full palette.
- The four theme palette identifiers.
- Constants for fixed-across-theme tokens.

This header does **not** contain any actual color values. The four palette implementations live in Z06 (`src/theme/palette_no_limit.cpp`, etc.) and are the only files in the codebase permitted to contain literal RGBA values.

### Dependencies

None. This header uses `ImVec4` from Dear ImGui via forward declaration to avoid pulling ImGui into every consumer.

### Consumers

Every zone that renders anything visible. The major consumers:

- Z06 (Theme System): owns the palette implementations and the active-theme switching logic. The only zone that creates `Theme` instances.
- Z07 (Root + Mode Selection Screens): tokens for button backgrounds, text, separators, logo placement region.
- Z08 (Game Screen): tokens for table felt overlay, HUD text, opponent name colors, position indicator backgrounds, side pot all-in marker, chip stack labels.
- Z09 (Math Interrogator): tokens for input field background, input field border (default and focused), input text color, bet-size button background and text, error/correct state colors.
- Z10 (Temporal): tokens for the visual countdown text (default text color, overtime red).
- Z11 (Modal Infrastructure): tokens for modal background, modal border, cluster icon tint, offline indicator color, outage banner background and text.
- Z12 (Settings): tokens for settings list background, section header text, search bar styling, dropdown styling, slider track and handle, theme picker swatches.
- Z13 (Post-Round): tokens for stat modal background, tier tab background and active state, time grade row colors, Again button states (default, armed, commit).
- Z14 (Tutorial): tokens for spotlight overlay lens color, callout panel background, callout border, Next button background.

### Contents

    #pragma once

    #include <array>
    #include <cstddef>
    #include <cstdint>
    #include <string_view>

    // Forward declaration of ImVec4 to avoid pulling Dear ImGui into every
    // consumer of this header. The actual definition is in imgui.h, which
    // palette implementation files in Z06 include.
    struct ImVec4;

    namespace poker_trainer::theme {

    // Token identifiers. Every color-bearing element in the trainer
    // references one of these tokens. The enum value is also the index
    // into Theme::tokens below.
    enum class ColorToken : std::uint16_t {
        // --- Backgrounds ---
        BgRoot = 0,                  // Root screen base background
        BgGame = 1,                  // Game screen base background
        BgPostRound = 2,             // Post-Round Screen base background
        BgModalSurface = 3,          // Modal panel fill
        BgModalScrim = 4,            // Dimming layer behind modals
        BgTableFelt = 5,             // Poker table felt overlay tint

        // --- Text ---
        TextPrimary = 6,             // Default text color
        TextSecondary = 7,           // Subdued text (labels, captions)
        TextDisabled = 8,            // Greyed-out text
        TextOnAccent = 9,            // Text on accent-colored elements (buttons)

        // --- Borders and separators ---
        BorderDefault = 10,          // Default border for input fields, panels
        BorderFocus = 11,            // 2px focus indicator outline
        SeparatorLine = 12,          // Horizontal/vertical separator lines

        // --- Buttons ---
        ButtonBg = 13,               // Default button background
        ButtonBgHover = 14,          // Button on hover
        ButtonBgActive = 15,         // Button while being clicked
        ButtonBgDisabled = 16,       // Disabled button
        ButtonBgPrimary = 17,        // Primary call-to-action background
        ButtonBgDanger = 18,         // Destructive action background (delete, reset)

        // --- Inputs ---
        InputBg = 19,                // Text input field background
        InputBgFocused = 20,         // Text input when focused
        InputText = 21,              // Text inside input fields

        // --- State colors ---
        StatePass = 22,              // Success / pass color
        StateFail = 23,              // Failure / error color
        StateWarn = 24,              // Warning / caution color
        StateOvertime = 25,          // Overtime countdown red

        // --- HUD on Game screen ---
        HudPotText = 26,             // Pot amount text
        HudBlindsText = 27,          // Blinds text
        HudBetAmountText = 28,       // Floating bet amount text

        // --- Cluster icons ---
        ClusterIconTint = 29,        // Default cluster icon tint
        ClusterIconHover = 30,       // Cluster icon on hover

        // --- Offline indicator ---
        OfflineIndicator = 31,       // The sync-failing indicator

        // --- Outage banner ---
        OutageBannerBg = 32,         // Service Outage Banner background
        OutageBannerText = 33,       // Service Outage Banner text
        OutageBannerCountdown = 34,  // Countdown bar fill

        // --- Settings ---
        SettingsSidebarBg = 35,      // Settings modal sidebar background
        SettingsSectionHeader = 36,  // Section header text
        SettingsSliderTrack = 37,    // Slider track background
        SettingsSliderFill = 38,     // Slider fill before the handle
        SettingsSliderHandle = 39,   // Slider handle

        // --- Tutorial ---
        TutorialScrim = 40,          // Tutorial overlay dim layer
        TutorialCalloutBg = 41,      // Callout panel background
        TutorialCalloutBorder = 42,  // Callout panel border

        // --- Post-Round stat modal ---
        StatModalBg = 43,            // Stat modal background (65% opacity)
        StatModalTabBg = 44,         // Tier tab default background
        StatModalTabActive = 45,     // Active tier tab background
        TimeGradeUndertime = 46,     // Time grade row, under-time color
        TimeGradeOvertime = 47,      // Time grade row, over-time color

        // --- Again button (Post-Round) ---
        AgainButtonDefault = 48,     // AGAIN default state
        AgainButtonArmed = 49,       // AGAIN armed state
        AgainButtonCommit = 50,      // CONFIRM commit state

        // --- Fixed across all themes ---
        // These tokens exist for API uniformity but their values are
        // identical in all four palettes. They cannot be themed.
        ChipWhite = 51,
        ChipRed = 52,
        ChipGreen = 53,
        ChipBlack = 54,
        ChipPurple = 55,
        ChipYellow = 56,
        ChipBrown = 57,
        ChipGold = 58,
        DealerButtonBlue = 59,
        DealerButtonGreen = 60,

        // Sentinel.
        Count = 61,
    };

    inline constexpr std::size_t kColorTokenCount =
        static_cast<std::size_t>(ColorToken::Count);

    // Theme palette holder. One instance per theme; the four palette
    // implementations in Z06 produce one of these each. The active
    // theme pointer is held by Z06 and queried by every rendering
    // consumer via get_color().
    struct Theme {
        // Non-owning pointer to a kColorTokenCount-element ImVec4 array.
        // The pointee is owned by the palette .cpp file in Z06 (which
        // includes imgui.h), giving ImVec4 its complete definition there.
        // theme_tokens.hpp itself never needs ImVec4 to be a complete type.
        // Indexed by ColorToken: tokens[static_cast<size_t>(ColorToken::X)]
        // gives the ImVec4 for token X under this theme.
        const ImVec4* tokens;

        // Theme identifier (one of the kThemeId* values below).
        std::uint8_t theme_id;

        // Display name for the Settings theme picker.
        std::string_view display_name;
    };

    // Theme identifiers. The Settings theme dropdown lists themes in
    // this exact order.
    inline constexpr std::uint8_t kThemeIdNoLimit = 0;
    inline constexpr std::uint8_t kThemeIdSlate = 1;
    inline constexpr std::uint8_t kThemeIdOcean = 2;
    inline constexpr std::uint8_t kThemeIdSage = 3;
    inline constexpr std::uint8_t kThemeIdCount = 4;

    // Display names for each theme, for the Settings theme picker.
    inline constexpr std::array<std::string_view, kThemeIdCount> kThemeDisplayNames = {
        "No Limit",
        "Slate",
        "Ocean",
        "Sage",
    };

    // The set of tokens whose values are fixed across all themes.
    // Palette implementations must populate these tokens with the same
    // values in all four palettes. The integration test asserts this.
    inline constexpr std::array<ColorToken, 10> kFixedAcrossThemeTokens = {
        ColorToken::ChipWhite,
        ColorToken::ChipRed,
        ColorToken::ChipGreen,
        ColorToken::ChipBlack,
        ColorToken::ChipPurple,
        ColorToken::ChipYellow,
        ColorToken::ChipBrown,
        ColorToken::ChipGold,
        ColorToken::DealerButtonBlue,
        ColorToken::DealerButtonGreen,
    };

    // Returns the active theme's color for a given token. Implemented
    // in Z06 (palette switching logic). Phase 0 declares the signature
    // only; the implementation is part of Z06's wave.
    //
    // Returned by const reference to avoid copying ImVec4 (16 bytes).
    [[nodiscard]] const ImVec4& get_color(ColorToken token) noexcept;

    }  // namespace poker_trainer::theme

### Notes

- This header is the largest single point of contact between Phase 0 and the rest of the codebase. Every visible UI element queries it. Get it right.
- The token enum is partitioned into semantic groups (backgrounds, text, borders, buttons, inputs, state, HUD, cluster, offline, outage, settings, tutorial, post-round, again button, fixed). The grouping is for human readability; the underlying values are dense integers 0..60 for indexing into the `tokens` array.
- `ColorToken::Count` is the sentinel value that gives the total count. It is not a real token; consumers must never use it as an argument to `get_color()`. The integration test will verify the count is 61 (matching the manually counted tokens above).
- The fixed-across-theme tokens (chips, dealer buttons) are part of the enum because the consumer API is uniform: every color is fetched via `get_color(token)`. The palette implementations in Z06 must populate these 10 tokens with identical values across all four palettes. The integration test (Part 5) asserts equality of these tokens across palettes once the palettes exist.
- `get_color()` is declared but not implemented in Phase 0. The implementation lives in Z06. The Phase 0 integration test does not call this function; it only verifies the header compiles. This is appropriate because palette implementation is squarely Z06's scope.
- The 65% opacity for `StatModalBg` is not encoded in the token itself; the token holds the base color with full alpha, and the rendering layer applies the 65% opacity. This separation allows the base color to come from the theme palette while the opacity remains constant across themes.
- The `Theme::display_name` field uses `std::string_view`. It points to a string with static storage duration (one of the entries in `kThemeDisplayNames` or a string literal in the palette implementation). The lifetime contract is that palette implementations only ever use string literals or `kThemeDisplayNames` entries here; they never use dynamically allocated strings.
- The token list is **frozen** at Phase 0 sign-off. Adding a new token after sign-off requires updating every palette implementation. Removing a token requires checking every consumer. Both are explicit decisions, not silent changes.
- A handful of architectural-spec colors are **deliberately not in this enum** because they are not theme-controlled:
  - Card face PNGs (rendered as authored, no tinting).
  - Butler and Frog character art (rendered as authored, no tinting).
  - Theme-specific Root background PNGs (the four `RootBackground*` assets in `asset_paths.hpp`). Each theme has a different background asset; the asset itself is the color, not a token.
- `Theme::tokens` is a non-owning pointer to a `kColorTokenCount`-element `ImVec4` array. The pointee array is defined in each palette translation unit in Z06 (e.g., `no_limit_palette.cpp`), which is where `imgui.h` is included. `theme_tokens.hpp` itself never requires `ImVec4` to be a complete type. Palette `.cpp` files define their array as `inline constexpr std::array<ImVec4, kColorTokenCount> kPaletteNameColors = {...};` and then `inline constexpr Theme kPaletteNameTheme = { .tokens = kPaletteNameColors.data(), .theme_id = ..., .display_name = ... };`. The lifetime of the pointee is static storage duration, so the pointer in `Theme` is always valid for the program's lifetime.

---

## 9. src/persistence/persistence_schema.hpp

### Purpose

Defines the in-memory representation of all persisted application state. This is the data structure that gets serialized to IDBFS (the in-browser filesystem) for local persistence and synced to the leaderboard backend for account-linked persistence.

The schema is versioned. Every persisted blob carries a `schema_version` so future schema changes can be migrated forward. The current version is 1.

This header defines:

- The top-level `AppState` struct.
- Sub-structures for Tomatoes accounting, settings persistence, music library state, scenario history, and account linkage.
- The schema version constants.
- Free functions for schema-level validation and version checking.

This header does **not** define how the schema is read from or written to IDBFS, nor how it's synced to the backend. Those are Z04's job. This header only defines the shape.

### Dependencies

- `src/engine/scenario_id.hpp`
- `src/persistence/sync_state.hpp`

### Consumers

- Z04 (Persistence): the primary consumer. Reads and writes `AppState` to IDBFS. Syncs `AppState` to backend. Performs guest-to-account migration by merging `AppState` instances.
- Z12 (Settings): writes settings changes to the `settings_blob` field; reads the music library state and account state for rendering.
- Z11 (Modal Infrastructure): reads the music library state for the Shop modal contents; reads account state for the Account section in Settings.
- Z01 (Engine): reads the scenario history list when populating the Recap section.

### Contents

    #pragma once

    #include "engine/scenario_id.hpp"
    #include "persistence/sync_state.hpp"

    #include <array>
    #include <chrono>
    #include <cstdint>
    #include <optional>
    #include <string>
    #include <vector>

    namespace poker_trainer::persistence {

    // Schema version. Incremented when the on-disk format of AppState
    // changes incompatibly. Z04's load path checks this version and
    // either accepts the blob or runs a migration to the current version.
    inline constexpr std::uint32_t kCurrentSchemaVersion = 1;

    // The minimum schema version this build can read. Blobs older than
    // this are rejected with a fresh-state fallback. Currently equal to
    // the current version because there are no prior versions to migrate
    // from.
    inline constexpr std::uint32_t kMinSupportedSchemaVersion = 1;

    // Tomatoes accounting. The trainer has two distinct Tomatoes
    // counters: Spendable (earned, spent in Shop, decremented on
    // purchase) and Lifetime (earned, never decremented, used as the
    // leaderboard metric).
    struct TomatoesState {
        // Spendable Tomatoes balance. Earned by passing scenarios.
        // Decremented when the user purchases items from the Shop.
        std::uint64_t spendable{0};

        // Lifetime Tomatoes total. Earned by passing scenarios. Never
        // decremented. The leaderboard metric.
        std::uint64_t lifetime{0};
    };

    // Music library state. Tracks which music tracks the user has
    // unlocked via Shop purchase, and which tracks are currently in
    // each genre's shuffle pool (the user toggles tracks in and out
    // of the pool via the Settings Audio section).
    struct MusicLibraryState {
        // The set of MusicTrackId values the user has unlocked.
        // Stored as a sorted vector of raw track IDs for compact
        // serialization. Starter tracks (Track ID 0 in each genre)
        // are always considered unlocked regardless of presence in
        // this vector.
        std::vector<std::uint8_t> unlocked_track_ids;

        // The set of MusicTrackId values currently in each genre's
        // shuffle pool. Sorted vector of raw track IDs. By default,
        // each genre's pool contains only the starter track until
        // the user adds others via Settings.
        std::vector<std::uint8_t> active_pool_track_ids;
    };

    // One entry in the scenario history list. The history is bounded
    // (most-recent-N entries) and used to populate the Recap section
    // and detect repeat-scenario situations.
    struct ScenarioHistoryEntry {
        engine::ScenarioId scenario_id{};
        bool passed{false};
        std::uint32_t elapsed_ms{0};

        // Steady-clock timestamp of completion. Used for ordering
        // and for the Recap section's recency display.
        std::chrono::steady_clock::time_point completed_at{};
    };

    // The maximum number of scenario history entries retained. Older
    // entries are evicted when the list exceeds this size.
    inline constexpr std::size_t kMaxScenarioHistoryEntries = 256;

    // Account linkage state. Either the user is unauthenticated (guest)
    // or authenticated with an Auth0 user ID.
    struct AccountState {
        // True when the user has signed in via Auth0. False when in
        // guest mode.
        bool is_authenticated{false};

        // Auth0 user identifier ("sub" claim from the ID token).
        // Empty when is_authenticated is false.
        std::string auth0_user_id;

        // Display name from the Auth0 profile. Used in the Account
        // section of Settings and on the Leaderboard. Empty when in
        // guest mode.
        std::string display_name;

        // Email address from the Auth0 profile. Used for sign-in
        // identification only; never displayed publicly. Empty when
        // in guest mode.
        std::string email;
    };

    // Tutorial state. Tracks whether the user has been prompted to
    // start the tutorial and whether they've completed it.
    struct TutorialState {
        // True after the user has been shown the "Take the tutorial?"
        // prompt at first launch, regardless of whether they accepted
        // or skipped. Prevents the prompt from re-appearing.
        bool has_seen_tutorial_prompt{false};

        // True after the user has completed the tutorial (reached the
        // Tutorial Complete screen). Used by Z14 to skip the prompt
        // on subsequent launches.
        bool has_completed_tutorial{false};
    };

    // The top-level persisted application state. Everything stored
    // locally and synced to the backend lives in this struct.
    struct AppState {
        // The schema version this blob conforms to. Set to
        // kCurrentSchemaVersion when writing. Checked against the
        // supported range when reading.
        std::uint32_t schema_version{kCurrentSchemaVersion};

        // Settings blob. The full settings struct is too large to
        // inline here; it's serialized separately as a flat blob.
        // Z04 stores this as raw bytes (the serialized form of the
        // Settings struct from settings.hpp); Z12 deserializes on
        // read and serializes on write. The opaque-bytes approach
        // decouples the schema from the settings layout — a settings
        // field can be added without bumping the persistence schema
        // version, as long as the settings serializer handles missing
        // fields with their defaults.
        std::vector<std::uint8_t> settings_blob;

        // Tomatoes state.
        TomatoesState tomatoes;

        // Music library state.
        MusicLibraryState music_library;

        // Scenario history (bounded, most-recent-N).
        std::vector<ScenarioHistoryEntry> scenario_history;

        // Account linkage.
        AccountState account;

        // Tutorial state.
        TutorialState tutorial;
    };

    // Validation result for a loaded AppState blob.
    enum class SchemaValidationResult : std::uint8_t {
        // The blob is valid and matches the current schema version.
        Ok = 0,

        // The blob is valid but older than the current schema version;
        // migration is required before use.
        NeedsMigration = 1,

        // The blob's schema version is older than kMinSupportedSchemaVersion
        // or newer than kCurrentSchemaVersion. The blob cannot be loaded.
        Unsupported = 2,

        // The blob is structurally corrupt (e.g., truncated, invalid
        // checksum). The blob cannot be loaded.
        Corrupt = 3,
    };

    // Validate a loaded AppState's schema version field. Does not
    // validate the contents of individual fields — that's Z04's job
    // during the deserialization step.
    [[nodiscard]] constexpr SchemaValidationResult validate_schema_version(
        std::uint32_t version) noexcept {
        if (version == kCurrentSchemaVersion) {
            return SchemaValidationResult::Ok;
        }
        if (version >= kMinSupportedSchemaVersion &&
            version < kCurrentSchemaVersion) {
            return SchemaValidationResult::NeedsMigration;
        }
        return SchemaValidationResult::Unsupported;
    }

    // Returns true if an AppState represents an unauthenticated session.
    [[nodiscard]] constexpr bool is_guest_state(const AppState& state) noexcept {
        return !state.account.is_authenticated;
    }

    }  // namespace poker_trainer::persistence

### Notes

- The schema is versioned via `schema_version` at the top of `AppState`. Migration is Z04's responsibility; the version constants and validation function live here in Phase 0 so the schema version surface is part of the contract.
- The settings field is stored as `std::vector<std::uint8_t>` (opaque bytes) rather than inlining the full `Settings` struct. Rationale: the settings layout will evolve (new sections, new fields) far more frequently than the rest of the persistence schema. Decoupling means a settings change doesn't force a persistence schema version bump. Z12 owns the settings serialization format; Z04 just stores the bytes.
- The Tomatoes counts use `std::uint64_t`. A 64-bit unsigned integer is overkill in practice (the user would need to pass billions of scenarios), but the wasted bytes are trivial and the headroom prevents overflow concerns at any plausible play volume.
- `MusicLibraryState` stores raw `std::uint8_t` values rather than `MusicTrackId` enum values. The vectors are sorted on write for deterministic serialization. The starter tracks (track 0 per genre) are always implicitly unlocked; storing them in `unlocked_track_ids` is redundant but harmless. Z11 (Shop) and Z03 (Audio) treat starters as always-unlocked regardless of vector contents.
- Scenario history is bounded at 256 entries. The bound is chosen for two reasons: it covers a reasonable session length (a heavy user might do 100-200 scenarios in a sitting), and it keeps the serialized blob small enough that the per-second sync to the backend remains cheap. Entries beyond the bound are evicted oldest-first.
- `AccountState` stores the Auth0 user ID, display name, and email. These come from the Auth0 ID token at sign-in time. Email is stored locally for identification purposes but never displayed publicly. Display name is what shows on the Leaderboard.
- `TutorialState.has_seen_tutorial_prompt` and `has_completed_tutorial` are distinct fields. A user who declines the tutorial prompt has `has_seen_tutorial_prompt = true, has_completed_tutorial = false`. A user who completes the tutorial has both true. A user who hasn't been prompted yet (first launch) has both false; Z14 reads `has_seen_tutorial_prompt` to decide whether to show the prompt.
- The validation function is `constexpr` so the schema version check can happen at compile time when validating literal versions, and at runtime when validating loaded blobs.
- The integration test in Part 5 will verify that `AppState` is default-constructible, that `validate_schema_version(1)` returns `Ok`, and that `validate_schema_version(0)` and `validate_schema_version(999)` both return `Unsupported`.

---

End of Part 3. Part 4 covers the `settings.hpp` master settings struct, all 9 sections enumerated with types and defaults.

---

## 10. src/settings/settings.hpp

### Purpose

Defines the master `Settings` struct: a flat aggregate of every user-configurable preference in the trainer. The struct is the single source of truth for what the user has configured. It is read by every zone that needs to alter its behavior based on user preferences (which is nearly every zone), and it is mutated only via Z12 (Settings Page) when the user changes a setting.

The architecture spec lists 9 settings sections, each with multiple settings:

1. **Gameplay** — scenario difficulty, time pressure, scenario type filtering, multi-tier inclusion.
2. **Units** — global unit toggle (cash vs big blinds), street weight presets.
3. **Display** — theme picker, HUD toggle, countdown toggle, focus indicator preferences.
4. **Audio** — master volume, music volume, SFX volume, current music genre, music mute, SFX mute, autoplay gesture handling.
5. **Recap** — Post-Round Screen verbosity, transition style preferences.
6. **Tomatoes** — Lifetime Tomatoes display toggle, leaderboard opt-in.
7. **Account** — sign in / sign up / sign out actions, change password, delete account, display name.
8. **General** — language (English locked V1), keyboard mode auto-activation, scenario history retention.
9. **Legal** — Terms of Service, Privacy Policy, About sub-modal access.

This header enumerates **every field** across all 9 sections with its type, default value, validation range where applicable, and a brief purpose comment. Z12 (Settings) reads this struct to render the UI; every consumer zone reads the relevant fields.

The Settings struct is the largest and most content-dense file in Phase 0. Get the field list right — fields added or renamed after Phase 0 sign-off require coordinated updates across every consumer and a persistence migration.

### Dependencies

- `src/theme/theme_tokens.hpp` (for `kThemeIdNoLimit` and theme ID constants)

### Consumers

Effectively every zone. Major examples:

- Z01 (Engine): reads `gameplay.difficulty_*`, `gameplay.scenario_types_enabled`, `gameplay.include_multi_tier`, `gameplay.include_side_pots`, `gameplay.street_weights_*`, `units.cash_mode`.
- Z03 (Audio): reads `audio.master_volume`, `audio.music_volume`, `audio.sfx_volume`, `audio.current_music_genre`, `audio.music_muted`, `audio.sfx_muted`.
- Z06 (Theme): reads `display.active_theme_id`.
- Z08 (Game Screen): reads `display.show_hud`, `units.cash_mode`.
- Z09 (Math Interrogator): reads `gameplay.time_pressure_custom_seconds`, `gameplay.include_bet_sizing_inputs`.
- Z10 (Temporal): reads `display.show_countdown`, `gameplay.time_pressure_custom_seconds`, `gameplay.delta_timer_enabled`.
- Z11 (Modal Infrastructure): reads `tomatoes.show_lifetime_total`.
- Z12 (Settings): the master writer. Renders every section, mutates every field on user action.
- Z13 (Post-Round): reads `recap.transitions_enabled`, `recap.show_detailed_breakdown`.
- Z14 (Tutorial): reads `tutorial.has_seen_prompt` (technically lives in `persistence_schema.hpp`'s `TutorialState`, not here; settings overlap with persistence is minimal).

### Contents

    #pragma once

    #include "theme/theme_tokens.hpp"

    #include <array>
    #include <cstdint>
    #include <string>

    namespace poker_trainer::settings {

    // ----- Enumerations referenced across sections -----

    // Scenario type filter. The user can toggle which scenario types
    // are eligible for spawning. At least one must be enabled.
    enum class ScenarioTypeFilter : std::uint8_t {
        Aggressor = 0,
        Caller = 1,
    };

    inline constexpr std::size_t kScenarioTypeCount = 2;

    // Street weight preset. The user picks one preset (or custom) to
    // bias scenario spawn toward specific betting streets.
    enum class StreetWeightPreset : std::uint8_t {
        Uniform = 0,        // Equal weight across preflop/flop/turn/river
        FlopHeavy = 1,      // Weighted toward flop scenarios
        TurnHeavy = 2,      // Weighted toward turn scenarios
        RiverHeavy = 3,     // Weighted toward river scenarios
        Custom = 4,         // Use street_weights_custom_* fields
    };

    // Time pressure preset. The user picks a preset or "Off" or "Custom".
    enum class TimePressurePreset : std::uint8_t {
        Off = 0,            // No time pressure; countdown hidden
        Beginner = 1,       // Generous time (target * 2.0)
        Standard = 2,       // Target time (1.0x)
        Aggressive = 3,     // Tight time (target * 0.75)
        Brutal = 4,         // Very tight (target * 0.5)
        Custom = 5,         // Use time_pressure_custom_seconds
    };

    // Music genre selection (one active genre at a time).
    enum class ActiveMusicGenre : std::uint8_t {
        LoungeJazz = 0,
        Classical = 1,
        BossaNova = 2,
        Ambient = 3,
        // Special: no music plays. Distinct from muted (muted preserves
        // the active genre selection so unmuting resumes; None means
        // the user explicitly does not want music playing).
        None = 255,
    };

    // Language. English is the only supported language in V1; the enum
    // is here for forward compatibility.
    enum class Language : std::uint8_t {
        English = 0,
    };

    // ----- Sub-structs per settings section -----

    // ----- Section 1: Gameplay -----

    struct GameplaySettings {
        // Lower bound of the difficulty range, 0.0 to 1.0 (display
        // shows 0-100 to the user; internal storage is 0-1).
        // Default 0.2.
        float difficulty_min{0.2f};

        // Upper bound of the difficulty range, 0.0 to 1.0.
        // Must be >= difficulty_min. Default 0.8.
        float difficulty_max{0.8f};

        // Time pressure preset.
        TimePressurePreset time_pressure_preset{TimePressurePreset::Standard};

        // Custom time pressure value in seconds (only used when
        // time_pressure_preset == Custom). Range: 5-300. Default 30.
        std::uint16_t time_pressure_custom_seconds{30};

        // Which scenario types are enabled. At least one must be true.
        // The integration test asserts that at least one is true on
        // construction.
        std::array<bool, kScenarioTypeCount> scenario_types_enabled{{
            true,  // Aggressor
            true,  // Caller
        }};

        // Include multi-tier Aggressor scenarios (Pure Bluff with
        // multiple bet-size tiers to evaluate).
        bool include_multi_tier{true};

        // Include side-pot scenarios (multi-way all-in situations
        // with side pot calculations).
        bool include_side_pots{true};

        // Street weight preset.
        StreetWeightPreset street_weights_preset{StreetWeightPreset::Uniform};

        // Custom street weights (only used when street_weights_preset
        // == Custom). Each value 0.0 to 1.0; the four values are
        // normalized to sum to 1.0 at use time. Defaults: uniform.
        float street_weights_custom_preflop{0.25f};
        float street_weights_custom_flop{0.25f};
        float street_weights_custom_turn{0.25f};
        float street_weights_custom_river{0.25f};

        // Delta Timer (the live elapsed-time display) enabled.
        // Independent from the Visual Countdown. Default true.
        bool delta_timer_enabled{true};

        // Include the bet sizing input row in Aggressor scenarios.
        // When false, scenarios skip the bet-size component (still
        // valid for solo math practice). Default true.
        bool include_bet_sizing_inputs{true};
    };

    // ----- Section 2: Units -----

    struct UnitsSettings {
        // True means cash mode (dollar amounts displayed); false
        // means big blinds mode (BB multiples displayed). Affects
        // every numeric value shown on the Game screen and in
        // math inputs.
        bool cash_mode{true};

        // Big blind value in cents, used in cash mode to convert
        // between cash and BB representations internally. Always
        // 100 cents ($1) for V1. Range: 25-10000. Default 100.
        std::uint16_t big_blind_value_cents{100};
    };

    // ----- Section 3: Display -----

    struct DisplaySettings {
        // Active theme. One of kThemeIdNoLimit / kThemeIdSlate /
        // kThemeIdOcean / kThemeIdSage from theme_tokens.hpp.
        // Default kThemeIdNoLimit.
        std::uint8_t active_theme_id{theme::kThemeIdNoLimit};

        // Show the HUD overlay (pot, blinds, floating bet amount).
        // Default true.
        bool show_hud{true};

        // Show the Visual Countdown timer (top-right below cluster).
        // Independent from time pressure being active. Default true.
        bool show_countdown{true};

        // Show position indicators (UTG/HJ/CO/BTN/SB/BB) at each
        // opponent seat. Default true.
        bool show_position_indicators{true};

        // Auto-activate keyboard focus mode on first Tab press.
        // When false, the user must explicitly toggle keyboard mode
        // via a separate keybind. Default true.
        bool keyboard_mode_auto_activate{true};
    };

    // ----- Section 4: Audio -----

    struct AudioSettings {
        // Master volume, 0-100. Applied as a multiplier on both
        // music and SFX. Default 80.
        std::uint8_t master_volume{80};

        // Music stream volume, 0-100. Default 60.
        std::uint8_t music_volume{60};

        // SFX volume, 0-100. Default 75.
        std::uint8_t sfx_volume{75};

        // Active music genre. The shuffle pool draws from this
        // genre's unlocked tracks.
        ActiveMusicGenre current_music_genre{ActiveMusicGenre::LoungeJazz};

        // Music globally muted. When true, music is silenced
        // regardless of music_volume.
        bool music_muted{false};

        // SFX globally muted. When true, all SFX are silenced
        // regardless of sfx_volume.
        bool sfx_muted{false};

        // Defer audio playback until the user makes their first
        // gesture (click or keypress). Required by browser autoplay
        // policies. Default true (recommended for browser deployment).
        bool defer_until_user_gesture{true};
    };

    // ----- Section 5: Recap -----

    struct RecapSettings {
        // Enable smooth screen transitions (slide between Game and
        // Post-Round, ceremonial transition between Mode Selection
        // and Game). When false, transitions are instant cuts.
        // Default true.
        bool transitions_enabled{true};

        // Show the detailed breakdown row in the Post-Round stat
        // modal. When false, only the summary row is shown.
        // Default true.
        bool show_detailed_breakdown{true};

        // Auto-advance from Post-Round to next scenario after the
        // configured delay. When false, the user must click Again
        // explicitly. Default false (explicit click is the architecture
        // default; auto-advance is an opt-in).
        bool auto_advance{false};

        // Auto-advance delay in seconds (only used when auto_advance
        // is true). Range: 1-30. Default 5.
        std::uint8_t auto_advance_delay_seconds{5};
    };

    // ----- Section 6: Tomatoes -----

    struct TomatoesSettings {
        // Show the user's Lifetime Tomatoes total in the Account /
        // Profile view. Spendable Tomatoes always show in the Shop;
        // this toggle only affects the lifetime counter display.
        // Default true.
        bool show_lifetime_total{true};

        // Opt into the public leaderboard. When true, the user's
        // display name and Lifetime Tomatoes are published. When
        // false, the user can still view the leaderboard but is
        // not listed on it. Requires an authenticated account
        // (guests cannot opt in). Default false.
        bool leaderboard_opt_in{false};
    };

    // ----- Section 7: Account -----

    // Account settings are mostly action-driven (sign-in modal, etc.)
    // rather than persistent flags. The only persistent field is
    // display name, which is editable separately from the Auth0
    // profile display name. The actual auth state lives in
    // persistence_schema.hpp's AccountState, not here.

    struct AccountSettings {
        // Locally-edited display name override. When empty, the
        // Auth0 profile display name is used. When non-empty, this
        // overrides the Auth0 name for leaderboard and UI display.
        // Max length 32 characters. The integration test asserts the
        // default value is empty.
        std::string display_name_override;
    };

    // ----- Section 8: General -----

    struct GeneralSettings {
        // UI language. English is the only supported value in V1.
        Language language{Language::English};

        // Maximum scenario history entries to retain (mirrors the
        // kMaxScenarioHistoryEntries cap in persistence_schema.hpp
        // but allows the user to set a lower bound for privacy).
        // Range: 0 (no history) to 256 (full). Default 256.
        std::uint16_t scenario_history_retention{256};
    };

    // ----- Section 9: Legal -----

    // Legal section is action-driven: it opens sub-modals showing
    // ToS, Privacy Policy, and About content. No persistent fields
    // beyond the version of the policies the user has acknowledged.

    struct LegalSettings {
        // The latest Terms of Service version the user has read or
        // acknowledged. Used to surface a re-acknowledgment prompt
        // when the ToS is updated. Default 0 (never acknowledged).
        std::uint32_t acknowledged_tos_version{0};

        // The latest Privacy Policy version the user has read or
        // acknowledged. Default 0.
        std::uint32_t acknowledged_privacy_version{0};
    };

    // ----- Master settings struct -----

    // The full Settings object. Aggregates all 9 sections. Default
    // construction yields a fully-valid settings state with the
    // documented defaults; first-launch users start here.
    struct Settings {
        GameplaySettings gameplay;
        UnitsSettings units;
        DisplaySettings display;
        AudioSettings audio;
        RecapSettings recap;
        TomatoesSettings tomatoes;
        AccountSettings account;
        GeneralSettings general;
        LegalSettings legal;
    };

    // ----- Validation -----

    // Result of validating a Settings instance. Used by Z12 after
    // user mutations to detect invariant violations (e.g., user
    // disabled both scenario types). When invalid, Z12 reverts the
    // most recent mutation rather than committing an unusable state.
    enum class SettingsValidationResult : std::uint8_t {
        Ok = 0,

        // At least one scenario type must be enabled.
        NoScenarioTypeEnabled = 1,

        // difficulty_min must be <= difficulty_max, both in [0.0, 1.0].
        InvalidDifficultyRange = 2,

        // time_pressure_custom_seconds out of range when preset is Custom.
        InvalidTimePressureCustom = 3,

        // big_blind_value_cents out of range.
        InvalidBigBlindValue = 4,

        // Volume out of range (master/music/sfx, 0-100).
        InvalidVolumeValue = 5,

        // display_name_override exceeds 32 characters.
        InvalidDisplayNameOverride = 6,

        // scenario_history_retention exceeds 256.
        InvalidScenarioHistoryRetention = 7,

        // auto_advance_delay_seconds out of range (1-30).
        InvalidAutoAdvanceDelay = 8,

        // Street weight preset is Custom but the four custom values
        // sum to zero (cannot normalize).
        InvalidStreetWeightsCustom = 9,
    };

    // Validate a Settings instance. Returns Ok on success, otherwise
    // returns the first invariant violation encountered. Z12 uses
    // this to gate mutations.
    [[nodiscard]] SettingsValidationResult validate(const Settings& s) noexcept;

    // Maximum length for the display name override field, in characters.
    inline constexpr std::size_t kMaxDisplayNameOverrideLength = 32;

    }  // namespace poker_trainer::settings

### Notes

- The 9-section structure mirrors the architecture spec's Settings Page Specification. The order of sections in the master struct matches the order shown to the user in the Settings sidebar (Gameplay → Units → Display → Audio → Recap → Tomatoes → Account → General → Legal).
- Every field has an explicit default. The defaults are chosen to give a reasonable first-launch experience: HUD on, countdown on, transitions enabled, both scenario types enabled, multi-tier and side pots enabled, standard time pressure, English language, default theme. A user who never opens Settings sees a trainer that "just works."
- The internal difficulty representation is `float` in `[0.0, 1.0]`. The Settings UI in Z12 displays this as an integer in `[0, 100]` per the architecture spec. The conversion is a multiplication by 100; no precision is lost at the resolutions involved.
- `ActiveMusicGenre::None = 255` uses a sentinel value rather than crowding the genre enum. This is intentional: `None` is not a music genre, it's the absence of one. Treating it as a special value separates "what genre is selected" from "is music selected at all."
- The split between `music_muted` and `current_music_genre == None` is deliberate: muting preserves the genre selection so toggling mute restores the previous genre. Setting genre to None explicitly removes music as a preference. Both states are independently meaningful.
- The street weight custom fields (`street_weights_custom_preflop` etc.) sum to 1.0 by default. They are normalized at use time, so any positive-sum combination is valid. The validation function rejects the all-zero case because it cannot be normalized.
- `AccountSettings::display_name_override` is intentionally an `std::string` (allocates). Display names are user-typed text; the allocation cost is negligible compared to the user typing the value. The 32-character limit aligns with typical username constraints.
- The validation enum lists every invariant explicitly. When Z12 mutates a setting, it calls `validate()` and reverts the mutation if the result is non-Ok. This avoids the situation where the user changes one setting and another silently becomes invalid.
- `validate()` is declared in Phase 0 but implemented in Z12. The Phase 0 integration test does not call it; it only verifies the header compiles. The implementation lives at `src/settings/settings_validation.cpp` (Z12's owns list).
- Schema migration concerns: when a new field is added in a later release, the persistence schema must handle the case where the loaded blob does not contain the new field. The convention is that on read, missing fields take their default values from this struct's default initializers. Z04's settings deserializer must implement this fallback.
- The Settings struct is **not** aligned with `persistence_schema.hpp`'s `AppState::settings_blob` field directly. Z12 serializes the Settings struct into the blob using its own format (likely a versioned binary format or JSON; the choice is Z12's). The Phase 0 contract only specifies the Settings shape and the validation interface, not the serialization wire format.
- The integration test (Part 5) will instantiate a default `Settings`, call `validate()` against it (when Z12 stubs the function), confirm Ok, then test boundary conditions: setting both scenario types to false should yield `NoScenarioTypeEnabled`; setting `difficulty_max < difficulty_min` should yield `InvalidDifficultyRange`; etc.

---

End of Part 4. Part 5 covers the six backbone primitives (`animation_clock.hpp`, `screen_state.hpp`, `event_router.hpp`, `focus_manager.hpp`, `scenario_events.hpp`, `modal_state.hpp`) plus the Phase 0 integration test and closing matter.

---

## 11. src/backbone/animation_clock.hpp

### Purpose

Defines the **interface only** for the animation clock — the single source of truth for elapsed time in frame-level animations. Every animation in the trainer (button morphs, modal slides, chip pushes, dealer fade-ins, countdown ticks) reads from this clock. No zone may maintain its own time tracking for frame animations; this is one of the non-negotiable architectural rules.

The clock has two distinct concerns:

1. **Wall-clock elapsed time** — monotonic milliseconds since the application started. Used for animations that should never pause (loading screen progress, the offline indicator's pulse).
2. **Animation time** — the same monotonic time, but pausable. Pauses when any modal is open (so animations freeze behind a modal and resume on close). This is the time source nearly every animation actually reads.

The interface is declared in Phase 0; the implementation lives in Z05 (Emscripten Bridge / Main Loop) because the main loop drives the clock tick.

This is an interface-only header. No `.cpp` file is produced in Phase 0. Z05 will produce `src/bridge/animation_clock_impl.cpp` (or equivalent) in Wave 2.

### Dependencies

None beyond the standard library.

### Consumers

- Z05 (Bridge): the implementer. Ticks the clock once per frame as part of the main loop.
- Z07, Z08, Z11, Z13 (anything that animates): reads `animation_time_ms()` for animation progress.
- Z10 (Temporal): reads `animation_time_ms()` for the Delta Timer and Visual Countdown (which pause with modals).
- Z11 (Modal Infrastructure): calls `pause()` when a modal opens and `resume()` when the last modal closes.

### Contents

    #pragma once

    #include <cstdint>

    namespace poker_trainer::backbone {

    // Monotonic wall-clock time since app start, in milliseconds. Never
    // pauses. Suitable for animations that must run regardless of modal
    // state (loading screen, offline indicator pulse, outage banner
    // countdown bar).
    [[nodiscard]] std::uint64_t wall_clock_ms() noexcept;

    // Pausable animation time, in milliseconds. Pauses while any modal
    // is open; resumes when all modals close. Suitable for the bulk of
    // animations (button morphs, modal slides, chip pushes, dealer
    // fade-ins, countdown ticks). When paused, repeated calls return
    // the same value until resume() is called.
    [[nodiscard]] std::uint64_t animation_time_ms() noexcept;

    // Returns true if animation_time_ms() is currently paused.
    [[nodiscard]] bool is_animation_paused() noexcept;

    // Pause animation_time_ms(). Idempotent: calling pause() while
    // already paused is a no-op. Called by Z11 when a modal opens.
    // Multiple modal opens stack: the clock stays paused until all
    // modals have closed.
    void pause() noexcept;

    // Resume animation_time_ms(). Idempotent: calling resume() while
    // already running is a no-op. Called by Z11 when the last modal
    // closes. The internal pause counter decrements; when it hits
    // zero, the clock resumes.
    void resume() noexcept;

    // Advance the clock by the given delta. Called once per frame by
    // Z05 with the frame's elapsed time. The implementation maintains
    // separate counters for wall-clock and animation time; both
    // advance by `delta_ms` unless the animation time is paused.
    //
    // This function is part of the Z05-internal API surface, exposed
    // here so the integration test can drive the clock deterministically.
    // No other zone should call this.
    void tick(std::uint64_t delta_ms) noexcept;

    // Reset all clock state to zero. Used by the integration test to
    // restore a clean state between test cases. No other zone should
    // call this.
    void reset_animation_clock_for_testing() noexcept;

    }  // namespace poker_trainer::backbone

### Notes

- This is **interface only** during Phase 0. No `.cpp` file is produced. Phase 0's integration test includes this header but does not link against an implementation that exists yet. To make the integration test link cleanly, Phase 0 produces a stub implementation file `src/backbone/animation_clock_stub.cpp` that returns zeros and ignores all calls. The stub is replaced by Z05's real implementation in Wave 2 (the stub file is deleted when Z05's implementation lands).
- The pause counter is internal to the implementation. Z11 may call `pause()` multiple times (one per modal open); each call increments the pause counter. `resume()` decrements; the clock unpauses only when the counter returns to zero. This matches the modal stacking behavior — nested modals all keep the clock paused, and the clock resumes only when the user has closed back to the underlying screen.
- `wall_clock_ms()` and `animation_time_ms()` both start at 0 at app launch. They are monotonic; no implementation may return a smaller value than a prior call. The integration test asserts monotonicity by calling each function twice and verifying the second value is >= the first.
- `tick()` and `reset_for_testing()` are intentionally exposed in the public interface even though only Z05 and the integration test should call them. The alternative — splitting them into a "private" header — is more ceremony than it's worth for two functions. The comments document the intent.
- The architecture spec (Notes — Communication Backbone) names the animation clock as one of the six backbone primitives. The other five also live in `src/backbone/`.

---

## 12. src/backbone/screen_state.hpp

### Purpose

Defines the screen state singleton: a process-global record of which screen the user is currently on (Root, Mode Selection, Game, or Post-Round) and which scenario (if any) is active. Every zone that varies behavior by screen reads from this singleton; transitions between screens are mediated by this singleton's mutator API.

The screen state is one of the six backbone primitives. It lives in `src/backbone/` because it is shared global state with no natural single owner — every zone reads it, transitions happen from multiple zones, and the alternative (passing screen state through function arguments everywhere) would be far worse.

### Dependencies

- `src/engine/scenario_id.hpp` (the active scenario, when on Game or Post-Round, is identified by `ScenarioId`)

### Consumers

- Z05 (Bridge): drives top-level screen transitions (Root ↔ Mode Selection, Mode Selection → Game on scenario start, Game → Post-Round on scenario completion).
- Z07, Z08, Z13: each renders only when the current screen matches its responsibility.
- Z09 (Math Interrogator): reads the active scenario for input rendering.
- Z11 (Modal Infrastructure): reads the current screen to render the per-screen cluster variant (fourth cluster icon differs by screen).
- Z14 (Tutorial): reads and writes screen state during the scripted tutorial flow.

### Contents

    #pragma once

    #include "engine/scenario_id.hpp"

    #include <cstddef>
    #include <cstdint>
    #include <optional>

    namespace poker_trainer::backbone {

    // The set of top-level screens. The application is on exactly one
    // of these at any time.
    enum class ScreenId : std::uint8_t {
        Root = 0,
        ModeSelection = 1,
        Game = 2,
        PostRound = 3,
        Error = 4,  // The Tier 1/2 fatal failure screen (Z05)
    };

    inline constexpr std::size_t kScreenCount = 5;

    // Snapshot of the screen state at the moment of read.
    struct ScreenStateSnapshot {
        ScreenId current{ScreenId::Root};

        // When current == Game or current == PostRound, this is the
        // ScenarioId of the active scenario. When current is any other
        // screen, this is std::nullopt.
        std::optional<engine::ScenarioId> active_scenario;
    };

    // Read the current screen state.
    [[nodiscard]] ScreenStateSnapshot read_screen_state() noexcept;

    // Set the current screen. Called by Z05 and Z14 to drive transitions.
    // When transitioning into Game, the active_scenario must be provided.
    // When transitioning into any other screen, active_scenario should
    // be std::nullopt; the implementation clears it if provided.
    void set_screen(ScreenId screen,
                    std::optional<engine::ScenarioId> active_scenario) noexcept;

    // Convenience: returns true if the current screen is Game or
    // PostRound (i.e., a scenario is active or just completed).
    [[nodiscard]] bool is_in_scenario() noexcept;

    // Reset to initial state (Root, no scenario). Used by the integration
    // test only.
    void reset_screen_state_for_testing() noexcept;

    }  // namespace poker_trainer::backbone

### Notes

- The Error screen is included as a top-level screen state because Z05 needs to render it when Tier 1/2 asset loading fails fatally. From the user's perspective the Error screen is a terminal state — they can only retry, which reloads the page and starts the boot sequence over.
- The active scenario is a `std::optional<ScenarioId>` rather than a sentinel `kInvalidScenarioId`. The optional makes the "scenario is or is not active" question explicit at the type level.
- `is_in_scenario()` is provided for convenience because the check `current == Game || current == PostRound` is written in many places. Centralizing it here prevents the question "does Post-Round count as in-scenario?" from being answered differently in different zones (it does, because the recap is still showing the just-completed scenario).
- The Phase 0 implementation `src/backbone/screen_state.cpp` provides the singleton storage. The pattern is identical to `sync_state.hpp` — a seqlock-style version counter ensures coherent reads even though writes are single-threaded.
- Z14 (Tutorial) is the unusual case where a non-Z05 zone writes screen state. This is permitted because the tutorial's job is to script transitions through screens; doing so requires direct write access. The architectural rule about "no zone writes to backbone state except the owning zone" has Z14 as a documented exception for screen_state.

---

## 13. src/backbone/event_router.hpp

### Purpose

The central dispatcher for keyboard and mouse events. Every input event flows through this router before reaching any zone-specific handler. The router maintains a priority-ordered stack of handlers; the topmost handler that claims an event consumes it.

This is the architectural rule's mechanism: "no zone may register a global keyboard handler outside the event router." All keyboard handling — Tab cycling, arrow keys, Enter submission, Escape dismissal, the Frog easter egg's click counter — goes through this router.

### Dependencies

- `src/backbone/screen_state.hpp` (handlers may want to check current screen)

### Consumers

- Z05 (Bridge): forwards browser-native input events into the router.
- Z07, Z08, Z09, Z11, Z12, Z13, Z14: register handlers when their screens or modals are active; unregister when they go away.
- Z14 (Tutorial): registers a special escape handler at high priority (below the modal layer) for the tutorial skip confirmation flow.

### Contents

    #pragma once

    #include "backbone/screen_state.hpp"

    #include <cstdint>
    #include <functional>
    #include <string_view>

    namespace poker_trainer::backbone {

    // Keyboard event types the router dispatches.
    enum class KeyEventType : std::uint8_t {
        KeyDown = 0,
        KeyUp = 1,
    };

    // Mouse event types the router dispatches.
    enum class MouseEventType : std::uint8_t {
        MouseDown = 0,
        MouseUp = 1,
        MouseMove = 2,
        Wheel = 3,
    };

    // Key codes. Subset of physical keys the trainer cares about. The
    // bridge layer maps browser key events to these codes.
    enum class KeyCode : std::uint16_t {
        Unknown = 0,
        Tab = 1,
        Enter = 2,
        Escape = 3,
        Space = 4,
        ArrowUp = 5,
        ArrowDown = 6,
        ArrowLeft = 7,
        ArrowRight = 8,
        Backspace = 9,
        Delete = 10,

        // Digit row (used for math input boxes and bet size tier selection).
        Digit0 = 16, Digit1 = 17, Digit2 = 18, Digit3 = 19, Digit4 = 20,
        Digit5 = 21, Digit6 = 22, Digit7 = 23, Digit8 = 24, Digit9 = 25,

        // Letter keys (rare; mostly for the search bar fuzzy-match input).
        LetterA = 32, LetterB = 33, /* ... through LetterZ = 57. Full
        enumeration is implementation-only; the integration test does
        not exercise letter keys. */
    };

    // Modifier mask. Bits OR'd together when multiple modifiers are held.
    enum class ModMask : std::uint8_t {
        None = 0,
        Shift = 1 << 0,
        Ctrl = 1 << 1,
        Alt = 1 << 2,
        Meta = 1 << 3,  // Command on macOS, Windows key on Windows
    };

    [[nodiscard]] constexpr ModMask operator|(ModMask a, ModMask b) noexcept {
        return static_cast<ModMask>(
            static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
    }

    [[nodiscard]] constexpr bool has_mod(ModMask mask, ModMask flag) noexcept {
        return (static_cast<std::uint8_t>(mask) &
                static_cast<std::uint8_t>(flag)) != 0;
    }

    // A keyboard event passed to handlers.
    struct KeyEvent {
        KeyEventType type;
        KeyCode code;
        ModMask mods;
    };

    // A mouse event passed to handlers.
    struct MouseEvent {
        MouseEventType type;
        float x;            // Canvas coordinates, top-left origin
        float y;
        std::int32_t button;  // 0 = left, 1 = middle, 2 = right (for MouseDown/Up)
        float wheel_dy;       // Vertical scroll delta (for Wheel; 0 otherwise)
    };

    // Handler return value: true if the handler consumed the event,
    // false to pass through to the next handler in the stack.
    using KeyHandler = std::function<bool(const KeyEvent&)>;
    using MouseHandler = std::function<bool(const MouseEvent&)>;

    // Opaque handle for an installed handler. Returned by install_*
    // functions; passed to uninstall_handler to remove the handler.
    struct HandlerHandle {
        std::uint64_t value{0};
        constexpr bool operator==(const HandlerHandle&) const noexcept = default;
    };

    inline constexpr HandlerHandle kInvalidHandlerHandle{0};

    // Handler priority levels. Lower numbers run first (higher priority).
    // The router walks the stack in ascending priority order; the first
    // handler that returns true consumes the event.
    enum class HandlerPriority : std::uint8_t {
        ModalLayer = 0,           // Top: modal handlers (Esc-to-close, etc.)
        TutorialOverlay = 1,      // Z14's escape handler during tutorial
        ScreenContext = 2,        // Per-screen handlers (Z07, Z08, Z13)
        BackgroundCatchAll = 3,   // Lowest: catch-all (default behaviors)
    };

    // Install a keyboard handler at the given priority. Returns a handle
    // that can be used to uninstall the handler later. Handler is owned
    // by the router until uninstalled.
    //
    // The `tag` parameter is a human-readable string used for debugging
    // (printed in router logs when the handler is installed/uninstalled).
    // It does not affect behavior.
    HandlerHandle install_key_handler(KeyHandler handler,
                                      HandlerPriority priority,
                                      std::string_view tag) noexcept;

    // Install a mouse handler. Same semantics as install_key_handler.
    HandlerHandle install_mouse_handler(MouseHandler handler,
                                        HandlerPriority priority,
                                        std::string_view tag) noexcept;

    // Uninstall a previously-installed handler. The handle is invalidated
    // by this call. Calling uninstall on an invalid handle is a no-op.
    void uninstall_handler(HandlerHandle handle) noexcept;

    // Dispatch a keyboard event through the handler stack. Called by Z05
    // from the browser event bridge.
    void dispatch_key_event(const KeyEvent& event) noexcept;

    // Dispatch a mouse event through the handler stack.
    void dispatch_mouse_event(const MouseEvent& event) noexcept;

    // Clear all installed handlers. Used by the integration test only.
    void reset_event_router_for_testing() noexcept;

    }  // namespace poker_trainer::backbone

### Notes

- The handler stack is priority-ordered, not insertion-ordered. A modal-layer handler installed after a screen-context handler still runs first because its priority is lower. This is essential for modal focus traps — the modal can install its handler late but still capture events first.
- The `tag` parameter on installation is debug-only. It does not affect dispatch behavior. In production builds the tag may be discarded; in debug builds it's included in trace output.
- Handlers are `std::function`, which incurs a small-object-optimization-eligible heap allocation for non-trivial captures. This is acceptable because handler installations are rare (a few per screen transition) and dispatch frequency is bounded by the event rate.
- The Frog easter egg counter is a mouse handler installed by Z08 at `HandlerPriority::ScreenContext` that intercepts clicks on the dealer asset. The counter increments on each click; on reaching the trigger threshold, Z08 toggles the Butler/Frog asset and fires the Frog toggle SFX.
- Z14's tutorial escape handler is installed at `HandlerPriority::TutorialOverlay`, which is below `ModalLayer` (modals still get Escape first if any are open) but above `ScreenContext` (the tutorial intercepts before normal screen handlers see it). The architectural spec specifies this precedence.
- The Phase 0 implementation `src/backbone/event_router.cpp` provides the handler stack storage and dispatch loop. It is non-trivial: a sorted vector of `(priority, handle, handler)` tuples with O(log N) installation and O(N) dispatch where N is the active handler count (typically < 10). This is the largest Phase 0 implementation file.

---

## 14. src/backbone/focus_manager.hpp

### Purpose

Manages the keyboard focus pointer: which UI element is currently focused for keyboard navigation. The focus manager is the single source of truth for focus state. No zone may directly manipulate the focus pointer or render its own focus indicator; both flow through this primitive.

The focus manager supports a stack of focus contexts. When a modal opens, the modal pushes a new context (with its own focusable list and focus pointer); when the modal closes, the context is popped, restoring the prior focus. This is how modal focus traps work without zones needing to manually save and restore focus state.

### Dependencies

- `src/backbone/event_router.hpp` (focus interacts with keyboard events)

### Consumers

- Z05 (Bridge): drives focus advancement on Tab keypresses (calls the focus manager from a low-priority key handler).
- Z07, Z08, Z09, Z11, Z12, Z13, Z14: each registers a focus list when their screen or modal becomes active.
- The rendering layer (every zone with visible UI): reads the focused element id to render the focus indicator.

### Contents

    #pragma once

    #include "backbone/event_router.hpp"

    #include <cstddef>
    #include <cstdint>
    #include <span>
    #include <string_view>
    #include <vector>

    namespace poker_trainer::backbone {

    // A focusable element identifier. Each focusable element in the UI
    // has a unique ID. IDs are assigned by zones when they register
    // focus lists; the focus manager treats them as opaque.
    //
    // Convention: zones use hashed string literals for IDs (compile-time
    // computed via a constexpr fnv-1a or similar). This gives stable IDs
    // across builds without manually maintained ID assignments.
    struct FocusableId {
        std::uint64_t value{0};
        constexpr bool operator==(const FocusableId&) const noexcept = default;
        constexpr auto operator<=>(const FocusableId&) const noexcept = default;
    };

    inline constexpr FocusableId kInvalidFocusableId{0};

    // Sentinel: focus is on nothing. Used when keyboard mode is inactive
    // or when a context has no focusable elements.
    inline constexpr FocusableId kNoFocus{0};

    // Read the currently focused element. Returns kNoFocus when keyboard
    // mode is inactive or no element is focused.
    [[nodiscard]] FocusableId current_focus() noexcept;

    // Returns true if keyboard navigation mode is currently active.
    // Becomes true on first Tab press; remains true until reset
    // (typically via mouse interaction returning the user to mouse mode,
    // though the trainer does not exit keyboard mode automatically per
    // the architecture spec).
    [[nodiscard]] bool is_keyboard_mode_active() noexcept;

    // Activate keyboard navigation mode. Called from Z05's Tab handler.
    // If the setting display.keyboard_mode_auto_activate is true, this
    // is called automatically on first Tab. Otherwise, the user must
    // explicitly activate via a separate keybind.
    void activate_keyboard_mode() noexcept;

    // Snap focus to a specific element. Used when entering a new screen
    // or modal to set the initial focus, and when arrow keys move focus
    // within a bounded group (like the bet size tier buttons or a
    // coupled-slider group).
    void snap_focus_to(FocusableId target) noexcept;

    // Advance focus to the next element in the current context. Wraps
    // to the first element when called past the last. Called by Z05's
    // Tab handler.
    void advance_focus(bool reverse) noexcept;

    // Register a focus list for the current screen. Replaces the
    // existing list (used at screen transitions). The order of the
    // span determines Tab order.
    void register_focus_list(std::span<const FocusableId> focusables) noexcept;

    // Push a new focus context onto the stack. Used when a modal opens.
    // The current focus is saved; the new context starts with the
    // provided focusables and the provided initial focus.
    //
    // The `tag` parameter is a human-readable string for debugging.
    void push_focus_context(std::span<const FocusableId> focusables,
                            FocusableId initial_focus,
                            std::string_view tag) noexcept;

    // Pop the topmost focus context. Used when a modal closes. The
    // prior context's focus is restored. If the stack is at its
    // base context, this is a no-op (the base context is the
    // permanent screen-level context).
    void pop_focus_context() noexcept;

    // The current focus context depth. 0 means base (screen) context;
    // each modal push increments by 1. Used by debug tooling.
    [[nodiscard]] std::size_t context_depth() noexcept;

    // Reset all focus state to initial (no focus, no contexts beyond
    // base, keyboard mode inactive). Used by the integration test.
    void reset_focus_manager_for_testing() noexcept;

    // Compile-time helper: compute a FocusableId from a string literal
    // via FNV-1a hash. This is the recommended way to define focusable
    // IDs in zone code.
    //
    // Example:
    //     inline constexpr FocusableId kFocusPlayButton =
    //         make_focusable_id("root.play_button");
    [[nodiscard]] constexpr FocusableId make_focusable_id(
        std::string_view name) noexcept {
        std::uint64_t hash = 0xCBF29CE484222325ULL;  // FNV-1a 64-bit offset basis
        for (char c : name) {
            hash ^= static_cast<std::uint64_t>(static_cast<std::uint8_t>(c));
            hash *= 0x100000001B3ULL;  // FNV-1a 64-bit prime
        }
        // Reserve 0 as kNoFocus / invalid; if the hash collides with 0,
        // remap to 1. Collision probability is negligible.
        if (hash == 0) {
            hash = 1;
        }
        return FocusableId{hash};
    }

    }  // namespace poker_trainer::backbone

### Notes

- Focusable IDs are 64-bit hashes of string names. The hash is computed at compile time via the `make_focusable_id()` helper. Zones define their focusable IDs as `inline constexpr` values; this gives stable IDs that survive recompilation and don't require a central registry. Hash collisions are vanishingly unlikely at the scale of focusable elements in this app (low hundreds).
- The focus stack allows modal focus traps to nest naturally. The base context is the current screen's focus list; modal opens push, modal closes pop. The architectural spec specifies this stack-based behavior.
- `register_focus_list()` replaces the base context's list (used at screen transitions). `push_focus_context()` adds a new context above it (used when modals open). These are different operations; do not conflate.
- Keyboard mode is sticky: once activated (via Tab press), it remains active for the session. The architecture spec does not specify an automatic deactivation, so this implementation follows suit. A user can use the mouse without leaving keyboard mode; mouse clicks do not deactivate it.
- The focus indicator visual (a 2px outline using the `BorderFocus` token from `theme_tokens.hpp`) is rendered by the consuming zone, not by the focus manager. The focus manager only tracks state; rendering is the zone's responsibility, querying `current_focus()` and comparing against its own element IDs.
- `kNoFocus` (value 0) is distinct from "keyboard mode inactive but focus would be on element X if active." When keyboard mode is inactive, `current_focus()` returns `kNoFocus` regardless of any prior snap_focus_to() calls. When keyboard mode is active and no element is focused (e.g., an empty context), it also returns `kNoFocus`.
- The Phase 0 implementation `src/backbone/focus_manager.cpp` provides the focus state and the context stack. The stack is a `std::vector<Context>` where `Context` holds the focus list and the current focus pointer.

---

## 15. src/backbone/scenario_events.hpp

### Purpose

The typed event bus for scenario lifecycle events. When a scenario spawns, when the user submits answers, when grading completes, when the user transitions to Post-Round, when they click Again — all of these are events fired through this bus. Multiple zones subscribe; the bus dispatches to all subscribers in registration order.

This is the architectural rule's mechanism: "no zone may fire scenario lifecycle events directly to consumers." All scenario events flow through this bus.

### Dependencies

- `src/engine/scenario_id.hpp` (events carry the active ScenarioId)

### Consumers

- Z01 (Engine): fires `ScenarioSpawned` and `GradingComplete` events.
- Z03 (Audio): subscribes to `ScenarioSpawned` for the audio choreography sequence; subscribes to `GradingComplete` for the pass/fail SFX.
- Z08 (Game Screen): subscribes to `ScenarioSpawned` for rendering setup; subscribes to `AnswersSubmitted` for animating the chip resolution.
- Z09 (Math Interrogator): subscribes to `ScenarioSpawned` for input dispatch; fires `AnswersSubmitted` on user submission.
- Z10 (Temporal): subscribes to `ScenarioSpawned` to start the delta timer; subscribes to `AnswersSubmitted` to stop it.
- Z13 (Post-Round): subscribes to `GradingComplete` to render the recap; fires `AgainPressed` on user click.
- Z14 (Tutorial): subscribes to multiple events to advance the tutorial sequence.

### Contents

    #pragma once

    #include "engine/scenario_id.hpp"

    #include <cstdint>
    #include <functional>
    #include <string_view>
    #include <vector>

    namespace poker_trainer::backbone {

    // Scenario lifecycle event types.
    enum class ScenarioEventType : std::uint8_t {
        // A new scenario has been generated and is about to be rendered.
        // Fired by Z01 after generation, before any rendering.
        ScenarioSpawned = 0,

        // The user has submitted their answers for grading. Fired by
        // Z09 when the submission gesture (Enter or click Submit) is
        // received. The grading happens after this event; consumers
        // can use this event to stop timers, animate chip resolution, etc.
        AnswersSubmitted = 1,

        // Grading has completed and the result is available. Fired by
        // Z01 after evaluating the user's answers against the scenario.
        GradingComplete = 2,

        // The user has clicked the Again button to start a new scenario.
        // Fired by Z13 on the commit click (double-confirm).
        AgainPressed = 3,

        // The user has exited the Post-Round Screen to Mode Selection.
        // Fired by Z13 when the Exit button is clicked.
        ExitToModeSelection = 4,
    };

    // Payload for ScenarioSpawned events.
    struct ScenarioSpawnedEvent {
        engine::ScenarioId scenario_id;
    };

    // Payload for AnswersSubmitted events.
    struct AnswersSubmittedEvent {
        engine::ScenarioId scenario_id;
        // The actual answer values are not in this payload; consumers
        // that need them query Z01's scenario state directly.
    };

    // Payload for GradingComplete events.
    struct GradingCompleteEvent {
        engine::ScenarioId scenario_id;
        bool passed;
        std::uint32_t elapsed_ms;
        // Detailed grade breakdown is not in this payload; consumers
        // that need it query Z01's grading result directly.
    };

    // Payload for AgainPressed events.
    struct AgainPressedEvent {
        // The just-completed scenario; a new scenario will be generated
        // in response to this event.
        engine::ScenarioId previous_scenario_id;
    };

    // Payload for ExitToModeSelection events.
    struct ExitToModeSelectionEvent {
        engine::ScenarioId previous_scenario_id;
    };

    // Subscriber callbacks. One callback type per event payload.
    using ScenarioSpawnedHandler =
        std::function<void(const ScenarioSpawnedEvent&)>;
    using AnswersSubmittedHandler =
        std::function<void(const AnswersSubmittedEvent&)>;
    using GradingCompleteHandler =
        std::function<void(const GradingCompleteEvent&)>;
    using AgainPressedHandler =
        std::function<void(const AgainPressedEvent&)>;
    using ExitToModeSelectionHandler =
        std::function<void(const ExitToModeSelectionEvent&)>;

    // Opaque handle for an installed subscriber.
    struct SubscriberHandle {
        std::uint64_t value{0};
        constexpr bool operator==(const SubscriberHandle&) const noexcept = default;
    };

    inline constexpr SubscriberHandle kInvalidSubscriberHandle{0};

    // Subscribe to ScenarioSpawned events. Returns a handle that can
    // be used to unsubscribe. The tag is for debugging.
    SubscriberHandle subscribe_scenario_spawned(
        ScenarioSpawnedHandler handler, std::string_view tag) noexcept;

    SubscriberHandle subscribe_answers_submitted(
        AnswersSubmittedHandler handler, std::string_view tag) noexcept;

    SubscriberHandle subscribe_grading_complete(
        GradingCompleteHandler handler, std::string_view tag) noexcept;

    SubscriberHandle subscribe_again_pressed(
        AgainPressedHandler handler, std::string_view tag) noexcept;

    SubscriberHandle subscribe_exit_to_mode_selection(
        ExitToModeSelectionHandler handler, std::string_view tag) noexcept;

    // Unsubscribe a previously-installed subscriber. The handle is
    // invalidated by this call. Calling unsubscribe on an invalid
    // handle is a no-op.
    void unsubscribe(SubscriberHandle handle) noexcept;

    // Fire events. Called by the zones that produce each event type.
    void fire_scenario_spawned(const ScenarioSpawnedEvent& event) noexcept;
    void fire_answers_submitted(const AnswersSubmittedEvent& event) noexcept;
    void fire_grading_complete(const GradingCompleteEvent& event) noexcept;
    void fire_again_pressed(const AgainPressedEvent& event) noexcept;
    void fire_exit_to_mode_selection(
        const ExitToModeSelectionEvent& event) noexcept;

    // Clear all subscribers. Used by the integration test only.
    void reset_scenario_events_for_testing() noexcept;

    }  // namespace poker_trainer::backbone

### Notes

- The event bus uses typed payloads rather than a single variant or void* payload. This is more verbose but catches mismatches at compile time and makes the API self-documenting.
- The architectural spec lists five scenario-lifecycle events: spawn, submit, grade, again, exit. This header enumerates exactly those five. Adding a sixth event after Phase 0 sign-off requires explicit user approval.
- Event payloads are kept lean. Detailed scenario state (the actual hole cards, the user's answer values, the grading breakdown) is queried from the originating zone, not included in the event. Rationale: events fire frequently and to multiple subscribers; passing large payloads multiplies the cost. The originating zone holds the canonical state.
- Subscribers are invoked synchronously in registration order. There is no event queue, no async dispatch. The fire functions return after all subscribers have run.
- The Phase 0 implementation `src/backbone/scenario_events.cpp` provides subscriber storage and dispatch. One `std::vector<Subscriber>` per event type; subscribers are appended on subscribe, removed on unsubscribe. Dispatch is a linear walk over the vector.

---

## 16. src/backbone/modal_state.hpp

### Purpose

Defines the **interface only** for the modal state observer. Modal infrastructure (Z11) owns the real state — which modals are open, their stacking order, their identifiers. Phase 0 defines the read-only query API that other zones use to ask "is any modal open?" without depending on Z11.

The key consumer is Z10 (Temporal): the Delta Timer and Visual Countdown pause when any modal is open. Z10 must not depend on Z11 directly (that would invert the dependency direction); instead, Z10 reads the modal state through this Phase 0 interface, and Z11 writes to it as it opens and closes modals.

This is an interface-only header. No `.cpp` file is produced in Phase 0. Z11 will produce the implementation in Wave 3.

### Dependencies

None beyond the standard library.

### Consumers

- Z11 (Modal Infrastructure): the implementer. Updates the state on every modal open and close.
- Z10 (Temporal): reads `is_any_modal_open()` to gate the timer pause.
- Z14 (Tutorial): reads `is_any_modal_open()` to detect when a confirmation modal is up during the tutorial.

### Contents

    #pragma once

    #include <cstddef>
    #include <cstdint>

    namespace poker_trainer::backbone {

    // Modal identifier. Each modal type has a unique ID. The full set
    // of modal IDs is defined in Z11; this header treats them as opaque.
    struct ModalId {
        std::uint32_t value{0};
        constexpr bool operator==(const ModalId&) const noexcept = default;
    };

    inline constexpr ModalId kNoModal{0};

    // Returns true if any modal is currently open. Includes any modal
    // type — Settings, Shop, Help, confirmation modals, auth modals,
    // tutorial-related modals.
    [[nodiscard]] bool is_any_modal_open() noexcept;

    // Returns the ID of the topmost modal, or kNoModal if none.
    // The topmost modal is the one most recently opened (or the only
    // one open if there's just one).
    [[nodiscard]] ModalId topmost_modal() noexcept;

    // The current modal stack depth. 0 means no modals; each open
    // modal adds 1.
    [[nodiscard]] std::size_t modal_stack_depth() noexcept;

    // ----- Writer API (Z11 only) -----

    // Push a modal onto the stack. Called by Z11 when a modal opens.
    void notify_modal_opened(ModalId id) noexcept;

    // Pop a modal from the stack. Called by Z11 when a modal closes.
    // The id should match the topmost modal; if it doesn't, the
    // implementation logs a warning and pops anyway (treating the
    // mismatch as a bug to fix elsewhere).
    void notify_modal_closed(ModalId id) noexcept;

    // Reset modal state. Used by the integration test only.
    void reset_modal_state_for_testing() noexcept;

    }  // namespace poker_trainer::backbone

### Notes

- This is **interface only** during Phase 0. The implementation lives in Z11. To make the Phase 0 integration test link cleanly, Phase 0 produces a stub implementation `src/backbone/modal_state_stub.cpp` that maintains a simple counter. The stub is replaced by Z11's real implementation in Wave 3.
- The stub implementation:

      #include "backbone/modal_state.hpp"

      #include <atomic>

      namespace poker_trainer::backbone {

      namespace {
      std::atomic<std::size_t> g_modal_depth{0};
      std::atomic<std::uint32_t> g_topmost_value{0};
      }  // namespace

      bool is_any_modal_open() noexcept {
          return g_modal_depth.load(std::memory_order_acquire) > 0;
      }

      ModalId topmost_modal() noexcept {
          return ModalId{g_topmost_value.load(std::memory_order_acquire)};
      }

      std::size_t modal_stack_depth() noexcept {
          return g_modal_depth.load(std::memory_order_acquire);
      }

      void notify_modal_opened(ModalId id) noexcept {
          g_modal_depth.fetch_add(1, std::memory_order_acq_rel);
          g_topmost_value.store(id.value, std::memory_order_release);
      }

      void notify_modal_closed(ModalId /*id*/) noexcept {
          if (g_modal_depth.load(std::memory_order_acquire) > 0) {
              g_modal_depth.fetch_sub(1, std::memory_order_acq_rel);
          }
          if (g_modal_depth.load(std::memory_order_acquire) == 0) {
              g_topmost_value.store(0, std::memory_order_release);
          }
      }

      void reset_modal_state_for_testing() noexcept {
          g_modal_depth.store(0, std::memory_order_release);
          g_topmost_value.store(0, std::memory_order_release);
      }

      }  // namespace poker_trainer::backbone

- The stub does not track a full stack of modal IDs (only the topmost), because Z10 only needs `is_any_modal_open()`. The full implementation in Z11 will track the complete stack for proper close-mismatch detection.
- ModalIds are 32-bit. Z11 will define the set of modal types in its own header and assign IDs to each. The 32-bit range gives ample headroom.
- The architectural spec specifies that modal stacking pauses the animation clock; the integration here is that Z11's modal-open handler calls both `notify_modal_opened()` (this header) and `animation_clock::pause()` (animation_clock.hpp). The two primitives are independent — Z11 calls both.

---

## 17. Integration test

### File path

`tests/all_headers_test.cpp`

### Purpose

The single gate that proves Phase 0 is correctly assembled. The test includes every Phase 0 header, instantiates one of each declared type, accesses one of each declared constant, and exercises a small representative subset of each backbone primitive's API. Compilation success means the headers are mutually consistent; runtime success means the stubs link and the basic semantics hold.

This is **not** a comprehensive functional test of any subsystem. Comprehensive tests of the engine, audio, persistence, etc. come with their respective zones. This is only an integration check.

### Dependencies

All 16 Phase 0 headers, plus the stub `.cpp` files for `animation_clock` and `modal_state`, plus the real Phase 0 `.cpp` files for `rng_seed`, `scenario_id`, `sync_state`, `screen_state`, `event_router`, `focus_manager`, and `scenario_events`. Plus `<cassert>` (no third-party test framework). Compile-time correctness via `static_assert` where possible (e.g., the FNV-1a hash output); runtime correctness via `assert()` calls inside `static void test_*()` functions invoked sequentially from `main()`.

### Contents

Representative excerpt showing the test's actual shape. See `tests/all_headers_test.cpp` for the full as-built test (381 lines, 90 runtime assertions + 1 `static_assert`, 15 test functions).

    // tests/all_headers_test.cpp
    //
    // Phase 0 integration test: includes every Phase 0 header, links every
    // Phase 0 .cpp, and exercises a representative behavior subset of each
    // backbone primitive plus a sanity check on every contract-bearing
    // constant. Plain C++ assertions (no GoogleTest dependency) keep Phase 0
    // free of extra third-party deps.

    #undef NDEBUG  // assertions stay live regardless of build flags
    #include <cassert>
    #include <cstdio>

    // ... every Phase 0 header included here ...

    namespace pt = poker_trainer;

    static void test_animation_clock() {
        pt::backbone::reset_animation_clock_for_testing();
        assert(pt::backbone::wall_clock_ms() == 0);
        // ... more assertions ...
    }

    // ... one test function per backbone primitive plus static-data sanity tests ...

    static_assert(pt::backbone::make_focusable_id("test").value
                      == 0xf9e6e6ef197c2b25ULL,
                  "FNV-1a hash mismatch");

    int main() {
        test_scenario_id();
        test_rng_seed();
        test_auth0_config();
        test_sync_state();
        test_persistence_schema();
        test_audio_paths();
        test_asset_paths_and_tier_config();
        test_theme_tokens();
        test_settings_defaults();
        test_animation_clock();
        test_modal_state();
        test_screen_state();
        test_event_router();
        test_scenario_events();
        test_focus_manager();
        std::printf("all_headers_test: all assertions passed\n");
        return 0;
    }

### Notes

- The test uses plain `assert()` for runtime checks and `static_assert` for compile-time invariants like the FNV-1a hash. No third-party test framework is required, keeping Phase 0 dependency-free. The pattern is one `static void test_<area>()` function per primitive or static-data group, called sequentially from `main()`; failures abort via `assert` and the failing expression + line are printed by the C runtime.
- The per-primitive reset function (`reset_animation_clock_for_testing()`, `reset_modal_state_for_testing()`, `reset_screen_state_for_testing()`, `reset_scenario_events_for_testing()`, `reset_event_router_for_testing()`, `reset_focus_manager_for_testing()`) is called at the start of each backbone test to ensure a clean state. There is no single combined "reset everything" function; the original combined name `backbone::reset_for_testing()` was renamed per primitive in commit 958e0bc to resolve a link-time duplicate-symbol collision across the 6 backbone .cpp files.
- Each test focuses on the contract surface of one header. The tests are intentionally shallow — proving that the headers compile, link, and behave correctly at the interface level. Deep functional tests come with the zone implementations.
- The test file is at `tests/all_headers_test.cpp`.
- The test compiles via emcc to a WebAssembly artifact (`.js` + `.wasm`) and runs via Node.js: `node /tmp/all_headers_test.js`. Running through emcc rather than the host compiler verifies the entire Phase 0 surface works in the actual target environment (browser-equivalent WebAssembly), not just in a host-system simulation.

---

## 18. Phase 0 completion checklist

Phase 0 is signed off when the following have all been verified:

- [ ] All 16 header files exist at their specified paths.
- [ ] The Phase 0 `.cpp` files (`rng_seed.cpp`, `sync_state.cpp`, `screen_state.cpp`, `event_router.cpp`, `focus_manager.cpp`, `scenario_events.cpp`, `animation_clock_stub.cpp`, `modal_state_stub.cpp`) exist and compile.
- [ ] The integration test `tests/all_headers_test.cpp` exists.
- [ ] A clean wasm build (`emcmake cmake .. && emmake make -j`) succeeds with zero warnings.
- [ ] A clean native test build (`cmake -DENABLE_TESTS=ON .. && make -j`) succeeds with zero warnings.
- [ ] `ctest --output-on-failure` runs the integration test and it passes.
- [ ] CLAUDE.md is updated to reflect Phase 0 sign-off (the relevant section is the contract immutability statement; this is documented but no actual update is required if the section was written with sign-off in mind).
- [ ] A commit lands on `main` with message `complete phase 0` (or similar lowercase imperative), and the commit is pushed to the GitHub remote.

After sign-off, the headers in this document become immutable contracts. Any change to a Phase 0 header thereafter requires explicit user approval, a coordinated update across affected zones, and (if the change is breaking) a scenario format version bump or persistence schema version bump as appropriate.

---

## 19. What happens after sign-off

Once Phase 0 is complete, Wave 1 begins. Wave 1 contains four zones that can be developed in parallel:

- **Z01 (Core Scenario Engine)** — depends on `scenario_id.hpp`, `rng_seed.hpp`, `settings.hpp`.
- **Z02 (Asset Pipeline)** — depends on `asset_paths.hpp`, `tier_config.hpp`.
- **Z04 (Persistence Layer)** — depends on `persistence_schema.hpp`, `auth0_config.hpp`, `sync_state.hpp`.
- **Z06 (Theme System Implementation)** — depends on `theme_tokens.hpp`.

Each Wave 1 zone consumes only Phase 0 headers; none depends on the others. This is what makes parallel development viable.

Once Wave 1 is complete and integration-tested at the wave gate (full build, all zone tests pass, basic cross-zone smoke test), Wave 2 begins, and so on through Wave 5.

The wave-based plan is documented in ZONES.md ("Build Waves") and CLAUDE.md (sections 6 and 8). This document is the inventory for Wave 0 only. Each subsequent wave has its own implementation plan that lives at the start of its execution; those plans are separate documents (not part of PHASE0.md).

---

End of PHASE0.md.
