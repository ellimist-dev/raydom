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

`rd_update` does these things, in order:

1. Sets the context clock from `GetTime()` (`rd_ctx_set_time`), which fires due timers.
2. Feeds raylib's keyboard and text input (`rd_ctx_key`, `rd_ctx_text`).
3. Feeds the mouse position, buttons and wheel (`rd_ctx_pointer`). Listeners run here.
4. Lays the tree out at the screen size (`rd_ctx_layout`): advances animations, computes styles, lays out, and places
   anchored nodes.
5. Applies the mouse cursor, and, with `rd_set_event_waiting`, raylib's event waiting.

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

## Timers

Timers run on the context's clock. `rd_update` sets the clock from raylib each frame, before input, and every due
timer fires then, on the main thread, so callbacks may change the tree like any listener.

```c
static void hide_toast(rd_ctx* ctx, void* data) {
  toast* t = data;
  rd_node_merge_style(ctx, t->node, &(rd_style){.display = RD_DISPLAY_NONE});
}

static void show_toast(rd_ctx* ctx, toast* t) {
  rd_node_merge_style(ctx, t->node, &(rd_style){.display = RD_DISPLAY_FLEX});
  rd_ctx_clear_timer(ctx, t->hide);  // showing again restarts the countdown
  t->hide = rd_ctx_set_timer(ctx, 2.0, 0, hide_toast, t, NULL);
}
```

```c
int      seconds = 0;
rd_timer clock   = rd_ctx_set_timer(ctx, 1.0, RD_TIMER_REPEAT, tick, &seconds, NULL);
// ... later
rd_ctx_clear_timer(ctx, clock);
```

A one-shot timer is destroyed after it fires; clearing it afterwards is a harmless no-op, so "clear, then set" is
the restart idiom. A repeating timer that falls behind (the window was minimized, say) fires once and skips the rest.
A timer set from a timer callback doesn't fire until the next clock update, so callbacks can't loop forever within
one frame. `destroy(data)` runs when a timer is cleared, a one-shot has fired, or the context is destroyed.

C++: `ctx.set_timeout(2.0, [&] { ... })` and `ctx.set_interval(1.0, [&] { ... })` return an `rd_timer` for
`ctx.clear_timer`.

### Event waiting

raylib's `EnableEventWaiting` makes idle frames block until input arrives, which saves power but would also stall
timers. Let raydom manage it instead:

```c
rd_set_event_waiting(ctx, true);  // instead of EnableEventWaiting()
```

`rd_update` then turns raylib's event waiting off while any timer is pending or anything animates, and back on when
neither is true, so a
tooltip delay or toast fires on time and an idle UI still costs nothing. A repeating timer keeps frames running for as
long as it exists.

## Lifetime

Destroy the context before `CloseWindow`. `rd_ctx_destroy` frees every node and runs every destroy callback
registered for userdata, listeners, measure and paint functions.
