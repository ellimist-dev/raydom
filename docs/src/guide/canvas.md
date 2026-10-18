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
