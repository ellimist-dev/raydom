# Animation

## Transitions

A node with a `transition_duration` animates its own style changes: a class added or removed, a state variant
applying, an inline style set. Each changed property moves from the value it shows to its new value.

```c
{{#include ../../examples/animation.c:transition}}
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
{{#include ../../examples/animation.c:accordion}}
```

### Retargeting

Changing a property mid-transition starts a new transition from where it is, so hovering in and out quickly never
jumps.

## Explicit animations

```c
{{#include ../../examples/animation.c:explicit}}
```

`rd_node_animate` merges `to` into the node's inline style and animates the properties it sets, from what the node
shows, over the given seconds and easing, whatever the node's transition properties say. `from` sets starting values
for some properties, which is how a node that was just created animates in. `done` runs once the time is up; it's a
[timer](../getting-started/frame-loop.md#timers), so it may destroy the node.

C++: `node.animate(to, 0.2)`, `node.animate(from, to, 0.2)`, and `node.animate(to, 0.15, RD_EASING_EASE_IN, [&] {
... })`.

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
