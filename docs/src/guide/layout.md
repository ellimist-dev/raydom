# Layout

Every node is a flex container by default, with `flex_direction: column`, `align_items: stretch`, and
`flex_shrink: 0`. Children stack vertically and fill the width. Set `display` to `RD_DISPLAY_GRID` for grid, or
`RD_DISPLAY_NONE` to remove a node from layout, drawing and hit-testing.

Sizes are border-box: `width` includes padding and border.

## Flexbox

```c
{{#include ../../examples/layout.c:flex}}
```

The algorithm follows [CSS Flexible Box Layout §9](https://www.w3.org/TR/css-flexbox-1/#layout-algorithm), with
these differences:

- `flex_shrink` defaults to 0. Items keep their size and overflow unless they opt in to shrinking.
- `flex_direction` defaults to column.
- `min_width` and `min_height` `auto` resolve to 0, not to the content size. An item is never smaller than its
  padding plus border.

`flex_basis: 0` with `flex_grow: 1` splits free space without regard to content, which is the common "fill the rest"
pattern.

Shrinking follows CSS: each item gives up overflow in proportion to `flex_shrink` times its base size less its
padding and border. Two items with a 100px basis in a 100px row split the overflow evenly, unless one has 20px of
padding on each side, in which case it shrinks by 37.5px and the other by 62.5px.

## Grid

```c
{{#include ../../examples/layout.c:grid}}
```

Track lists are interned: `rd_tracks_create` copies the array into the context and returns a handle that styles
refer to. The list lives until the context is destroyed. C++ builds them with
`ctx.tracks({200_px, 1_fr, 2_fr})` and `rd::repeat(3, {1_fr})`.

A track is `{min, max}`. `rd_track_of(v)` uses `v` for both, except `fr`, which becomes `minmax(auto, fr)` as in
CSS. `rd_track_minmax` builds any other pair.

Placement uses `grid_{column,row}_{start,end}`: `rd_line(n)` is a 1-based line, negative from the end of the
explicit grid; `rd_span(n)` spans tracks. Items without placement are auto-placed in `grid_auto_flow` order into
implicit tracks sized by `grid_auto_rows` and `grid_auto_columns`. Items with only a row (or, in column flow, only a
column) go there first, each after the ones before it in the same row, adding implicit columns when the row is full.

The algorithm follows [CSS Grid §11](https://www.w3.org/TR/css-grid-1/#layout-algorithm). Named lines, named areas
and `dense` packing are not implemented.

### Auto-repeated tracks

```c
{{#include ../../examples/layout.c:cards}}
```

`rd_tracks_create_auto` is CSS's `repeat(auto-fill, ...)` and `repeat(auto-fit, ...)`: the list repeats as many times
as fits the grid's content width, so the column count follows the window. Each track counts at its max size when
that's a length, else its min size, which is why `minmax(260px, 1fr)` means "at least 260px". Without a definite
width, the grid's max width decides, then its min width; otherwise the list appears once.

- `RD_REPEAT_AUTO_FILL` keeps empty repetitions: three cards in a four-column row leave a gap.
- `RD_REPEAT_AUTO_FIT` drops repetitions no item occupies, so the cards stretch to fill the row.

The whole list repeats; fixed tracks before or after a repeat aren't supported. C++:
`ctx.tracks(rd::repeat(rd::auto_fill, {rd::minmax(260_px, 1_fr)}))`.

Track lists are interned: creating a list identical to an existing one returns the same handle, so code that
rebuilds lists doesn't grow memory.

## Running layout and reading geometry

```c
{{#include ../../examples/layout.c:run}}
```

| Function               | Returns                                                        |
| ---------------------- | -------------------------------------------------------------- |
| `rd_node_rect`         | Border box relative to the parent's border box, before scroll |
| `rd_node_bounds`       | Border box in root coordinates, after scroll offsets           |
| `rd_node_box`          | Resolved margin, border and padding insets                     |
| `rd_node_content_size` | Extent of the padding box and children's margin boxes          |
| `rd_node_baseline`     | First baseline from the top of the border box, or NaN          |

Layout results are cached per node and reused when neither the node nor its inputs changed.

## Absolute positioning

```c
{{#include ../../examples/layout.c:absolute}}
```

An absolute node is positioned against its parent's padding box using `inset`. With both insets on an axis and
no size, it stretches between them. With both insets and a size, `auto` margins share the space left over, so
`inset: 0` with a width, a height and `margin: auto` centers it. It takes no space in the parent's flex or grid
layout.

## Scrolling

```c
{{#include ../../examples/layout.c:scroll}}
```

`overflow_x` and `overflow_y` set to `RD_OVERFLOW_HIDDEN` clip children; `RD_OVERFLOW_SCROLL` also makes the node
scroll with the wheel. The wheel scrolls the nearest scroll container under the pointer that can move in the gesture's
direction; once a gesture starts, it stays with that container until the wheel pauses.

A `SCROLL` axis whose content overflows shows a scrollbar. Bars draw over the content's edge, inside the padding
box, and take no layout space, so content doesn't reflow when one appears. Dragging the thumb scrolls; pressing the
track pages toward the pointer. Presses on a bar never reach the content under it.

```c
rd_node_merge_style(ctx, list, &(rd_style){
    .scrollbar_width = rd_px(6),                     // 0px hides the bars; scrolling still works
    .scrollbar_color = rd_rgba(255, 255, 255, 60),   // inherited, so set it once on the root for a theme
});
```

Leave room with `padding` if content shouldn't sit under the bar.

## Layout hooks

Layout hooks run after every layout, with that layout's geometry and scroll offsets. They're for content that
depends on its own geometry: a virtualized list binding the rows that scrolled into view, a chat log staying
scrolled to its end. If hooks change the tree, layout runs once more before `rd_ctx_layout` returns, so the change
shows in the same frame; hooks don't run for that second pass.

`rd_node_on_layout(ctx, node, fn, data, destroy)` ties a hook to a node: destroying the node removes the hook and
runs `destroy(data)`, and the hook is skipped while the node is detached. That's the right lifetime for a hook that
belongs to a view, and it can't leak. C++: `node.on_layout([&] { ... })`. `rd_ctx_on_layout` adds a hook for the
context's whole lifetime.

```c
static void stick_to_bottom(rd_ctx* ctx, void* data) {
  rd_node log = *(rd_node*)data;
  rd_node_set_scroll(ctx, log, 0, 1e9f);  // clamped to the content just laid out
}

static rd_node log_node;  // the hook's data must live as long as the hook
rd_node_on_layout(ctx, log_node, stick_to_bottom, &log_node, NULL);  // removed with the log
```

## Custom measurement

A leaf node can report its own size with `rd_node_set_measure`. The callback receives a width and height, each with
a mode:

| Mode                     | Meaning                                       |
| ------------------------ | --------------------------------------------- |
| `RD_MEASURE_EXACTLY`     | The size is fixed; return it                  |
| `RD_MEASURE_AT_MOST`     | Return any size up to the value               |
| `RD_MEASURE_UNDEFINED`   | Size freely (max-content); the value is NaN   |
| `RD_MEASURE_MIN_CONTENT` | Return the smallest size without overflow; NaN |

Sizes are content-box. The callback must not change the tree. When the content it measures changes, call
`rd_node_mark_layout_dirty`.
