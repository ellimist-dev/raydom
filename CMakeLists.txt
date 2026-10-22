cmake_minimum_required(VERSION 3.25)
project(raydom VERSION 0.2.5 LANGUAGES C CXX)

option(RAYDOM_FETCH_RAYLIB "Download raylib when find_package can't find it" ON)
option(RAYDOM_BUILD_EXTRAS "Build raydom::extras" ON)
option(RAYDOM_FILE_DIALOG "Native file dialogs in raydom::extras (needs libdbus on Linux)" ON)
option(RAYDOM_INSTALL "Generate install and package rules" ${PROJECT_IS_TOP_LEVEL})
option(RAYDOM_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})
option(RAYDOM_BUILD_EXAMPLES "Build examples" ${PROJECT_IS_TOP_LEVEL})
option(RAYDOM_SANITIZE "Build raydom and its tests with ASan and UBSan" OFF)
option(RAYDOM_TEST_GRAPHICS "Also test drawing, which opens a window and needs a display" OFF)

# The macOS file dialog backend in extras is Objective-C.
if (APPLE AND RAYDOM_BUILD_EXTRAS AND RAYDOM_FILE_DIALOG)
    enable_language(OBJC)
endif ()

if (PROJECT_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif ()

include(cmake/fetch_raylib.cmake)
include(GNUInstallDirs)

set(RAYDOM_WARNINGS $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra>)
set(RAYDOM_SANITIZERS $<$<AND:$<BOOL:${RAYDOM_SANITIZE}>,$<NOT:$<C_COMPILER_ID:MSVC>>>:-fsanitize=address,undefined>)
set(RAYDOM_NO_EXCEPTIONS $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-fno-exceptions>)

configure_file(cmake/version.h.in ${PROJECT_BINARY_DIR}/include/raydom/version.h @ONLY)

add_library(raydom STATIC
    src/tree.c
    src/style.c
    src/layout.c
    src/grid.c
    src/font.c
    src/text.c
    src/display.c
    src/events.c
    src/input.c
    src/selection.c
    src/render.c
    src/role.c
    src/overlay.c
    src/image.c
    src/timer.c
    src/anim.c
    src/scratch.c
)
add_library(raydom::raydom ALIAS raydom)
target_include_directories(raydom
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE src)
# Consumers need C11 for the C headers (with GCC and Clang) and C++20 for the C++ layer; raydom itself is C23.
target_compile_features(raydom PUBLIC c_std_11 cxx_std_20 PRIVATE c_std_23)
target_link_libraries(raydom PUBLIC raylib)
target_compile_options(raydom PRIVATE ${RAYDOM_WARNINGS} ${RAYDOM_SANITIZERS})
# Sanitized builds link the runtime into this tree's programs only; an installed package never asks for it.
target_link_options(raydom PUBLIC $<BUILD_INTERFACE:${RAYDOM_SANITIZERS}>)

if (APPLE)
    target_link_libraries(raydom PUBLIC "-framework CoreVideo" "-framework IOKit" "-framework Cocoa"
                                        "-framework OpenGL")
endif ()

if (RAYDOM_BUILD_EXTRAS)
    add_subdirectory(extras)
endif ()

if (RAYDOM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

if (RAYDOM_BUILD_TESTS AND NOT EMSCRIPTEN)
    # The tests' font is raylib's Anonymous Pro: from raylib's sources when downloaded, else fetched alone.
    set(font "${raylib_SOURCE_DIR}/examples/text/resources/anonymous_pro_bold.ttf")
    if (NOT DEFINED raylib_SOURCE_DIR OR NOT EXISTS "${font}")
        set(font "${PROJECT_BINARY_DIR}/test-font/anonymous_pro_bold.ttf")
        if (NOT EXISTS "${font}")
            file(DOWNLOAD "https://raw.githubusercontent.com/raysan5/raylib/${RAYDOM_RAYLIB_VERSION}/examples/text/resources/anonymous_pro_bold.ttf"
                 "${font}" EXPECTED_HASH SHA256=52895657e7d48860089ebfac63a27244f37a2218ec9c5019a0b37fe885c0fc0c STATUS status)
            list(GET status 0 code)
            if (NOT code EQUAL 0)
                file(REMOVE "${font}")
            endif ()
        endif ()
    endif ()
    if (NOT EXISTS "${font}")
        message(WARNING "raydom: tests need raylib's Anonymous Pro font, which couldn't be found or downloaded; skipping them")
    else ()
        enable_testing()
        foreach (t test_tree test_tree_cpp test_style test_style_cpp test_layout test_layout_cpp test_grid
                   test_grid_cpp test_text test_text_cpp test_render test_events test_events_cpp test_input
                   test_input_cpp test_selection test_role test_cursor test_overlay test_scrollbar test_image test_timer test_anim test_graph test_dialog test_controls test_popups test_views test_extras_cpp)
            file(GLOB src tests/${t}.c tests/${t}.cc)
            add_executable(${t} ${src})
            target_link_libraries(${t} $<IF:$<TARGET_EXISTS:raydom_extras>,raydom_extras,raydom>)
            target_include_directories(${t} PRIVATE src)
            target_compile_options(${t} PRIVATE ${RAYDOM_WARNINGS} ${RAYDOM_SANITIZERS} ${RAYDOM_NO_EXCEPTIONS})
            target_compile_definitions(${t} PRIVATE "RAYDOM_TEST_FONT=\"${font}\"")
            target_compile_features(${t} PRIVATE c_std_23)
            add_test(NAME ${t} COMMAND ${t})
        endforeach ()

        if (RAYDOM_TEST_GRAPHICS)
            add_executable(test_render_gl tests/test_render_gl.c)
            target_link_libraries(test_render_gl raydom)
            target_compile_options(test_render_gl PRIVATE ${RAYDOM_WARNINGS} ${RAYDOM_SANITIZERS})
            target_compile_definitions(test_render_gl PRIVATE "RAYDOM_TEST_FONT=\"${font}\"")
            target_compile_features(test_render_gl PRIVATE c_std_23)
            add_test(NAME test_render_gl COMMAND test_render_gl)
            # GL drivers keep allocations for the process's life, which LeakSanitizer would report.
            set_tests_properties(test_render_gl PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" SKIP_RETURN_CODE 77)
        endif ()

        # Programs quoted by the docs. All of them compile; the ones that don't open a window also run.
        file(GLOB doc_sources docs/examples/*.c docs/examples/*.cc)
        foreach (f ${doc_sources})
            file(READ ${f} content)
            if (content MATCHES "raydom/extras" AND NOT TARGET raydom_extras)
                continue()
            endif ()
            get_filename_component(name ${f} NAME_WE)
            get_filename_component(ext ${f} LAST_EXT)
            string(REPLACE "." "" ext ${ext})
            set(t doc_${name}_${ext})
            add_executable(${t} ${f})
            target_link_libraries(${t} $<IF:$<TARGET_EXISTS:raydom_extras>,raydom_extras,raydom>)
            target_compile_options(${t} PRIVATE ${RAYDOM_WARNINGS} ${RAYDOM_SANITIZERS})
            target_compile_definitions(${t} PRIVATE "FONT_PATH=\"${font}\"")
            if (NOT content MATCHES "InitWindow")
                add_test(NAME ${t} COMMAND ${t})
            endif ()
        endforeach ()
    endif ()
endif ()

if (RAYDOM_INSTALL)
    include(CMakePackageConfigHelpers)
    set(targets raydom)
    if (TARGET raydom_extras)
        list(APPEND targets raydom_extras)
        install(DIRECTORY extras/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    endif ()
    install(TARGETS ${targets} EXPORT raydomTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(DIRECTORY include/ ${PROJECT_BINARY_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(EXPORT raydomTargets NAMESPACE raydom:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/raydom)

    # Until 1.0, minor versions may break compatibility.
    if (PROJECT_VERSION_MAJOR EQUAL 0)
        set(compat SameMinorVersion)
    else ()
        set(compat SameMajorVersion)
    endif ()
    configure_package_config_file(cmake/raydomConfig.cmake.in ${PROJECT_BINARY_DIR}/raydomConfig.cmake
                                  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/raydom)
    write_basic_package_version_file(${PROJECT_BINARY_DIR}/raydomConfigVersion.cmake COMPATIBILITY ${compat})
    install(FILES ${PROJECT_BINARY_DIR}/raydomConfig.cmake ${PROJECT_BINARY_DIR}/raydomConfigVersion.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/raydom)
endif ()
