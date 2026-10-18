# Values and units

## Lengths: `rd_val`

| Constructor          | C++               | Meaning                                    |
| -------------------- | ----------------- | ------------------------------------------ |
| `rd_px(v)`           | `v_px`            | Logical pixels                             |
| `rd_pct(v)`          | `v_pct`           | Percent of the containing block            |
| `rd_num(v)`          | `rd::num(v)`      | Unitless: flex factors, opacity, line height multiple, line clamp |
| `rd_fr(v)`           | `v_fr`            | Grid fraction of free space                |
| `rd_auto()`          | `rd::auto_`       | Automatic                                  |
| `rd_min_content()`   | `rd::min_content` | Grid tracks: smallest size without overflow |
| `rd_max_content()`   | `rd::max_content` | Grid tracks: size without wrapping          |

Percentages of width resolve against the containing block's width. Percentages of height resolve against its height
when that is definite, and are treated as `auto` otherwise. Padding and margin percentages resolve against the
width on every side, as in CSS.

Logical pixels are scaled by raylib's DPI scale when drawn; layout never sees device pixels.

## Colors: `rd_color`

`rd_hex(0xRRGGBB)` (opaque), `rd_rgb(r, g, b)`, `rd_rgba(r, g, b, a)`. A zero `rd_color` is unset. Transparent is
`rd_rgba(0, 0, 0, 0)`, which is set.

## Edges and corners

`rd_edges` is `{top, right, bottom, left}`; `rd_edges_all(v)` and `rd_edges_vh(vertical, horizontal)` build it.
`rd_corners` is `{top_left, top_right, bottom_right, bottom_left}`; `rd_corners_all(v)` builds it. Each side is an
independent `rd_val`, so `.padding.left = rd_px(4)` sets one side and leaves the others unset.

## Shadows

`rd_shadow` is `{x, y, blur, spread, color}`. `rd_shadow_of(x, y, blur, color)` builds one with zero spread.

## Grid

| Constructor                | Meaning                                       |
| -------------------------- | --------------------------------------------- |
| `rd_track_of(v)`           | A track sized `v`; `fr` becomes `minmax(auto, fr)` |
| `rd_track_minmax(min, max)`| A track between two sizes                     |
| `rd_tracks_create(ctx, a, n)` | Interns a track list; returns a handle     |
| `rd_tracks_none()`         | No explicit tracks                            |
| `rd_line(n)`               | A grid line, 1-based; negative from the end   |
| `rd_span(n)`               | Span `n` tracks                               |
| `rd_line_auto()`           | Auto placement                                |

## Unset

Zero means unset for every style type. That is what makes `&(rd_style){.width = rd_px(10)}` set only the width,
and it is why `rd_node_merge_style` can tell which fields to copy.
