# Roles

A role says what a node is: a button, a heading, a text input. raydom doesn't draw or behave differently by role,
with one exception below. Roles exist so a future accessibility backend can describe the tree to screen readers
without every application revisiting its controls. Set them now and that work becomes a backend change only.

## Constructors

The direct way to get a role is to create the node with one:

| C                               | C++                              | Role                                  |
| ------------------------------- | -------------------------------- | ------------------------------------- |
| `rd_node_create_text(ctx, s, n)`| `ctx.create_text(s, style)`      | text                                  |
| `rd_node_create_button(ctx, s, n)` | `ctx.create_button(s, style)` | button, with a text label            |
| `rd_node_create_button(ctx, NULL, 0)` | `ctx.create_button(style)` | button, empty, for icon or composite content |
| `rd_node_create_input(ctx, flags)` | `ctx.create_input(multiline, style)` | text input or multi-line text input |

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

Each returns a detached node, or a null node on allocation failure.

## Setting a role

```c
rd_node_set_role(ctx, title, RD_ROLE_HEADING);
rd_node_set_role(ctx, menu, RD_ROLE_LIST);
rd_role r = rd_node_role(ctx, node);
```

`RD_ROLE_AUTO`, the default, derives the role from the node: multi-line or single-line text input for inputs, text
for other text leaves, canvas for nodes with a paint callback, and generic otherwise. `rd_node_role` returns the
resolved role. Setting `RD_ROLE_AUTO` returns a node to its derived role.

The roles are a subset of [AccessKit](https://accesskit.dev/)'s: generic, group, button, link, checkbox, radio
button, text, label, heading, text input, multi-line text input, list, list item, image, canvas. More can be added
in a minor release.

## Focus

Interactive roles (button, link, checkbox, radio button) take focus without a `focusable` style, as inputs do:

- Tab reaches them, unless their `focusable` is `POINTER`, which keeps pointer focus and skips Tab.
- Enter and Space on a focused one fire `CLICK`, as for any focused node that isn't an input.
- To take one out of focus and pointer input, set `RD_STATE_DISABLED` on it.

Roles carry no other behavior. A checkbox doesn't toggle itself; the application tracks its state and styles it.
