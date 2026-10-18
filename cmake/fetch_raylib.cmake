# Provides the raylib target: one the parent project already defined, an installed raylib, or a
# download when RAYDOM_FETCH_RAYLIB is on.
set(RAYDOM_RAYLIB_VERSION 6.0)

if (TARGET raylib)
    return()
endif ()

find_package(raylib ${RAYDOM_RAYLIB_VERSION} QUIET)
if (raylib_FOUND)
    return()
endif ()

if (NOT RAYDOM_FETCH_RAYLIB)
    message(FATAL_ERROR "raydom: raylib ${RAYDOM_RAYLIB_VERSION} not found and RAYDOM_FETCH_RAYLIB is off")
endif ()

include(FetchContent)
FetchContent_Declare(
    raylib
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYDOM_RAYLIB_VERSION}.tar.gz)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)
