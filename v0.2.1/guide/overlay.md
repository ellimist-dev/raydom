# Overlay and anchoring

Menus, popovers, tooltips and modals have to draw above everything else and must not be clipped by the scroll
container their trigger sits in. raydom provides two primitives for them: the overlay layer, and anchored
positioning.

## The overlay

```c
rd_node menu = rd_node_create(ctx);
rd_node_set_style(ctx, menu, &(rd_style){.position = RD_POSITION_ABSOLUTE, .width = rd_px(160), .height = rd_px(90)});
rd_node_append(ctx, rd_ctx_overlay(ctx), menu);
rd_node_set_anchor(ctx, menu, &(rd_anchor){.node = button, .offset = 4});  // below the button
```

`rd_ctx_overlay` returns the overlay node, creating it on first use. It is a child of the root with special rules:

- **Always last.** It stays the root's last child: appending to the root inserts before it. So it draws after,
  and is hit-tested before, everything else.
- **Never clipped.** It always covers the root, and no other node is its ancestor, so overflow containers don't
  clip what's inside it. Its position, inset, margin and size can't be changed by styles; padding, background and
  layout properties can.
- **Transparent to input.** The overlay node itself is never a hit target, so it doesn't block the page under it.
  Only its children take input.

Overlay children are ordinary nodes. Give them `position: absolute`, or they stack in the overlay's flex layout.
Events from them bubble through the overlay to the root, and they're in Tab order after the page.

## Anchoring

`rd_node_set_anchor` positions a node against another node, or a point, after every layout:

```c
rd_node_set_anchor(ctx, menu, &(rd_anchor){.node = button, .side = RD_SIDE_RIGHT, .align = RD_ALIGN_CENTER});
rd_node_set_anchor(ctx, menu, &(rd_anchor){.x = 300, .y = 200});  // at a point: a context menu
rd_node_set_anchor(ctx, menu, &(rd_anchor){.node = button, .match_width = true});  // a dropdown
```

| Field         | Meaning                                                              |
| ------------- | -------------------------------------------------------------------- |
| `node`        | The anchor node; null to use `x`, `y` in root coordinates            |
| `side`        | `BOTTOM` (zero), `TOP`, `RIGHT` or `LEFT` of the anchor              |
| `align`       | `START` (zero), `CENTER` or `END` along that side                    |
| `offset`      | Gap between the anchor and the node                                  |
| `no_flip`     | Keep `side` even when the node only fits on the opposite side        |
| `no_shift`    | Don't slide the node to keep it inside the root                      |
| `match_width` | Lay the node out at the anchor's width                               |

A zero-initialized `rd_anchor` places the node below its anchor, start-aligned, flipping to the other side when it
doesn't fit and shifting to stay inside the window:

```c
rd_node_merge_style(ctx, button, &(rd_style){.margin.top = rd_px(560)});
rd_ctx_layout(ctx, 800, 600);
rd_side side = rd_node_anchor_side(ctx, menu);  // RD_SIDE_TOP: no room below, so it flipped
```

`rd_node_anchor_side` reports the side actually used, for drawing an arrow that points at the anchor.

The anchor is followed every layout, including when it scrolls. Anchored nodes are placed in the order they were
anchored, so a submenu anchored to an item of an anchored menu lands next to the item's final position. If the
anchor is destroyed or detached, the node stays where it was last placed.

## Opening and closing

The overlay has no notion of open or closed. Show and hide popups with `display`, or create and destroy them:

```c
rd_node_set_style(ctx, menu, &(rd_style){.position = RD_POSITION_ABSOLUTE, .display = RD_DISPLAY_NONE});  // close
rd_node_destroy(ctx, menu);                                                                           // or remove
```

Light dismiss, closing when the user presses outside, is a capture listener on the root:

```cpp
ctx.root().on(RD_EVENT_POINTER_DOWN, [&](rd::Event& e) {
  if (!popover.contains(e.target()) && !toggle.contains(e.target())) hide(popover);
}, /*capture=*/true);
```
