# Quickstart

raydom has no built-in font. Load a TrueType or OpenType file before creating text; the first font loaded becomes
the default for every node.

## C

```c
#include <raydom/raydom.h>
#include <raylib.h>

int main(void) {
  SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
  InitWindow(800, 600, "hello");
  rd_ctx* ctx = rd_ctx_create();
  rd_font_load_file(ctx, FONT_PATH);  // any .ttf or .otf file

  rd_node card = rd_node_create_text(ctx, "Hello from raydom", RD_TEXT_NUL);
  rd_node_set_style(ctx, card,
                    &(rd_style){
                        .padding       = rd_edges_all(rd_px(16)),
                        .border_radius = rd_corners_all(rd_px(8)),
                        .background    = rd_hex(0x1a1f2b),
                        .color         = rd_hex(0xe4e7ee),
                    });
  rd_node_append(ctx, rd_ctx_root(ctx), card);

  while (!WindowShouldClose()) {
    rd_update(ctx);  // input, then layout at the window size
    BeginDrawing();
    ClearBackground(BLACK);
    rd_draw(ctx);
    EndDrawing();
  }
  rd_ctx_destroy(ctx);  // before CloseWindow, so GPU resources are released
  CloseWindow();
}
```

## C++

```cpp
#include <raydom/raydom.hh>
#include <raylib.h>

#include <string>

using namespace rd::literals;

int main() {
  SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
  InitWindow(800, 600, "hello");
  {
    rd::Context ctx;
    ctx.load_font_file(FONT_PATH);  // any .ttf or .otf file

    rd::Class button_style = ctx.create_class(rd::Style()
                                                  .padding(8_px, 16_px)
                                                  .border_radius(8_px)
                                                  .background(rd::hex(0x5b8cff))
                                                  .color(rd::hex(0xffffff)));
    button_style.on(RD_STATE_HOVER, rd::Style().background(rd::hex(0x7aa2ff)));

    int      clicks = 0;
    rd::Node label  = ctx.create_text("Clicked 0 times", rd::Style().color(rd::hex(0xe4e7ee)));
    rd::Node button = ctx.create_button("Click me").add_class(button_style);  // Tab focuses it; Enter and Space click it
    button.on_click([&] { label.text("Clicked " + std::to_string(++clicks) + " times"); });

    ctx.root()
        .style(rd::Style().padding(24_px).row_gap(12_px).align_items(RD_ALIGN_START))
        .append(button)
        .append(label);

    while (!WindowShouldClose()) {
      ctx.update();
      BeginDrawing();
      ClearBackground(BLACK);
      ctx.draw();
      EndDrawing();
    }
  }  // the context is destroyed here, before CloseWindow
  CloseWindow();
}
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
    GIT_TAG        v0.2.1)
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
