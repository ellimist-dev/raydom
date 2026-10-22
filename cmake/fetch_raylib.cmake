# Provides the raylib target: one the parent project already defined, an installed raylib, or a
# download when RAYDOM_FETCH_RAYLIB is on.
set(RAYDOM_RAYLIB_VERSION 6.0)

if (TARGET raylib)
    return()
endif ()

# 6.0 exactly: raylib's minor versions change its API.
find_package(raylib ${RAYDOM_RAYLIB_VERSION}...<6.1 QUIET)
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
    URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYDOM_RAYLIB_VERSION}.tar.gz
    URL_HASH SHA256=2b3ee1e2120c7a0796b33062c7e9a694dd8a8caa56a96319ac8c8ecf54a90d0b)
# A plain variable, scoped to this directory, so a parent's own BUILD_EXAMPLES is left alone.
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(BUILD_EXAMPLES OFF)
FetchContent_MakeAvailable(raylib)
