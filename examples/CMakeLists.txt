set(SHOWCASE_FONT_DIR "${raylib_SOURCE_DIR}/examples/text/resources")
if (NOT EXISTS "${SHOWCASE_FONT_DIR}/anonymous_pro_bold.ttf")
    message(STATUS "raydom: showcase needs raylib's example fonts; skipping")
    return()
endif ()

if (NOT TARGET raydom_extras)
    message(STATUS "raydom: showcase needs raydom::extras; skipping")
    return()
endif ()

# The showcase embeds its font with #embed: Clang 19+, Apple Clang 17+, GCC 15+.
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "-std=c++20 --embed-dir=${SHOWCASE_FONT_DIR} -Wno-c23-extensions")
check_cxx_source_compiles("const unsigned char f[] = {\n#embed \"anonymous_pro_bold.ttf\"\n};\nint main() { return f[0]; }"
                          RAYDOM_HAVE_EMBED)
unset(CMAKE_REQUIRED_FLAGS)
if (NOT RAYDOM_HAVE_EMBED)
    message(STATUS "raydom: the showcase needs a compiler with #embed; skipping")
    return()
endif ()

if (NOT TARGET raydom_router)
    message(STATUS "raydom: showcase needs raydom::router; skipping")
    return()
endif ()

add_executable(showcase showcase/main.cc showcase/tour.cc showcase/messages.cc showcase/network.cc showcase/routes.cc)
target_link_libraries(showcase raydom_extras raydom_router)
target_compile_options(showcase PRIVATE --embed-dir=${SHOWCASE_FONT_DIR} $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-c23-extensions>)
set_source_files_properties(showcase/main.cc PROPERTIES OBJECT_DEPENDS "${SHOWCASE_FONT_DIR}/anonymous_pro_bold.ttf")

if (EMSCRIPTEN)
    # The browser drives the frames (emscripten_set_main_loop), so no Asyncify. One self-contained page,
    # from web/shell.html: full window, sized in device pixels. just serve builds and serves it.
    set(shell ${CMAKE_CURRENT_SOURCE_DIR}/showcase/web/shell.html)
    target_link_options(showcase PRIVATE -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 -sSINGLE_FILE=1 --shell-file ${shell})
    set_target_properties(showcase PROPERTIES SUFFIX ".html" LINK_DEPENDS ${shell})
endif ()
