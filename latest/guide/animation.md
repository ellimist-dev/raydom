# Animation

## Transitions

A node with a `transition_duration` animates its own style changes: a class added or removed, a state variant
applying, an inline style set. Each changed property moves from the value it shows to its new value.

```c
rd_class card = rd_class_create(ctx, &(rd_style){.width               = rd_px(200),
                                                 .background          = rd_hex(0x1a1f2b),
                                                 .transition_duration = rd_ms(200),
                                                 .transition_easing   = RD_EASING_EASE_OUT});
rd_class_set_state(ctx, card, RD_STATE_HOVER,
                   &(rd_style){.width = rd_px(240), .background = rd_hex(0x252c3d)});  // both animate
```

| Property                | Meaning                                                              |
| ----------------------- | -------------------------------------------------------------------- |
| `transition_duration`   | Seconds: `rd_ms(150)`, `rd_s(0.2)`; C++ `150_ms`. 0, the default, changes at once |
| `transition_delay`      | Seconds before the change starts animating                           |
| `transition_easing`     | `LINEAR`, `EASE` (default), `EASE_IN`, `EASE_OUT`, `EASE_IN_OUT`: the CSS curves |
| `transition_properties` | A mask of groups: `COLOR`, `OPACITY`, `SHADOW`, `RADIUS`, `SIZE`, `SPACING`, `INSET`, `TEXT`; all by default |

Put the transition properties on the base style of a class, so they apply in every state. A variant that sets a
different duration applies it to changes made while the variant is active.

### What animates

- **Lengths** (sizes, spacing, insets, gaps, radii, font size, line height, flex factors) when both values have the
  same unit. px to px animates; px to percent, or anything to `auto`, changes at once.
- **Height to and from `auto`.** Collapsing from `auto` starts at the laid-out height. Expanding to `auto` animates
  to the content's height, measured every frame, and ends as `auto`, so the node keeps following its content.
- **Colors**, in premultiplied alpha, so fading to or from transparent doesn't pass through black.
- **Edges, corners and shadows**, side by side.

Everything else (`display`, `flex_direction`, fonts, enumerations) changes at once. A node's first style never
animates, nor does a change while the node has `display: none`.

### Layout

Layout properties animate through layout: every frame of a width or margin transition lays the tree out again, so
siblings and parents move with it, and hit-testing, anchoring and scrolling all follow. Layout is cached per node,
so only the animating node's ancestors and the siblings it moves are recomputed.

To move something without moving its neighbors, animate `inset` on a node with the default `position: relative`:
it's offset from where it was laid out, and nothing else reflows.

### Accordions

```c
rd_node panel = rd_node_create(ctx);
rd_node_set_style(ctx, panel, &(rd_style){.height              = rd_px(0),  // closed
                                          .overflow_y          = RD_OVERFLOW_HIDDEN,
                                          .transition_duration = rd_ms(250)});
// open: animates to the content's height, then stays auto as the content changes
rd_node_merge_style(ctx, panel, &(rd_style){.height = rd_auto()});
```

### Retargeting

Changing a property mid-transition starts a new transition from where it is, so hovering in and out quickly never
jumps.

## Explicit animations

```c
static rd_node toast;
toast = rd_node_create(ctx);
rd_node_append(ctx, rd_ctx_root(ctx), toast);
// In: from transparent and 24px lower, to its place.
rd_node_animate(ctx, toast, &(rd_style){.opacity = rd_num(0), .margin.top = rd_px(24)},
                &(rd_style){.opacity = rd_num(1), .margin.top = rd_px(0)}, 0.2, RD_EASING_EASE_OUT);
// ... later, out: fade, then remove it when the animation ends, with a timer for the same time.
rd_node_animate(ctx, toast, NULL, &(rd_style){.opacity = rd_num(0)}, 0.15, RD_EASING_EASE_IN);
rd_node* doomed = malloc(sizeof *doomed);
*doomed         = toast;
rd_ctx_set_timer(ctx, 0.15, 0, remove_node, doomed, free);  // freed when it fires, or if cleared
```

`rd_node_animate` merges `to` into the node's inline style and animates the properties it sets, from what the node
shows, over the given seconds and easing, whatever the node's transition properties say. `from` sets starting values
for some properties, which is how a node that was just created animates in. Several calls before the next frame add
up, each property keeping its own duration and easing. To act when the animation ends, for example to destroy a node
it faded out, set a [timer](../getting-started/frame-loop.md#timers) for the same seconds: it runs on the same
clock, can be cleared if the node is shown again, and frees its data with its destroy function.

C++: `node.animate(to, 0.2)` and `node.animate(from, to, 0.2)` return false if nothing could start; for the end, `ctx.set_timeout(0.2, [=] { ... })` returns the `rd_timer` to clear.

## Clock and frames

Animations advance in `rd_ctx_layout`, on the context's clock (`rd_ctx_set_time`, which `rd_update` sets from
raylib); `rd_ctx_time` reads it. `rd_ctx_animating` is true while anything moves. With
[`rd_set_event_waiting`](../getting-started/frame-loop.md#event-waiting), frames keep running while anything
animates and wait for input once everything is still.

For motion you compute yourself, such as a canvas drawing an algorithm's state, run a repeating timer while it
moves and advance by the difference in `rd_ctx_time` between calls. The timer keeps frames coming, and the motion
stays on the same clock as everything else, so a test can drive it with `rd_ctx_set_time`.

Animating layout costs one layout pass per frame for the animating parts of the tree, which is the same cost as
resizing the window.
