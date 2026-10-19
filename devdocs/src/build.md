# Build and packaging

## Targets

| Target            | Alias             | Contents                                                      |
| ----------------- | ----------------- | ------------------------------------------------------------- |
| `raydom`          | `raydom::raydom`  | The core: `src/*.c` plus vendored stb and nanosvg, compiled in |
| `raydom_extras`   | `raydom::extras`  | `extras/src/*.c`, the file dialog backend; links `raydom`     |

Both are static libraries. `raydom` publicly requires `c_std_23` and `cxx_std_20`, links raylib publicly, and on
macOS the frameworks raylib needs. Warnings (`-Wall -Wextra`) and sanitizers are private.

## raylib

`cmake/fetch_raylib.cmake` provides a `raylib` target, in order: an existing target in the parent project, an
installed raylib 6.0 from `find_package`, then a download with FetchContent (unless `RAYDOM_FETCH_RAYLIB=OFF`).
Projects that already build raylib get theirs used, not a second copy.

## Vendored code

| Code                         | Where                        | Linkage                                     |
| ---------------------------- | ---------------------------- | ------------------------------------------- |
| stb_truetype                 | `src/external/`              | `STBTT_STATIC`: no exported symbols         |
| stb_image                    | `src/external/`              | `STB_IMAGE_STATIC`                          |
| nanosvg, nanosvgrast         | `src/external/`              | Public functions renamed with `#define` before inclusion |
| nativefiledialog-extended    | `extras/src/external/nfd/`   | One platform backend compiled into extras   |

raylib bundles its own stb libraries, and applications may bundle nanosvg; the static and renamed linkage keeps
them from colliding. The vendored files are unmodified so they can be updated by copying; `VERSION` files record
where they came from.

## Platforms

- The core uses only C23 and raylib, and builds wherever raylib does, including Emscripten.
- The file dialog backend is Objective-C on macOS, so the top-level `CMakeLists.txt` enables `OBJC` right after
  `project()` when extras and dialogs are on. Enabling it inside `extras/` fails (`CMAKE_OBJC_COMPILE_OBJECT` is
  missing for targets in that directory).
- On Linux the dialog backend is xdg-desktop-portal over libdbus (`pkg-config dbus-1`). `RAYDOM_FILE_DIALOG=OFF`
  compiles a stub that returns errors, so the API stays available.

## Options

`RAYDOM_BUILD_EXTRAS`, `RAYDOM_FILE_DIALOG`, `RAYDOM_FETCH_RAYLIB`, `RAYDOM_INSTALL`, `RAYDOM_BUILD_TESTS`,
`RAYDOM_BUILD_EXAMPLES`, `RAYDOM_SANITIZE`. Tests, examples and install rules default to on only when raydom is the
top-level project, so a project embedding it with FetchContent builds just the libraries.

## Installing

`RAYDOM_INSTALL=ON` installs the libraries, headers (including the generated `version.h`) and a CMake package:
`raydomConfig.cmake` calls `find_dependency(raylib)` and imports `raydom::raydom` and `raydom::extras`. The version
file uses `SameMinorVersion` while the major version is 0 and `SameMajorVersion` after.

Platform libraries the extras need (AppKit, UniformTypeIdentifiers, ole32, dbus-1) are attached as plain link
names, so they survive export without imported targets.

`just consumer` is the packaging test: it builds `tests/consumer` both ways and runs it.

## Versioning

Semantic versioning; the version lives in `project(raydom VERSION ...)` and flows into `version.h`, the package
version file and the docs. Before 1.0 a minor release may break the API, and changing the numeric value of a public
enumerator counts as breaking.
