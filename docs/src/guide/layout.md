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
implicit tracks sized by `grid_auto_rows` and `grid_auto_columns`.

The algorithm follows [CSS Grid §11](https://www.w3.org/TR/css-grid-1/#layout-algorithm). Named lines, named areas
and `dense` packing are not implemented.

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
no size, it stretches between them. It takes no space in the parent's
flex or grid layout.

## Scrolling

```c
{{#include ../../examples/layout.c:scroll}}
```

`overflow_x` and `overflow_y` set to `RD_OVERFLOW_HIDDEN` clip children; `RD_OVERFLOW_SCROLL` also makes the node
scroll with the wheel. The wheel scrolls the nearest scroll container under the pointer that can move in the gesture's
direction; once a gesture starts, it stays with that container until the wheel pauses.

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
