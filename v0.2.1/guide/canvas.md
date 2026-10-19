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
draw on top. raydom flushes raylib's batch before the call, with no scissor or shader active.

`rd_paint_info` has:

| Field     | Meaning                                                            |
| --------- | ------------------------------------------------------------------ |
| `content` | The node's content box in screen coordinates, after scrolling      |
| `clip`    | The part of the screen left visible by enclosing overflow containers |
| `dpi`     | Device pixels per logical pixel                                    |

`rd_paint_scissor(info)` clips raylib drawing to the content box within `clip`; raydom ends the scissor after the
callback. Restore any other raylib state you change. Rendering to a texture with `BeginTextureMode` inside the
callback is safe.

C++: `node.paint([](const rd_paint_info& info) { ... })`, with the callable owned by the node.

Pair a canvas with listeners on the same node for interaction: pointer events carry `local_x` and `local_y`
relative to the node's border box. Use the paint callback for drawing only; it must not change the tree.

The [network view](../extras/graph.md) is built on this.
