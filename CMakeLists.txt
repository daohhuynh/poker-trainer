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

    # The integration test source is written at the end of Phase 0. Marking
    # it GENERATED lets configure succeed before the file exists on disk;
    # the build step still fails until the file is written, which matches
    # the Phase 0 sequencing.
    set_source_files_properties(
        tests/phase0_integration/all_headers_test.cpp
        PROPERTIES GENERATED TRUE
    )
    add_executable(phase0_integration_test
        tests/phase0_integration/all_headers_test.cpp
    )
    target_include_directories(phase0_integration_test
        PRIVATE ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_options(phase0_integration_test
        PRIVATE ${POKER_TRAINER_WARNING_FLAGS}
    )
    target_link_libraries(phase0_integration_test
        PRIVATE GTest::gtest GTest::gtest_main
    )

    add_test(NAME phase0_integration_test COMMAND phase0_integration_test)
endif()
