# Canvases

A paint callback lets a node draw with raylib directly. The node is laid out and styled as usual; the callback draws
inside its content box.

```c
{{#include ../../examples/canvas.c:paint}}
```

```c
{{#include ../../examples/canvas.c:attach}}
```

The callback runs inside `rd_draw`, after the node's background and border and before its children, so children
draw on top. raydom flushes raylib's batch before the call, with no scissor or shader active. Blending is raydom's: colors
blend as usual, and alpha accumulates toward opaque. raylib's default blends alpha like color, so a translucent
fill over an opaque pixel leaves it translucent; the desktop ignores that, but a browser composites the canvas over
the page, which would show through. A callback may change the blend mode; raydom sets its own again afterwards.

`rd_paint_info` has:

| Field     | Meaning                                                            |
| --------- | ------------------------------------------------------------------ |
| `content` | The node's content box in screen coordinates, after scrolling      |
| `clip`    | The part of the screen left visible by enclosing overflow containers |
| `dpi`     | Device pixels per logical pixel                                    |
| `ratio`   | The pixel ratio the transform is scaled by (see below)             |

`rd_paint_scissor(info)` clips raylib drawing to the content box within `clip`; raydom ends the scissor after the
callback. Restore any other raylib state you change. Rendering to a texture with `BeginTextureMode` inside the
callback is safe: size the texture and draw into it in device pixels (`content` times `dpi`), then call
`rd_paint_restore(info)` after `EndTextureMode`, which resets the transform raydom drew with. raydom restores it
again after the callback.

## Text

`rd_paint_text(ctx, info, font, size, x, baseline, utf8, len, color)` draws a line of text from the callback with
raydom's fonts, rasterized for the display's density like the rest of the UI. That keeps labels on a canvas
sharp and in the same typeface as the interface around it. `x` is the left edge and `baseline` the baseline, in
logical coordinates. To center a label in a box, measure it with `rd_font_measure` and place the baseline with
`rd_font_get_metrics`:

```c
rd_font_metrics m = rd_font_get_metrics(ctx, (rd_font){0}, 18);  // {0}: the default font
float           w = rd_font_measure(ctx, (rd_font){0}, 18, label, RD_TEXT_NUL);
rd_paint_text(ctx, info, (rd_font){0}, 18, box.x + (box.width - w) / 2,
              box.y + (box.height + m.ascent - m.descent) / 2, label, RD_TEXT_NUL, rd_hex(0xe4e7ee));
```

Text draws over whatever the callback drew before it and under what it draws after. It doesn't wrap. In C++:
`ctx.paint_text(info, label, x, baseline, size, color)`, with `ctx.measure_text` and `ctx.font_metrics`.

## Pixel ratio

`rd_set_pixel_ratio(ctx, r)` makes a logical pixel `r` raylib screen pixels: `rd_update` divides the pointer and
the screen size by `r`, and `rd_draw` scales its drawing by `r` and rasterizes text and shapes for it. raylib's web
backend renders at CSS resolution, which is blurry on high-density displays; to render at device resolution, size
raylib's window (the canvas) in device pixels, show it at CSS size, and set the ratio to `devicePixelRatio`:

```c
// Each frame, before rd_update. In CSS: canvas { width: 100vw; height: 100vh; }
double r = emscripten_get_device_pixel_ratio();
int    w = (int)lround(EM_ASM_DOUBLE(return window.innerWidth;) * r);
int    h = (int)lround(EM_ASM_DOUBLE(return window.innerHeight;) * r);
if (w != GetScreenWidth() || h != GetScreenHeight()) SetWindowSize(w, h);
rd_set_pixel_ratio(ctx, (float)r);
```

Leave out `FLAG_WINDOW_RESIZABLE`, whose resize handler sets the canvas back to CSS pixels, and
`FLAG_WINDOW_HIGHDPI`, whose scissor scaling raylib applies on the web without rendering at high density. On the
desktop, raylib's own `FLAG_WINDOW_HIGHDPI` handles density and the ratio works as a zoom. Pass logical sizes when
calling `rd_ctx_layout` yourself.

C++: `node.paint([](const rd_paint_info& info) { ... })`, with the callable owned by the node.

Pair a canvas with listeners on the same node for interaction: pointer events carry `local_x` and `local_y`
relative to the node's border box. Use the paint callback for drawing only; it must not change the tree.

The [network view](../extras/graph.md) is built on this.
