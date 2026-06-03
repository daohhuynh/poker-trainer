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

# Reduced baseline for low-level binding code (Emscripten/WebGL/DOM plumbing and
# the dev-tool placeholder generator). Still -Werror (0 warnings), but without
# -Wconversion / -Wold-style-cast / -Wcast-align / -Wshadow, which would bury the
# binding code in casts without catching real defects (see gen_placeholders and
# the Z05 bridge_platform library below).
set(POKER_TRAINER_BINDING_WARNING_FLAGS
    -Wall
    -Wextra
    -Werror
)

# =========================================================================
# Shared libraries
# =========================================================================
#
# Built whenever the WebAssembly app (EMSCRIPTEN) or the native test binaries
# (ENABLE_TESTS) are configured. The app links these into the wasm bundle; the
# test executables link the same libraries so a zone is proven to build clean
# under the project warning baseline exactly once.
if(EMSCRIPTEN OR ENABLE_TESTS)
    # ---------------------------------------------------------------------
    # Dear ImGui core (vendored source drop; the approved mechanism per
    # CLAUDE.md section 3). Third-party code is not held to the project warning
    # baseline; its headers are exposed SYSTEM so including imgui.h never trips
    # -Werror inside project translation units. The Z05 bridge supplies its own
    # WebGL2 renderer (src/bridge/gl_renderer.cpp), so no third-party ImGui
    # backend is vendored.
    add_library(imgui STATIC
        third_party/imgui/imgui.cpp
        third_party/imgui/imgui_draw.cpp
        third_party/imgui/imgui_tables.cpp
        third_party/imgui/imgui_widgets.cpp
    )
    target_include_directories(imgui SYSTEM PUBLIC
        ${CMAKE_SOURCE_DIR}/third_party/imgui
    )

    # ---------------------------------------------------------------------
    # Backbone primitives (Phase 0 contracts; implementations owned by Z05,
    # except modal_state which is Zone 11's).
    #
    # The real Z05 animation clock (animation_clock.cpp) is compiled here and is
    # what the app and the Z05 tests link. The no-op animation_clock_stub.cpp is
    # deliberately NOT in this library: it stays bound solely to
    # phase0_integration_test, whose sign-off gate asserts the stub's behavior.
    # Zone 11 will append its modal_state.cpp here later; until then
    # modal_state_stub.cpp supplies the modal-state symbols.
    add_library(backbone STATIC
        src/backbone/animation_clock.cpp
        src/backbone/event_router.cpp
        src/backbone/screen_state.cpp
        src/backbone/scenario_events.cpp
        src/backbone/focus_manager.cpp
        src/backbone/modal_state_stub.cpp  # SEAM(Z11): swapped for modal_state.cpp later
    )
    target_include_directories(backbone PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_compile_options(backbone PRIVATE ${POKER_TRAINER_WARNING_FLAGS})

    # ---------------------------------------------------------------------
    # Zone 01 (Core Scenario Engine). scenario_id.cpp and rng_seed.cpp are
    # compiled here as their own objects, distinct from the copies linked into
    # phase0_integration_test.
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

    # ---------------------------------------------------------------------
    # Zone 02 (Asset Pipeline). stb_image is the PNG decoder, exposed SYSTEM so
    # its single-TU implementation in loader.cpp never trips -Werror.
    add_library(assets STATIC
        src/assets/loader.cpp
        src/assets/registry.cpp
        src/assets/tier_loader.cpp
    )
    target_include_directories(assets PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_include_directories(assets SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party/stb)
    target_compile_options(assets PRIVATE ${POKER_TRAINER_WARNING_FLAGS})

    # ---------------------------------------------------------------------
    # Zone 04 (Persistence Layer). sync_state.cpp is the Phase 0 primitive Z04
    # writes; it is compiled here as its own object, distinct from the copy
    # linked into phase0_integration_test.
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

    # ---------------------------------------------------------------------
    # Zone 06 (Theme System). Links ImGui (it writes the active theme into the
    # ImGui style on switch).
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

    # ---------------------------------------------------------------------
    # Zone 05 (Bridge) — pure-logic library (native-testable, no ImGui /
    # Emscripten). Every bridge translation unit guards its ImGui / Emscripten
    # code behind `#ifdef __EMSCRIPTEN__`, so under the native test compiler
    # these objects contain only the pure logic: the screen-dispatch registry,
    # the mode->id reject loop, canvas sizing / min-size / mobile-UA decisions,
    # shared-scenario URL routing, and the loading-arc fraction.
    add_library(bridge STATIC
        src/bridge/screen_dispatch.cpp
        src/bridge/game_launch.cpp
        src/bridge/canvas_sizing.cpp
        src/bridge/shared_scenario.cpp
        src/bridge/loading_screen.cpp
        src/bridge/settings_persistence.cpp
        src/bridge/persistent_weights_store.cpp
    )
    target_include_directories(bridge PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_compile_options(bridge PRIVATE ${POKER_TRAINER_WARNING_FLAGS})
    # persistence: the persistent Custom-weights store wraps Zone 04's IdbfsStore.
    target_link_libraries(bridge PUBLIC backbone engine persistence)

    # ---------------------------------------------------------------------
    # Zone 07 (Root + Mode Selection Screens). Built here in the shared section
    # (not just for tests) so Z05 can link the real screens into the wasm app via
    # its render-dispatch seam. Held to the full warning baseline (proven clean
    # under -Werror, including the ImGui render TUs). The render / registration
    # TUs reference bridge symbols (request_game_launch, register_screen_renderer)
    # resolved at final link; declaring bridge as a link dependency keeps the
    # static-lib link order correct. bridge does NOT depend on screens, so this is
    # a one-way edge (no cycle).
    add_library(screens STATIC
        src/animations/button_morph.cpp
        src/screens/root_screen.cpp
        src/screens/mode_selection_screen.cpp
        src/screens/custom_popup.cpp
        src/screens/screen_registration.cpp
    )
    target_include_directories(screens PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_compile_options(screens PRIVATE ${POKER_TRAINER_WARNING_FLAGS})
    target_link_libraries(screens PUBLIC backbone theme bridge)  # theme brings imgui

    # ---------------------------------------------------------------------
    # Zone 09 (Math Interrogator). Built here in the shared section so Z05 / Z08
    # can link the math inputs into the wasm app, and so the native tests exercise
    # the same -Werror-clean objects. Held to the full warning baseline (including
    # the ImGui render TUs). The submission TU calls Z01's locked evaluator (Z09
    # owns no truth/margin math); the render + keybind TUs reference theme tokens,
    # the backbone (focus / router / scenario bus), and bridge
    # (register_screen_renderer), resolved at final link. backbone / engine / theme
    # / bridge do NOT depend on interrogator, so this is a one-way edge (no cycle).
    add_library(interrogator STATIC
        src/math/input_boxes.cpp
        src/math/bet_size_buttons.cpp
        src/math/submission.cpp
        src/math/keybinds.cpp
    )
    target_include_directories(interrogator PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_compile_options(interrogator PRIVATE ${POKER_TRAINER_WARNING_FLAGS})
    target_link_libraries(interrogator PUBLIC backbone engine theme bridge)  # theme brings imgui
endif()

# =========================================================================
# WebAssembly app target
# =========================================================================
#
# src/main.cpp guards the boot call behind __EMSCRIPTEN__, so in a native build
# this target links as an empty success (the app is exercised through the test
# binaries). In the Emscripten build it links the bridge platform layer and the
# zone libraries into the wasm bundle.
add_executable(poker_trainer src/main.cpp)
target_include_directories(poker_trainer PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(poker_trainer PRIVATE ${POKER_TRAINER_WARNING_FLAGS})

if(EMSCRIPTEN)
    # Zone 05 render / platform / boot layer. Binding code (WebGL2 renderer, DOM
    # input, canvas binding, CDN fetch, ImGui-render bodies, main loop, boot):
    # held to the reduced binding baseline, compiled only into the wasm app.
    add_library(bridge_platform STATIC
        src/bridge/platform.cpp
        src/bridge/gl_renderer.cpp
        src/bridge/cdn_fetch.cpp
        src/bridge/render_screens.cpp
        src/bridge/error_screen.cpp
        src/bridge/idbfs_backend.cpp
        src/bridge/main_loop.cpp
        src/bridge/boot.cpp
    )
    target_include_directories(bridge_platform PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_compile_options(bridge_platform PRIVATE ${POKER_TRAINER_BINDING_WARNING_FLAGS})
    # screens: boot installs Zone 07's renders into the dispatch registry.
    # interrogator: Z09's math inputs; boot adds one install_interrogator(runtime)
    # call at pass-2 integration (boot.cpp is not edited by the Z09 wave).
    target_link_libraries(bridge_platform PUBLIC
        bridge screens interrogator backbone engine assets persistence theme imgui)

    target_link_libraries(poker_trainer PRIVATE bridge_platform)
    target_link_options(poker_trainer PRIVATE
        "-sMIN_WEBGL_VERSION=2"
        "-sMAX_WEBGL_VERSION=2"
        "-sFULL_ES3=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sEXIT_RUNTIME=0"
        "-sASSERTIONS=1"
        "-lidbfs.js"  # IDBFS filesystem backend for durable guest-mode persistence
    )

    # Serve the service worker from the bundle directory so the boot-time
    # navigator.serviceWorker.register('service_worker.js') resolves (a worker
    # only controls pages at or below its own scope).
    add_custom_command(TARGET poker_trainer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/src/bridge/service_worker.js
            $<TARGET_FILE_DIR:poker_trainer>/service_worker.js
        COMMENT "Copying service_worker.js next to the wasm bundle"
    )
endif()

# -------------------------------------------------------------------------
# Host tools (native only; never part of the WebAssembly app build)
# -------------------------------------------------------------------------
#
# Zone 02 placeholder generator. Emits a valid stand-in PNG at every path in the
# asset_paths.hpp contract so the asset pipeline can load canonical paths today.
# It iterates kAssetEntries directly (reusing the contract), is self-contained
# (its own PNG writer, no third-party dependency), and runs at development time;
# real art overwrites assets/ with no code change. Held to the reduced binding
# baseline: it is dev tooling, and -Wconversion would bury the bit/font code in
# casts without catching real defects.
if(NOT EMSCRIPTEN)
    add_executable(gen_placeholders tools/gen_placeholders.cpp)
    target_include_directories(gen_placeholders PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}
    )
    target_compile_options(gen_placeholders PRIVATE ${POKER_TRAINER_BINDING_WARNING_FLAGS})
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
    # See the header comment in tests/all_headers_test.cpp. It links its OWN copy
    # of the backbone primitives — including animation_clock_stub.cpp, whose
    # no-op behavior the gate asserts — so this source list is left untouched by
    # the Z05 wave (the real animation_clock.cpp lives in the `backbone` library
    # above).
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

    # Zone 02 unit tests. POKER_TRAINER_SOURCE_DIR lets the loader/integration
    # tests locate the committed placeholder PNGs.
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

    # Zone 04 unit tests.
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

    # Zone 01 unit tests.
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

    # Backbone primitive unit tests (Z05). Test glue is not held to -Werror; the
    # primitives under test are held to it via the `backbone` library above.
    add_executable(backbone_test
        tests/backbone/animation_clock_test.cpp
        tests/backbone/event_router_test.cpp
        tests/backbone/focus_manager_test.cpp
        tests/backbone/screen_state_test.cpp
        tests/backbone/scenario_events_test.cpp
    )
    target_include_directories(backbone_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests/backbone
    )
    target_link_libraries(backbone_test PRIVATE backbone GTest::gtest_main)

    add_test(NAME backbone_test COMMAND backbone_test)

    # Zone 05 bridge pure-logic unit tests. Test glue is not held to -Werror; the
    # zone code under test is held to it via the `bridge` library above.
    add_executable(bridge_test
        tests/bridge/canvas_sizing_test.cpp
        tests/bridge/loading_arc_test.cpp
        tests/bridge/shared_scenario_test.cpp
        tests/bridge/game_launch_test.cpp
        tests/bridge/screen_dispatch_test.cpp
    )
    target_include_directories(bridge_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests/bridge
    )
    target_link_libraries(bridge_test PRIVATE bridge GTest::gtest_main)

    add_test(NAME bridge_test COMMAND bridge_test)

    # -------------------------------------------------------------------------
    # Zone 07 (Root + Mode Selection Screens) — native unit tests.
    # -------------------------------------------------------------------------
    #
    # The `screens` library is defined in the shared section above (it is now built
    # for the wasm app too, since Z05 links the real screens in). Its code is held
    # to the full -Werror baseline there. These tests reference only the pure logic
    # (morph math + controller, coupled solver, keystroke rules, action semantics)
    # plus the focus-list registration — never the render/launch TUs.
    add_executable(screens_test
        tests/animations/button_morph_test.cpp
        tests/screens/custom_popup_test.cpp
        tests/screens/focus_lists_test.cpp
    )
    target_include_directories(screens_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(screens_test PRIVATE screens GTest::gtest_main)

    add_test(NAME screens_test COMMAND screens_test)

    # -------------------------------------------------------------------------
    # Pass-1 integration wiring tests (Z05 <-> Z07 <-> Z04 finalization).
    # -------------------------------------------------------------------------
    #
    # Cross-zone wiring that no single zone's test owns: the Zone 07 render
    # registration into the Z05 dispatch registry, the interim settings-blob codec,
    # the theme-at-boot read, and the persistence-backed Custom-weights round-trip.
    # Links screens + bridge + persistence + theme (all already held to -Werror via
    # their libraries); the test glue itself is not held to -Werror.
    add_executable(wiring_test
        tests/screens/screen_registration_test.cpp
        tests/bridge/settings_persistence_test.cpp
    )
    target_include_directories(wiring_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/tests/persistence
    )
    target_link_libraries(wiring_test PRIVATE
        screens bridge persistence theme GTest::gtest_main)

    add_test(NAME wiring_test COMMAND wiring_test)

    # -------------------------------------------------------------------------
    # Zone 09 (Math Interrogator) — native unit tests.
    # -------------------------------------------------------------------------
    #
    # The `interrogator` library (shared section above) is held to -Werror,
    # including its ImGui render TUs. These tests reference only the pure logic
    # (numeric filter, per-branch spawning, focus segment, parsing, gather/grade,
    # pass/fail) plus the installed event-router handlers driven through dispatch;
    # they never invoke the render TUs.
    add_executable(interrogator_test
        tests/math/input_boxes_test.cpp
        tests/math/submission_test.cpp
        tests/math/keybinds_test.cpp
    )
    target_include_directories(interrogator_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(interrogator_test PRIVATE interrogator GTest::gtest_main)

    add_test(NAME interrogator_test COMMAND interrogator_test)
endif()
