# raydom

A retained-mode UI tree for [raylib](https://www.raylib.com/), written in C with a C++ layer. You build a tree of
nodes, style them with CSS-like properties, and raydom handles layout, text, input and drawing. Apps stay a single
statically linked binary.

- Flexbox and grid layout
- Styles, reusable classes, and hover/focus/active/disabled variants
- Text: wrapping, alignment, ellipsis, line clamping, highlights, drag selection
- Pointer events with capture and bubbling, keyboard focus, Tab navigation
- Text inputs, single- and multi-line, with selection, clipboard and undo
- Rounded boxes, borders, shadows, scrolling, clipping
- Canvas nodes for custom raylib drawing
- `raydom::extras`: an interactive network view (3D spheres, edges, labels, pan and zoom)

## Status

raydom follows [semantic versioning](https://semver.org/). Before 1.0, minor versions may contain breaking changes;
patch versions don't.

## Requirements

- CMake 3.25 or newer
- A C23 compiler for C code using raydom (clang 18+, GCC 13+); a C++20 compiler for the C++ layer
- raylib 6.0, which raydom downloads if your project doesn't already provide it

## Installation

### FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(raydom
    GIT_REPOSITORY https://github.com/ellimist-dev/raydom.git
    GIT_TAG        v0.1.1)
FetchContent_MakeAvailable(raydom)

target_link_libraries(your_app PRIVATE raydom::raydom)   # or raydom::extras
```

If your project already defines a `raylib` target, raydom uses it. Otherwise it looks for an installed raylib, then
downloads one; set `RAYDOM_FETCH_RAYLIB=OFF` to require an installed copy instead.

### Installed package

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DRAYDOM_INSTALL=ON
cmake --build build
cmake --install build --prefix /your/prefix
```

```cmake
find_package(raydom 0.1 CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE raydom::raydom)
```

### Options

| Option                 | Default            | Effect                                         |
| ---------------------- | ------------------ | ---------------------------------------------- |
| `RAYDOM_BUILD_EXTRAS`  | `ON`               | Build `raydom::extras`                         |
| `RAYDOM_FETCH_RAYLIB`  | `ON`               | Download raylib when it can't be found         |
| `RAYDOM_INSTALL`       | top-level only     | Generate install rules and the CMake package   |
| `RAYDOM_BUILD_TESTS`   | top-level only     | Build the test suite                           |
| `RAYDOM_BUILD_EXAMPLES`| top-level only     | Build the examples                             |
| `RAYDOM_SANITIZE`      | `OFF`              | Build with AddressSanitizer and UBSan          |

## Usage

raydom has no built-in font, so load one before showing text. `rd_update` reads raylib's input and lays the tree out
at the window size; `rd_draw` draws it.

### C

```c
#include <raydom/raydom.h>
#include <raylib.h>

int main(void) {
  InitWindow(800, 600, "hello");
  rd_ctx* ctx = rd_ctx_create();
  rd_font_load_file(ctx, "font.ttf");

  rd_node card = rd_node_create(ctx);
  rd_node_set_style(ctx, card, &(rd_style){
      .padding       = rd_edges_all(rd_px(16)),
      .border_radius = rd_corners_all(rd_px(8)),
      .background    = rd_hex(0x1a1f2b),
      .color         = rd_hex(0xe4e7ee),
  });
  rd_node_set_text(ctx, card, "Hello from raydom", RD_TEXT_NUL);
  rd_node_append(ctx, rd_ctx_root(ctx), card);

  while (!WindowShouldClose()) {
    rd_update(ctx);
    BeginDrawing();
    ClearBackground(BLACK);
    rd_draw(ctx);
    EndDrawing();
  }
  rd_ctx_destroy(ctx);  // before CloseWindow, so GPU resources are released
  CloseWindow();
}
```

### C++

```cpp
#include <raydom/raydom.hh>
#include <raylib.h>

#include <string>

using namespace rd::literals;

int main() {
  InitWindow(800, 600, "hello");
  {
    rd::Context ctx;
    ctx.load_font_file("font.ttf");

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
  }
  CloseWindow();
}
```

### Extras

Link `raydom::extras` and include `<raydom/extras/graph.h>` (C) or `<raydom/extras/graph.hh>` (C++) for the network
view. `examples/network` shows it driving a deterministic Paxos simulation.

## Building from source

With [just](https://github.com/casey/just):

```sh
just build      # development build with sanitizers
just test       # run the test suite
just showcase   # feature tour
just network    # network view example
just consumer   # check that FetchContent and find_package both work
just clean      # remove build outputs
```

## License

MIT. See [LICENSE](LICENSE).
