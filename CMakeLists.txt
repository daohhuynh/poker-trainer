# Poker Trainer V1.1 build configuration.
#
# App target: WebAssembly via Emscripten (invoke with `emcmake cmake ..`).
# Tests target: native compile (invoke with plain `cmake -DENABLE_TESTS=ON ..`).
#
# CLAUDE.md section 3 is the authority on build settings.

cmake_minimum_required(VERSION 3.24)

project(poker_trainer
    LANGUAGES CXX
    DESCRIPTION "Poker Trainer V1.1 - browser-based real-time poker math drill"
)

# C++23 strict (CLAUDE.md section 3 "Language standard").
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Release when not specified and no multi-config generator is in use.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

option(ENABLE_TESTS "Build the native unit-test binaries via googletest" OFF)

if(EMSCRIPTEN)
    message(STATUS "poker_trainer: configuring for Emscripten / WebAssembly")
else()
    message(STATUS "poker_trainer: configuring for native host "
                   "(${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")
endif()

# Warning baseline from CLAUDE.md section 3 (warnings-as-errors).
set(POKER_TRAINER_WARNING_FLAGS
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Werror
)

# -------------------------------------------------------------------------
# App target
# -------------------------------------------------------------------------
#
# Placeholder executable: src/main.cpp is a stub until Z05 supplies the real
# entry point and Emscripten link configuration. Phase 0 only needs this
# target to be declared so the rest of the configuration parses cleanly.

add_executable(poker_trainer src/main.cpp)
target_include_directories(poker_trainer PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(poker_trainer PRIVATE ${POKER_TRAINER_WARNING_FLAGS})

# -------------------------------------------------------------------------
# Host tools (native only; never part of the WebAssembly app build)
# -------------------------------------------------------------------------
#
# Zone 02 placeholder generator. Emits a valid stand-in PNG at every path in the
# asset_paths.hpp contract so the asset pipeline can load canonical paths today.
# It iterates kAssetEntries directly (reusing the contract), is self-contained
# (its own PNG writer, no third-party dependency), and runs at development time;
# real art overwrites assets/ with no code change. Held to -Wall -Wextra -Werror
# rather than the full project baseline: it is dev tooling, and -Wconversion would
# bury the bit/font code in casts without catching real defects.
if(NOT EMSCRIPTEN)
    add_executable(gen_placeholders tools/gen_placeholders.cpp)
    target_include_directories(gen_placeholders PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}
    )
    target_compile_options(gen_placeholders PRIVATE -Wall -Wextra -Werror)
endif()

# -------------------------------------------------------------------------
# Tests (native compile only)
# -------------------------------------------------------------------------

