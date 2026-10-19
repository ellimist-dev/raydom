# Installation

## Requirements

- CMake 3.25 or newer.
- A C23 compiler for C code that includes raydom (clang 18+, GCC 13+). A C++20 compiler for the C++ layer.
- raylib 6.0. raydom downloads it when the project doesn't provide one.

## FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(raydom
    GIT_REPOSITORY https://github.com/ellimist-dev/raydom.git
    GIT_TAG        v0.2.3)
FetchContent_MakeAvailable(raydom)

target_link_libraries(your_app PRIVATE raydom::raydom)
```

Link `raydom::extras` instead to get the [network view](../extras/graph.md) and [file dialogs](../extras/dialog.md);
it links `raydom::raydom` for you.

raylib is resolved in this order: an existing `raylib` target in the project, an installed raylib found with
`find_package`, then a download. Set `RAYDOM_FETCH_RAYLIB=OFF` to require one of the first two.

## Installed package

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DRAYDOM_INSTALL=ON
cmake --build build
cmake --install build --prefix /your/prefix
```

```cmake
find_package(raydom 0.2 CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE raydom::raydom)
```

The package's version check accepts any patch release of the requested minor version while raydom is below 1.0.
After 1.0 it accepts any later release with the same major version.

## Options

| Option                  | Default        | Effect                                       |
| ----------------------- | -------------- | -------------------------------------------- |
| `RAYDOM_BUILD_EXTRAS`   | `ON`           | Build `raydom::extras`                       |
| `RAYDOM_FILE_DIALOG`    | `ON`           | Native [file dialogs](../extras/dialog.md) in extras; needs libdbus on Linux |
| `RAYDOM_FETCH_RAYLIB`   | `ON`           | Download raylib when it can't be found       |
| `RAYDOM_INSTALL`        | top-level only | Generate install rules and the CMake package |
| `RAYDOM_BUILD_TESTS`    | top-level only | Build the test suite                         |
| `RAYDOM_BUILD_EXAMPLES` | top-level only | Build the examples                           |
| `RAYDOM_SANITIZE`       | `OFF`          | Build with AddressSanitizer and UBSan        |

## Headers

| Header                         | Contents                                   |
| ------------------------------ | ------------------------------------------ |
| `<raydom/raydom.h>`            | The whole C API                            |
| `<raydom/raydom.hh>`           | The C++ layer; includes the C API          |
| `<raydom/extras/graph.h>`      | Network view, C                            |
| `<raydom/extras/graph.hh>`     | Network view, C++                          |
| `<raydom/extras/controls.h>`   | Checkbox, switch, radio, slider, C         |
| `<raydom/extras/toast.h>`      | Toasts, C                                  |
| `<raydom/extras/popups.h>`     | Menus, selects, modals, tooltips, C        |
| `<raydom/extras/popups.hh>`    | Menus, selects, modals, tooltips, C++      |
| `<raydom/extras/views.h>`      | Tabs, splitter, list, table, tree, C       |
| `<raydom/extras/views.hh>`     | Tabs, splitter, list, table, tree, C++     |
| `<raydom/extras/theme.h>`      | Classes behind the controls and toasts     |
| `<raydom/extras/controls.hh>`  | Controls and toasts, C++                   |
| `<raydom/extras/dialog.h>`     | File dialogs, C                            |
| `<raydom/extras/dialog.hh>`    | File dialogs, C++                          |
| `<raydom/image.h>`             | Images and image nodes                     |
| `<raydom/overlay.h>`           | The overlay layer and anchored positioning |
| `<raydom/animate.h>`           | Explicit animations; transitions are style properties |
| `<raydom/timer.h>`             | Timers                                     |
| `<raydom/role.h>`              | Roles and the button, text and input constructors |
| `<raydom/version.h>`           | `RD_VERSION_MAJOR`, `_MINOR`, `_PATCH` |
