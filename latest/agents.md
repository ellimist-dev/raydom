# For AI agents

This page is for coding agents and the tools that feed them. It states the facts an agent needs to write correct
raydom code, in a form that is cheap to retrieve.

## Machine-readable copies

Every published version of this book has, next to its HTML:

| Path              | Contents                                                       |
| ----------------- | -------------------------------------------------------------- |
| `llms.txt`        | Index of pages with one-line descriptions, per [llmstxt.org](https://llmstxt.org/) |
| `llms-full.txt`   | The whole book as one Markdown file, code samples inlined      |
| `<page>.md`       | Each page as Markdown, at the HTML page's path with `.md`      |

Use the copy under the version that matches the project's `GIT_TAG` or `find_package` version, for example
`/v0.1.1/llms-full.txt`. `/latest/` follows the newest release; `/dev/` follows the main branch and may
describe unreleased API.

The headers are the ground truth. `include/raydom/*.h` documents every function in a comment above it, and
`include/raydom/style.h` defines every style property in the `RD_STYLE_PROPS` X-macro.

## Rules

1. Create the context after `InitWindow` and destroy it before `CloseWindow`. In C++, scope `rd::Context` in a block
   that ends before `CloseWindow`.
2. Load a font before creating text. There is no built-in font.
3. Call `rd_update` before `BeginDrawing` and `rd_draw` between `BeginDrawing` and `EndDrawing`, once per frame.
4. A new node is detached. Append it under `rd_ctx_root(ctx)` or a descendant, or it isn't drawn.
5. `rd_style` fields left out are unset, and unset fields fall through to classes, inheritance and defaults.
   Write `&(rd_style){.width = rd_px(100)}`. Never pass a computed style as an inline style: it sets every
   property and overrides all classes.
6. The default `flex_direction` is column and the default `flex_shrink` is 0. Set `flex_direction = RD_FLEX_ROW`
   for rows. Set `flex_shrink` explicitly if items must shrink.
7. Inline styles beat class state variants. Put hover, focus and active styling in classes.
8. Lengths need units: `rd_px(8)`, not `8`. Unitless values (`flex_grow`, `opacity`, `line_clamp`, multiplier
   `line_height`) use `rd_num`.
9. Grid track lists are handles: build them with `rd_tracks_create` (C) or `ctx.tracks({...})` (C++).
10. Handles are values. Store `rd_node`, not pointers. Check `rd_node_alive` when a node may have been destroyed.
11. Listeners may mutate and destroy nodes, including their own node. A destroyed node is freed when dispatch
    ends.
12. Geometry (`rd_node_rect`, `rd_node_bounds`) reflects the last layout. After building or changing the tree
    outside the frame loop, call `rd_ctx_layout` before reading it.
13. The C++ layer doesn't throw. Check returned handles and `bool` results where failure matters.
14. raydom doesn't own the main loop, the window, or time. The application drives all three.
15. Create controls with their role: `rd_node_create_button` / `ctx.create_button` for buttons, `rd_node_create_input`
    / `ctx.create_input` for inputs, `rd_node_set_role` for headings, lists, links and checkboxes. Don't build a
    button from a plain node and a click listener; it gets no role and no default focus.

## Minimal program

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

## Verifying generated code

Headless programs can drive input and assert on results without a window; see
[The frame loop](getting-started/frame-loop.md#without-raylib-input). The repository's `docs/examples/` holds
programs of this kind for each chapter.