if(ENABLE_TESTS)
    if(EMSCRIPTEN)
        message(FATAL_ERROR
            "ENABLE_TESTS=ON is incompatible with the Emscripten toolchain. "
            "Configure tests with a native compiler instead (plain cmake).")
    endif()

    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    # Required on Windows; harmless elsewhere.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    # The Phase 0 sign-off gate is a standalone plain-assert program (its own
    # main(), no GoogleTest) that links directly against every Phase 0 .cpp.
    # See the header comment in tests/all_headers_test.cpp. It therefore does
    # not link the googletest libraries fetched above; those serve the
    # unit-tested zones (Z01, Z10) that come online in later waves.
    add_executable(phase0_integration_test
        tests/all_headers_test.cpp
        src/backbone/event_router.cpp
        src/backbone/scenario_events.cpp
        src/backbone/screen_state.cpp
        src/backbone/focus_manager.cpp
        src/backbone/modal_state_stub.cpp
        src/backbone/animation_clock_stub.cpp
        src/engine/scenario_id.cpp
        src/engine/rng_seed.cpp
        src/persistence/sync_state.cpp
    )
    target_include_directories(phase0_integration_test
        PRIVATE ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_options(phase0_integration_test
        PRIVATE ${POKER_TRAINER_WARNING_FLAGS}
    )

    add_test(NAME phase0_integration_test COMMAND phase0_integration_test)

    # -------------------------------------------------------------------------
    # Zone 06 (Theme System) — native unit tests.
    # -------------------------------------------------------------------------
    #
    # Dear ImGui core, vendored as a source drop under third_party/imgui (the
    # approved mechanism per CLAUDE.md section 3). Built as a static library for
    # the test target only; the WebAssembly app target wires ImGui plus a
    # renderer backend in a later wave (Z05/Bridge), so the app build is left
    # untouched here. Third-party code is not held to the project warning
    # baseline, and its headers are exposed SYSTEM so including imgui.h never
    # trips -Werror inside Z06 translation units.
    add_library(imgui STATIC
        third_party/imgui/imgui.cpp
        third_party/imgui/imgui_draw.cpp
        third_party/imgui/imgui_tables.cpp
        third_party/imgui/imgui_widgets.cpp
    )
    target_include_directories(imgui SYSTEM PUBLIC
        ${CMAKE_SOURCE_DIR}/third_party/imgui
    )

    # Zone 06 theme system. Project code: held to the full warning baseline so
    # the zone is proven to compile clean under -Werror.
    add_library(theme STATIC
        src/theme/theme.cpp
        src/theme/palette_no_limit.cpp
        src/theme/palette_slate.cpp
        src/theme/palette_ocean.cpp
        src/theme/palette_sage.cpp
        src/theme/imgui_style_refresh.cpp
    )
    target_include_directories(theme PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_compile_options(theme PRIVATE ${POKER_TRAINER_WARNING_FLAGS})
    target_link_libraries(theme PUBLIC imgui)

    # Zone 06 unit tests (googletest). Test glue is not held to -Werror so that
    # googletest macro expansions do not fight the strict baseline; the zone
    # code under test is held to it via the `theme` library above.
    add_executable(theme_test
        tests/theme/theme_test.cpp
        tests/theme/imgui_style_refresh_test.cpp
    )
    target_include_directories(theme_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests/theme
    )
    target_link_libraries(theme_test PRIVATE theme GTest::gtest_main)

    add_test(NAME theme_test COMMAND theme_test)

    # -------------------------------------------------------------------------
    # Zone 02 (Asset Pipeline) — native unit tests.
    # -------------------------------------------------------------------------
    #
    # Project code: held to the full warning baseline so the zone is proven to
    # compile clean under -Werror. stb_image is the PNG decoder, exposed SYSTEM so
    # its single-TU implementation in loader.cpp never trips -Werror (the same
    # mechanism used for ImGui above).
    add_library(assets STATIC
        src/assets/loader.cpp
        src/assets/registry.cpp
        src/assets/tier_loader.cpp
    )
    target_include_directories(assets PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_include_directories(assets SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party/stb)
    target_compile_options(assets PRIVATE ${POKER_TRAINER_WARNING_FLAGS})

    # Zone 02 unit tests (googletest). Test glue is not held to -Werror so the
    # macro expansions do not fight the strict baseline; the zone code under test
    # is held to it via the `assets` library above. POKER_TRAINER_SOURCE_DIR lets
    # the loader/integration tests locate the committed placeholder PNGs.
    add_executable(assets_test
        tests/assets/loader_test.cpp
        tests/assets/registry_test.cpp
        tests/assets/tier_loader_test.cpp
    )
    target_include_directories(assets_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests/assets
    )
    target_compile_definitions(assets_test PRIVATE
        POKER_TRAINER_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    target_link_libraries(assets_test PRIVATE assets GTest::gtest_main)

    add_test(NAME assets_test COMMAND assets_test)

    # Zone 04 (Persistence Layer) — native unit tests.
    # -------------------------------------------------------------------------
    #
    # Project code held to the full warning baseline so the zone is proven to
    # compile clean under -Werror. Like Z06, this is built for the test target
    # only; the WebAssembly app target wires persistence in a later wave
    # (Z05/Bridge), so the app build (the poker_trainer target above) is left
    # untouched here. sync_state.cpp is the Phase 0 primitive Z04 writes; it is
    # compiled into this library (its own object, distinct from the copy linked
    # into phase0_integration_test).
    add_library(persistence STATIC
        src/persistence/sync_state.cpp
        src/persistence/idbfs.cpp
        src/persistence/sync.cpp
        src/persistence/migration.cpp
        src/persistence/auth.cpp
        src/persistence/persistence_service.cpp
    )
    target_include_directories(persistence PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_compile_options(persistence PRIVATE ${POKER_TRAINER_WARNING_FLAGS})

    # Zone 04 unit tests (googletest). Test glue is not held to -Werror so that
    # googletest macro expansions do not fight the strict baseline; the zone
    # code under test is held to it via the `persistence` library above.
    add_executable(persistence_test
        tests/persistence/serialization_test.cpp
        tests/persistence/idbfs_test.cpp
        tests/persistence/sync_test.cpp
        tests/persistence/migration_test.cpp
        tests/persistence/auth_test.cpp
        tests/persistence/credentials_test.cpp
    )
    target_include_directories(persistence_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests/persistence
    )
    target_link_libraries(persistence_test PRIVATE persistence GTest::gtest_main)

    add_test(NAME persistence_test COMMAND persistence_test)

    # -------------------------------------------------------------------------
    # Zone 01 (Core Scenario Engine) — native unit tests.
    # -------------------------------------------------------------------------
    #
    # Project code held to the full warning baseline so the zone is proven to
    # compile clean under -Werror. Built for the test target only; the
    # WebAssembly app wires the engine in a later wave (Z05/Bridge), so the app
    # target above is left untouched. Depends only on Phase 0 headers
    # (scenario_id / rng_seed / settings, the last pulling in the header-only
    # theme_tokens) via the src include path. scenario_id.cpp and rng_seed.cpp
    # are compiled here as their own objects, distinct from the copies linked
    # into phase0_integration_test.
    add_library(engine STATIC
        src/engine/scenario_id.cpp
        src/engine/rng_seed.cpp
        src/engine/deck.cpp
        src/engine/fold_function.cpp
        src/engine/hand_eval.cpp
        src/engine/side_pot.cpp
        src/engine/evaluator.cpp
        src/engine/generator.cpp
    )
    target_include_directories(engine PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_compile_options(engine PRIVATE ${POKER_TRAINER_WARNING_FLAGS})

    # Zone 01 unit tests (googletest). Test glue is not held to -Werror so that
    # googletest macro expansions do not fight the strict baseline; the zone code
    # under test is held to it via the `engine` library above.
    add_executable(engine_test
        tests/engine/deck_test.cpp
        tests/engine/fold_function_test.cpp
        tests/engine/hand_eval_test.cpp
        tests/engine/evaluator_test.cpp
        tests/engine/side_pot_test.cpp
        tests/engine/generator_test.cpp
    )
    target_include_directories(engine_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests/engine
    )
    target_link_libraries(engine_test PRIVATE engine GTest::gtest_main)

    add_test(NAME engine_test COMMAND engine_test)
endif()
