# The C++ layer

`<raydom/raydom.hh>` wraps the C API in namespace `rd`. It is header-only, needs C++20, and doesn't throw. Failures
are reported the same way as in C: null handles and `false`.

| C++               | C                    | Ownership                                     |
| ----------------- | -------------------- | --------------------------------------------- |
| `rd::Context`     | `rd_ctx*`            | Owns the context; move-only; destroys it      |
| `rd::Node`        | `rd_ctx*` + `rd_node`| Non-owning value, like the C handle           |
| `rd::Class`       | `rd_ctx*` + `rd_class` | Non-owning                                  |
| `rd::Style`       | `rd_style`           | Value; builder methods                        |
| `rd::Event`       | `rd_event*`          | Valid only inside the listener                |

`get()` and `handle()` return the C values, so C and C++ calls can be mixed freely on one context.

`rd::Node` and `rd::Class` are handles, so their setters are `const`, as with `std::span`: they change the node in
the context, not the handle. A node captured by value in a lambda, or a `const rd::Node`, can be changed directly.
Setters return the node by value, so a chain like `auto n = ctx.create().style(s).add_class(c);` is safe to keep.
Both hash (`std::hash`), for use as keys in unordered containers.

The C structs meant for designated initializers, such as `rdx_slider_desc`, give every field a default in C++, so
`rdx::slider(ctx, {.max = 10})` compiles without missing-initializer warnings from GCC or Clang.

`raydom::extras` components are typed handles: `rdx::slider` returns an `rdx::Slider`, an `rd::Node` with the
slider's operations as members (`value()`, `set_value()`, `set_range()`); likewise `Toggle`, `RadioGroup`, `Number`,
`Select`, `Tabs`, `Split`, `List`, `Table` and `Tree`. They convert to `rd::Node`, and the free functions taking an
`rd::Node` (`rdx::slider_value(node)`) work on any handle.

## Styles

`rd::Style` has one chainable setter per property, named as in the [reference](../reference/properties.md), plus
overloads for common shapes:

```cpp
using namespace rd::literals;

rd::Style()
    .padding(8_px, 16_px)                             // vertical, horizontal; also (all) and (t, r, b, l)
    .border_radius(6_px)                              // all corners
    .box_shadow(0, 4, 12, rd::rgba(0, 0, 0, 120))     // x, y, blur, color, spread = 0
    .grid_auto_rows(40_px)                            // any rd_val becomes a track
    .gap(8_px)                                        // row_gap and column_gap
    .flex(1);                                         // grow 1, shrink 1, basis 0
```

Literals: `_px`, `_pct`, `_fr`. Functions: `rd::px`, `rd::pct`, `rd::num`, `rd::fr`, `rd::hex`, `rd::rgb`,
`rd::rgba`, `rd::line`, `rd::span`, `rd::minmax`, `rd::repeat`. Constants: `rd::auto_`, `rd::min_content`,
`rd::max_content`. `-5_px` works for negative offsets.

## Callbacks

`node.on(type, f, capture)`, `node.on_click(f)`, `node.paint(f)` and `node.measure(f)` take any callable. The
callable is moved into a heap allocation owned by raydom and deleted when the listener is removed or the node is
destroyed. Captured references must outlive the node, or the listener must be removed first. Callbacks run from C,
so they are `noexcept`: an exception escaping one terminates the program rather than unwinding through C frames.

`node.emplace_userdata<T>(args...)` constructs a `T` owned by the node; `node.userdata<T>()` returns it.

## The quickstart, annotated

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

- `button_style` is an `rd::Class`: a shared style, not a node. `button_style.on(RD_STATE_HOVER, ...)` is the C++
  form of `rd_class_set_state`.
- `create_text(s, style)` creates a text node. `create_button(s, style)` does the same with the button
  [role](../guide/roles.md), which makes it focusable and activatable with Enter and Space.
- `append` returns the parent, so a subtree builds in one expression.
- The lambda captures `label` by reference; both live in the same scope as the context.
