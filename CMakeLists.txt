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
        src/backbone/modal_state.cpp  # Zone 11's real modal-stack implementation
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
        src/persistence/auth0_backend.cpp
        src/persistence/supabase_backend.cpp
        src/persistence/economy.cpp
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
        src/bridge/input_routing.cpp
        src/bridge/focus_registry.cpp
        src/bridge/focus_reconcile.cpp
        src/bridge/frame_tick.cpp
        src/bridge/canvas_sizing.cpp
        src/bridge/shared_scenario.cpp
        src/bridge/loading_screen.cpp
        src/bridge/tier_schedule.cpp
        src/bridge/settings_persistence.cpp
        src/bridge/persistent_weights_store.cpp
    )
    target_include_directories(bridge PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_compile_options(bridge PRIVATE ${POKER_TRAINER_WARNING_FLAGS})
    # persistence: the persistent Custom-weights store wraps Zone 04's IdbfsStore.
    target_link_libraries(bridge PUBLIC backbone engine persistence)
    if(EMSCRIPTEN)
        # focus_reconcile.cpp (the reconcile + ring render glue) includes <imgui.h>
        # only under __EMSCRIPTEN__; link imgui for its headers + objects in the wasm
        # build. Natively that ImGui code is compiled out, so the bridge library
        # stays ImGui-free under the native test compiler (every bridge TU still
        # guards ImGui behind __EMSCRIPTEN__).
        target_link_libraries(bridge PRIVATE imgui)
    else()
        # Native test build: compile the no-op (#else) branch of the art-texture
        # seam into the pure bridge library so the screens / game test binaries
        # resolve bridge::asset_gl_texture. The branch is ImGui/GL-free (returns 0,
        # no GL context under the test compiler), so bridge stays ImGui-free. The
        # real WebGL2 upload (the #ifdef __EMSCRIPTEN__ branch) is compiled into
        # bridge_platform for the wasm app instead (mirrors the audio_backend split).
        target_sources(bridge PRIVATE src/bridge/texture_bind.cpp)
    endif()

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
    target_link_libraries(screens PUBLIC backbone theme bridge modal)  # theme brings imgui

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
        src/math/tier_flow.cpp
    )
    target_include_directories(interrogator PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_compile_options(interrogator PRIVATE ${POKER_TRAINER_WARNING_FLAGS})
    target_link_libraries(interrogator PUBLIC backbone engine theme bridge temporal)  # theme brings imgui

    # ---------------------------------------------------------------------
    # Zone 03 (Audio Engine). The pure logic + integration (shuffle pool,
    # choreography scheduler, music transport state machine, SFX dispatch, the
    # engine state with the volume / mute / gesture-gate math) is held to the full
    # warning baseline and is native-testable; the actual audio backend (miniaudio
    # SFX device + HTML5 <audio> music) is split out below behind __EMSCRIPTEN__,
    # mirroring the bridge / bridge_platform pure-vs-binding split. backbone / engine
    # / bridge do NOT depend on audio, so this is a one-way edge (no cycle). audio
    # reads bridge::active_scenario() for the spawn choreography — the documented
    # consumer path (the scenario_spawned bus event carries only the id).
    add_library(audio STATIC
        src/audio/shuffle_pool.cpp
        src/audio/choreography.cpp
        src/audio/music.cpp
        src/audio/sfx.cpp
        src/audio/audio.cpp
    )
    target_include_directories(audio PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_compile_options(audio PRIVATE ${POKER_TRAINER_WARNING_FLAGS})
    target_link_libraries(audio PUBLIC backbone engine bridge)
    if(EMSCRIPTEN)
        # Real backend (miniaudio SFX device + HTML5 <audio> music via EM_ASM): held
        # to the reduced binding baseline. miniaudio.h and stb_vorbis.c are SYSTEM
        # includes, so their single-TU implementations never trip -Werror. stb_vorbis
        # supplies miniaudio's bundled Vorbis decoding backend so the .ogg SFX decode.
        add_library(audio_backend STATIC src/audio/audio_backend.cpp)
        target_include_directories(audio_backend PUBLIC ${CMAKE_SOURCE_DIR}/src)
        target_include_directories(audio_backend SYSTEM PRIVATE
            ${CMAKE_SOURCE_DIR}/third_party/miniaudio
            ${CMAKE_SOURCE_DIR}/third_party/stb)
        target_compile_options(audio_backend PRIVATE ${POKER_TRAINER_BINDING_WARNING_FLAGS})
        target_link_libraries(audio PUBLIC audio_backend)
    else()
        # Native test build: compile the same TU's no-op (#else) branch into `audio`
        # so the pure library links without any audio backend.
        target_sources(audio PRIVATE src/audio/audio_backend.cpp)
    endif()

    # ---------------------------------------------------------------------
    # Zone 08 (Game Screen Rendering). Built here in the shared section so Z05 can
    # link the Game render into the wasm app and the native tests exercise the same
    # -Werror-clean objects. Held to the full warning baseline (including the ImGui
    # render TUs). The render TUs reference theme tokens + ImGui; game_screen.cpp
    # references the backbone (scenario bus / screen state / clock / modal state),
    # bridge (register_screen_renderer, active_scenario; load_frog_bundle only under
    # __EMSCRIPTEN__), interrogator (render_math_inputs — Z08 composes Z09 by calling
    # it), and audio (the Frog toggle SFX). None of those depend on game, so this is
    # a one-way edge (no cycle).
    add_library(game STATIC
        src/render/table.cpp
        src/render/dealer.cpp
        src/render/chips.cpp
        src/render/cards.cpp
        src/render/opponents.cpp
        src/render/hud.cpp
        src/animations/chip_slide.cpp
        src/easter_egg/frog_toggle.cpp
        src/screens/game_screen.cpp
    )
    target_include_directories(game PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_compile_options(game PRIVATE ${POKER_TRAINER_WARNING_FLAGS})
    target_link_libraries(game PUBLIC
        backbone engine theme bridge interrogator audio modal temporal)  # theme brings imgui

    # ---------------------------------------------------------------------
    # Zone 13 (Post-Round Screen). Built here in the shared section so Z05 can link
    # the recap render into the wasm app and the native tests exercise the same
    # objects. The render / logic TUs are held to the full warning baseline; the
    # clipboard_fallback_modal TU carries the Copy/Share browser interop (EM_JS) and
    # is held to the reduced binding baseline (no -Wpedantic), mirroring the
    # bridge / bridge_platform and audio / audio_backend pure-vs-binding split.
    # Warning flags are set per source (not at target level) so the binding TU gets
    # only the reduced set. game: Z13 reads Z08's easter_egg_frog_active() query;
    # interrogator: Z13 reads Z09's GradingResult handoff (InterrogatorState::
    # last_result). None of those depend on post_round, so this is a one-way edge.
    add_library(post_round STATIC
        src/screens/again_button.cpp
        src/render/front_dealer.cpp
        src/render/stat_modal.cpp
        src/screens/post_round_screen.cpp
        src/screens/clipboard_fallback_modal.cpp
    )
    target_include_directories(post_round PUBLIC ${CMAKE_SOURCE_DIR}/src)
    set_source_files_properties(
        src/screens/again_button.cpp
        src/render/front_dealer.cpp
        src/render/stat_modal.cpp
        src/screens/post_round_screen.cpp
        PROPERTIES COMPILE_OPTIONS "${POKER_TRAINER_WARNING_FLAGS}")
    set_source_files_properties(
        src/screens/clipboard_fallback_modal.cpp
        PROPERTIES COMPILE_OPTIONS "${POKER_TRAINER_BINDING_WARNING_FLAGS}")
    target_link_libraries(post_round PUBLIC
        backbone engine theme bridge interrogator game modal temporal)  # theme brings imgui

    # ---------------------------------------------------------------------
    # Zone 11 (Modal Infrastructure + Persistent Cluster). Built here in the shared
    # section so Z05 links the modal + outage-banner overlay and the persistent
    # cluster into the wasm app, and the native tests exercise the same -Werror-clean
    # objects (ImGui render TUs included). The render TUs reference theme tokens +
    # ImGui; the cluster/offline TUs reach bridge (asset_image, focus_registry,
    # screen_dispatch) + persistence (sync_state) + the backbone modal stack. modal
    # does NOT depend on screens / game / post_round (the cluster geometry + ids are
    # passed in via ClusterContext), so those zones depend on modal one-way (no cycle).
    add_library(modal STATIC
        src/modal/modal_base.cpp
        src/modal/cluster.cpp
        src/modal/offline_indicator.cpp
        src/modal/confirm_modal.cpp
        src/modal/help_modal.cpp
        src/modal/outage_banner.cpp
        src/modal/leaderboard_view.cpp
        src/modal/shop_view.cpp
        src/modal/auth_modals.cpp
    )
    target_include_directories(modal PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_compile_options(modal PRIVATE ${POKER_TRAINER_WARNING_FLAGS})
    target_link_libraries(modal PUBLIC backbone theme bridge persistence)  # theme brings imgui

    # ---------------------------------------------------------------------
    # Zone 10 (Temporal Layer). Built here in the shared section so Z05 links the
    # Delta Timer + Visual Countdown into the wasm app and the native tests exercise
    # the same -Werror-clean objects. The pure cores (target-time table, the advance
    # state machine, the display format) are unit-tested; visual_countdown.cpp's
    # ImGui draw is browser-verified only. delta_timer.cpp reads the backbone (clock /
    # modal poll / scenario bus / screen state) and bridge (active_scenario for the
    # spawn handler — the documented path, mirroring Z03 audio; register_frame_tick
    # for the per-frame update). None of those depend on temporal, so this is a
    # one-way edge (no cycle).
    add_library(temporal STATIC
        src/temporal/target_time.cpp
        src/temporal/delta_timer.cpp
        src/temporal/visual_countdown.cpp
    )
    target_include_directories(temporal PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_compile_options(temporal PRIVATE ${POKER_TRAINER_WARNING_FLAGS})
    target_link_libraries(temporal PUBLIC backbone engine theme bridge)  # theme brings imgui

    # ---------------------------------------------------------------------
    # Zone 12 (Settings Page). The Settings modal CONTENT rendered into Zone 11's
    # cluster-modal shell via Z11's generic content-provider seam: the section sidebar,
    # the scrollable body, the fuzzy search, the nine per-section renders, and the
    # coupled/clamped widgets. The pure cores (street redistribution, difficulty clamp,
    # int parse/step, search match, validate) are unit-tested; the section render TUs
    # are browser-verified (CLAUDE.md §9). Links modal (Z11 host shell + confirm +
    # asset ids), theme (Z06 tokens + set_theme), bridge (the focus-reconcile
    # substrate), backbone (focus / router). Zone 04 persistence and Zone 03 audio are
    # reached through boot-injected callbacks rather than a direct link, keeping this
    # library's dependency edges minimal. modal / theme / bridge / backbone do NOT
    # depend on settings, so this is a one-way edge (no cycle).
    add_library(settings STATIC
        src/settings/search.cpp
        src/settings/settings_modal.cpp
        src/settings/username_denylist.cpp
        src/settings/auth_form.cpp
        src/settings/account_modal.cpp
    )
    target_include_directories(settings PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_compile_options(settings PRIVATE ${POKER_TRAINER_WARNING_FLAGS})
    target_link_libraries(settings PUBLIC backbone theme bridge modal)  # theme brings imgui
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
        src/bridge/texture_bind.cpp
        src/bridge/cdn_fetch.cpp
        src/bridge/sfx_prefetch.cpp
        src/bridge/tier_orchestrator.cpp
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
    # audio: Z03; boot calls install_audio(), the main loop calls audio_update(),
    # and platform.cpp calls on_first_user_gesture() (audio_backend comes via audio).
    # game: Z08's Game-screen render; boot adds one install_game_screen(...) call
    # after install_interrogator so Z08's renderer wins the single Game slot.
    # post_round: Z13's Post-Round render; boot adds one install_post_round_screen
    # call. It registers ScreenId::PostRound's renderer slot (no conflict with Z08's
    # Game slot) and the GradingComplete capture that drives the transition.
    target_link_libraries(bridge_platform PUBLIC
        bridge screens interrogator game post_round audio modal settings temporal backbone engine assets persistence theme imgui)

    target_link_libraries(poker_trainer PRIVATE bridge_platform)
    target_link_options(poker_trainer PRIVATE
        "-sMIN_WEBGL_VERSION=2"
        "-sMAX_WEBGL_VERSION=2"
        "-sFULL_ES3=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sEXIT_RUNTIME=0"
        "-sASSERTIONS=1"
        # Export the wasm heap views onto Module. miniaudio's Web Audio backend
        # reads Module.HEAPF32 from its ScriptProcessorNode onaudioprocess callback
        # (third_party/miniaudio/miniaudio.h: `new Float32Array(Module.HEAPF32.buffer, ...)`);
        # without the export the first audio callback after the device starts on the
        # first user gesture hits abort('HEAPF32' was not exported), which kills the
        # runtime and freezes the app. sfx_prefetch.cpp's memfs writeback also reaches
        # for HEAPU8. Export the full standard heap-view set so the audio callback (and
        # any sibling heap access) reaches the heap without aborting; over-exporting only
        # enlarges the JS glue slightly. ALLOW_MEMORY_GROWTH keeps these views valid
        # across heap resizes (Emscripten reassigns them on growth).
        "-sEXPORTED_RUNTIME_METHODS=HEAPF32,HEAPF64,HEAP8,HEAPU8,HEAP16,HEAPU16,HEAP32,HEAPU32"
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
        tests/persistence/auth0_backend_test.cpp
        tests/persistence/supabase_backend_test.cpp
        tests/persistence/economy_test.cpp
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
        tests/bridge/tier_schedule_test.cpp
        tests/bridge/shared_scenario_test.cpp
        tests/bridge/game_launch_test.cpp
        tests/bridge/screen_dispatch_test.cpp
        tests/bridge/input_routing_test.cpp
        tests/bridge/focus_reconcile_test.cpp
        tests/bridge/frame_tick_test.cpp
        tests/bridge/auth_outcome_test.cpp
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
        tests/math/tier_flow_test.cpp
    )
    target_include_directories(interrogator_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(interrogator_test PRIVATE interrogator GTest::gtest_main)

    add_test(NAME interrogator_test COMMAND interrogator_test)

    # -------------------------------------------------------------------------
    # Zone 03 (Audio Engine) — native unit tests.
    # -------------------------------------------------------------------------
    #
    # The `audio` library (shared section above) is held to -Werror, including the
    # no-op backend branch linked here. These tests exercise only the pure logic:
    # shuffle-pool semantics, the choreography schedule decision + scheduler, the
    # music transport gate, and the volume / mute / gesture-gate math. The audio
    # backend (miniaudio / HTML5 audio) is browser-verified, never unit-tested.
    add_executable(audio_test
        tests/audio/shuffle_pool_test.cpp
        tests/audio/choreography_test.cpp
        tests/audio/music_engine_test.cpp
        tests/audio/audio_engine_test.cpp
    )
    target_include_directories(audio_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(audio_test PRIVATE audio GTest::gtest_main)

    add_test(NAME audio_test COMMAND audio_test)

    # -------------------------------------------------------------------------
    # Zone 08 (Game Screen Rendering) — native unit tests.
    # -------------------------------------------------------------------------
    #
    # The `game` library (shared section above) is held to -Werror, including its
    # ImGui render TUs. These tests exercise only the pure logic — chip
    # decomposition + denomination tiers, scenario-type -> visual-state + position
    # labels, chip-push easing, the Frog click-counter state machine — never the
    # render TUs (no live ImGui frame).
    add_executable(game_test
        tests/render/chips_test.cpp
        tests/render/cards_test.cpp
        tests/render/opponents_test.cpp
        tests/animations/chip_slide_test.cpp
        tests/easter_egg/frog_toggle_test.cpp
    )
    target_include_directories(game_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(game_test PRIVATE game GTest::gtest_main)

    add_test(NAME game_test COMMAND game_test)

    # -------------------------------------------------------------------------
    # Zone 13 (Post-Round Screen) — native unit tests.
    # -------------------------------------------------------------------------
    #
    # The `post_round` library (shared section above) is held to its warning
    # baselines, including the ImGui render TUs. These tests exercise only the pure
    # recap model — stat-modal row assembly, tier-tab content selection, Summary
    # aggregation, tab-strip geometry hit-test, the Again state machine, the
    # pass -> dealer-expression mapping, the Time-Grade overtime/undertime decision,
    # and the side-pot-icon condition — never the render TUs (no live ImGui frame).
    add_executable(post_round_test
        tests/screens/again_button_test.cpp
        tests/render/stat_modal_test.cpp
    )
    target_include_directories(post_round_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(post_round_test PRIVATE post_round GTest::gtest_main)

    add_test(NAME post_round_test COMMAND post_round_test)

    # -------------------------------------------------------------------------
    # Zone 11 (Modal Infrastructure + Persistent Cluster) — native unit tests.
    # -------------------------------------------------------------------------
    #
    # The `modal` library (shared section above) is held to -Werror, including its
    # ImGui render TUs. These tests exercise only the pure logic: the real modal
    # stack (backbone modal_state.cpp, linked via modal), the outage-banner state
    # machine, the confirmation focus contract, the leaderboard search filter, and
    # the offline-indicator visibility mapping. They never invoke the render TUs (no
    # live ImGui frame).
    add_executable(modal_test
        tests/modal/modal_state_test.cpp
        tests/modal/outage_banner_test.cpp
        tests/modal/confirm_modal_test.cpp
        tests/modal/leaderboard_view_test.cpp
        tests/modal/shop_view_test.cpp
        tests/modal/offline_indicator_test.cpp
    )
    target_include_directories(modal_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(modal_test PRIVATE modal GTest::gtest_main)

    add_test(NAME modal_test COMMAND modal_test)

    # -------------------------------------------------------------------------
    # Zone 10 (Temporal Layer) — native unit tests.
    # -------------------------------------------------------------------------
    #
    # The `temporal` library (shared section above) is held to -Werror, including the
    # ImGui draw TU. These tests exercise only the pure logic: the target-time table +
    # modifiers + custom override, the advance state machine (accumulate / modal pause
    # / resume / disabled / clock-regression clamp), the countdown display format
    # (ceil undertime, the "0s" overtime pivot, the color token on each side), and the
    # exposed time-result queries (time_within_target inclusive, result-valid). They
    # never invoke render_countdown (no live ImGui frame).
    add_executable(temporal_test
        tests/temporal/target_time_test.cpp
        tests/temporal/delta_timer_test.cpp
        tests/temporal/visual_countdown_test.cpp
    )
    target_include_directories(temporal_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(temporal_test PRIVATE temporal GTest::gtest_main)

    add_test(NAME temporal_test COMMAND temporal_test)

    # -------------------------------------------------------------------------
    # Zone 12 (Settings Page) — native unit tests.
    # -------------------------------------------------------------------------
    #
    # The `settings` library (shared section above) is held to -Werror. These tests
    # exercise only the pure logic: the coupled street-split redistribution, the
    # two-handle difficulty-range clamp + display<->internal round-trip, the integer
    # parse/clamp/step behind the volume and custom-time inputs, the slider ghost
    # fraction, and the fuzzy search match / empty-result state. The section render
    # TUs (sections/*.cpp) are browser-verified, never invoked here (no ImGui frame).
    add_executable(settings_test
        tests/settings/settings_logic_test.cpp
        tests/settings/search_test.cpp
        tests/settings/username_denylist_test.cpp
        tests/settings/auth_form_test.cpp
    )
    target_include_directories(settings_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(settings_test PRIVATE settings GTest::gtest_main)

    add_test(NAME settings_test COMMAND settings_test)
endif()
