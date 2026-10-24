# Canvases

A paint callback lets a node draw with raylib directly. The node is laid out and styled as usual; the callback draws
inside its content box.

```c
typedef struct chart {
  float values[5];
} chart;

static void paint_chart(rd_ctx* ctx, const rd_paint_info* info, void* data) {
  (void)ctx;
  const chart* c = data;
  rd_rect      r = info->content;  // screen coordinates, already scrolled
  rd_paint_scissor(info);          // clip to the visible part of the node
  float bar = r.width / 5;
  for (int i = 0; i < 5; i++) {
    float h = r.height * c->values[i];
    DrawRectangleRec((Rectangle){r.x + bar * (float)i + 4, r.y + r.height - h, bar - 8, h}, SKYBLUE);
  }
}
```

```c
static chart data = {{0.2f, 0.5f, 0.9f, 0.4f, 0.7f}};
rd_node      plot = rd_node_create(ctx);
rd_node_set_style(ctx, plot, &(rd_style){.height = rd_px(240), .margin = rd_edges_all(rd_px(20))});
rd_node_set_paint(ctx, plot, paint_chart, &data, NULL);  // NULL: nothing to free when the node goes away
rd_node_append(ctx, rd_ctx_root(ctx), plot);
```

The callback runs inside `rd_draw`, after the node's background and border and before its children, so children
draw on top. raydom flushes raylib's batch before the call, with no scissor or shader active. Blending is raydom's: colors
blend as usual, and alpha accumulates toward opaque. raylib's default blends alpha like color, so a translucent
fill over an opaque pixel leaves it translucent; the desktop ignores that, but a browser composites the canvas over
the page, which would show through. A callback may change the blend mode; raydom sets its own again afterwards.
raydom sets its blending directly in GL rather than through rlgl, and when `rd_draw` returns, GL's blend factors and
equations are as the app left them, as are rlgl's mode and custom factors (`rlSetBlendFactorsSeparate`): blending
set with `BeginBlendMode` before `rd_draw` still applies after it. One consequence inside a callback: rlgl still
believes the app's mode is current, so `BeginBlendMode` with that same mode does nothing; another mode applies as
usual. On a GL without these entry points, raydom leaves `BLEND_ALPHA`, as `EndBlendMode` does. rlgl doesn't
expose the active shader, so `rd_draw` returns with raylib's default shader active; begin a custom shader after it.

A callback may replace or clear its own node's painter (`rd_node_set_paint`), or destroy the node: the old data's
destroy function runs once the callback returns, so the callback can still use its data. In C++, a lambda can call
`node.paint(...)` or `node.destroy()` on its own node and keep using its captures.

A context may be drawn from another's paint callback with `rd_draw`. It draws under the transform it finds,
including the outer context's pixel ratio, and clips and rasterizes for that transform; the same holds under a
raylib camera (`BeginMode2D`).

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
`rd_paint_restore(info)` after `EndTextureMode`, which resets the transform raydom drew with, whatever the pixel
ratio. raydom restores it again after the callback.

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

Text draws over whatever the callback drew before it and under what it draws after, with the shader the callback
has active, as raylib's `DrawText` does; it changes no state. It doesn't wrap. In C++:
`ctx.paint_text(info, size, x, baseline, label, color, font)`, in the C function's order with the font last and
optional, with `ctx.measure_text` and `ctx.font_metrics`.

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

C++: `node.paint([](const rd_paint_info& info) { ... })`, with the callable owned by the node, and
`node.clear_paint()` to remove it.

Pair a canvas with listeners on the same node for interaction: pointer events carry `local_x` and `local_y`
relative to the node's border box. Use the paint callback for drawing only; it must not change the tree.

The [network view](../extras/graph.md) is built on this.
