# The frame loop

```c
while (!WindowShouldClose()) {
  rd_update(ctx);
  BeginDrawing();
  ClearBackground(BLACK);
  rd_draw(ctx);
  EndDrawing();
}
```

`rd_update` does four things, in order:

1. Sets the context clock from `GetTime()` (`rd_ctx_set_time`).
2. Feeds raylib's keyboard and text input (`rd_ctx_key`, `rd_ctx_text`).
3. Feeds the mouse position, buttons and wheel (`rd_ctx_pointer`). Listeners run here.
4. Lays the tree out at the screen size (`rd_ctx_layout`).

`rd_draw` builds a display list from the laid-out tree and executes it with raylib. It must be called between
`BeginDrawing` and `EndDrawing`. Anything drawn with raylib after `rd_draw` appears on top of the UI.

## Mutating the tree

Tree and style changes are allowed at any time: in listeners, between frames, or from application logic. They are
recorded as dirty flags; nothing is recomputed until the next layout. Several changes in one frame cost one layout.

Geometry queries such as `rd_node_rect` return the result of the most recent layout. After changing the tree outside
the frame loop, call `rd_ctx_layout` before reading geometry.

## Without raylib input

`rd_update` is a convenience. Applications with their own input handling, and tests, call the lower-level functions
directly:

```c
rd_ctx_set_time(ctx, seconds);
rd_ctx_pointer(ctx, x, y, RD_BUTTON_PRIMARY, 0, 0);  // position, held buttons, wheel
rd_ctx_key(ctx, RD_KEY_TAB, true, false, 0);          // key, down, repeat, modifiers
rd_ctx_text(ctx, 'a');                                // one codepoint
rd_ctx_layout(ctx, width, height);
```

Nothing except `rd_update`, `rd_draw`, and paint callbacks touches raylib, so a context can be built, laid out and
driven with input without a window. The headless examples behind the guide chapters run this way.

## Lifetime

Destroy the context before `CloseWindow`. `rd_ctx_destroy` frees every node and runs every destroy callback
registered for userdata, listeners, measure and paint functions.
