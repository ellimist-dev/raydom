# Quickstart

raydom has no built-in font. Load a TrueType or OpenType file before creating text; the first font loaded becomes
the default for every node.

## C

```c
{{#include ../../examples/quickstart.c:all}}
```

## C++

```cpp
{{#include ../../examples/quickstart.cc:all}}
```

The C++ context is an RAII owner. It is scoped inside a block so it is destroyed before `CloseWindow`; raydom owns
GPU textures and a shader that must be released while the window still exists.

## Build

```cmake
cmake_minimum_required(VERSION 3.25)
project(hello LANGUAGES C CXX)

include(FetchContent)
FetchContent_Declare(raydom
    GIT_REPOSITORY https://github.com/ellimist-dev/raydom.git
    GIT_TAG        v{{version}})
FetchContent_MakeAvailable(raydom)

add_executable(hello main.cc)
target_link_libraries(hello PRIVATE raydom::raydom)
target_compile_definitions(hello PRIVATE FONT_PATH="${CMAKE_SOURCE_DIR}/font.ttf")
```

For a single binary with no files next to it, embed the font and use `rd_font_load(ctx, data, size, false)`. With
C23 `#embed`:

```c
static const unsigned char font_data[] = {
#embed "font.ttf"
};
rd_font_load(ctx, font_data, sizeof font_data, false);  // false: the data outlives the context
```
