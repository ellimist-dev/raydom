# Layout

## The measure contract

All layout goes through one function:

```c
rd_size rd__layout_node(rd_ctx* ctx, uint32_t i, rd_layout_key k, bool perform);
```

`rd_layout_key` is the question asked of a node: available width and height, each with a mode, and the containing
block size for percentages.

| Mode          | Meaning                                                          |
| ------------- | ---------------------------------------------------------------- |
| `EXACTLY`     | The border box is this size                                      |
| `AT_MOST`     | Fit content, up to this size                                     |
| `UNDEFINED`   | Max-content: size freely (available size is NaN)                 |
| `MIN_CONTENT` | The smallest size without overflow                               |

With `perform` false it's a measurement and returns a size; with `perform` true it lays out the subtree and sets
child rects. A node's own `width`, `height`, `min_*` and `max_*` are resolved by its **parent** (or the absolute or
anchoring code) into the key; `rd__layout_node` itself answers for the content. Forgetting this in a new caller is
the classic bug: a fixed-height node measured with `AT_MOST` reports its content height. Copy the resolution from
`rd__layout_absolute`.

This is Yoga's model: one recursive function, intrinsic sizes on demand, no separate pre-pass.

## Caches

Each node keeps one **layout** entry (the last `perform`) and four **measure** entries (round robin), each keyed by
`rd_layout_key`:

- A hit returns the stored size without recursion. Flex and grid ask the same questions repeatedly (measure for
  basis, measure for cross size, then perform), so this is what keeps layout near-linear.
- `normalize_key` drops the containing block from the key when the node has no percentage padding or min/max, and
  sets available sizes to NaN for `UNDEFINED`/`MIN_CONTENT`, so equivalent questions share an entry.
- `RD_FLAG_LAYOUT_DIRTY` invalidates a node's entries the next time it is asked anything.
  `rd__mark_layout_dirty` marks the node and **every ancestor**, without stopping early: subtrees layout never
  visits (display none, measured leaves, detached) keep stale flags, so an already-dirty node doesn't prove its
  ancestors are.

The cost model: an unchanged frame is one `perform` call on the root that hits the cache. A change re-lays out the
path to the root and whatever the changed sizes affect.

## Flexbox

`layout_flex` follows CSS Flexbox §9: collect items (skipping absolute and `display: none`), resolve flex basis
(measuring content when `auto`), break into lines when wrapping, resolve flexible lengths (§9.7, with min/max
freezing), determine cross sizes and stretch, then justify and align. Differences from CSS are deliberate and
documented for users: `flex_direction` defaults to column, `flex_shrink` to 0, and automatic minimum sizes are 0.

## Grid

`grid.c` follows CSS Grid: placement (§8.5, sparse auto-placement over an occupancy map), then track sizing (§11):
intrinsic contributions (§11.5) and fr expansion (§11.7), each axis sized against the other's result. An indefinite
axis is sized to content, clamped, and re-run at the clamped size when that differs. Named lines, named areas and
dense packing aren't implemented.

**Auto repetition** (§7.2.3.2): a list created with `rd_tracks_create_auto` is expanded per layout. `repetitions`
computes the count from the content box (definite size, else max size, else the min size to reach, else one), with
each track at its max size when that's a length, else its min size. The expanded list replaces the explicit list
before placement, so line numbers (including negative ones) refer to it. For auto-fit, `collapse_empty` removes the
repeated tracks no item occupies after placement and renumbers later items. Removal is equivalent to CSS's
collapsing (zero size and merged gaps) because nothing spans those tracks, and it leaves track sizing untouched.

## Text and measured leaves

A node with a measure callback is a leaf: `layout_leaf` calls `measure_fn(data, w, wmode, h, hmode)` with content
box sizes. Text nodes, inputs and image nodes are measured leaves whose callbacks live in `text.c` and `image.c`;
`rd__is_text` and `rd__node_image` identify them by comparing the callback pointer, so there's no type field to
keep in sync.

## Absolute positioning

`rd__layout_absolute` positions an absolute child against its parent's padding box after the flow pass, resolving
`inset`, margins and the node's own size (stretching between opposite insets when no size is set).

## The overlay

`rd_ctx_overlay` creates a node that is the root's last child. Its geometry is fixed during **style computation**:
`build` forces position absolute, zero inset and margin, and auto sizes, whatever classes or inline styles say. So
the overlay always covers the root, and layout needs no special case. Hit-testing skips the overlay node itself so
it doesn't block the page; its children are hit-tested first because it's the last child.

## Anchoring

`rd__place_anchored` runs after the main pass. For each node with an anchor, in the order they were anchored, it
reads the anchor's bounds from this layout, chooses a side (flipping when only the other side fits), aligns,
shifts into the root, and writes the node's rect relative to its parent. `match_width` re-lays out the node at the
anchor's width, resolving the node's own height and bounds the way absolute layout does.

Placing after layout means anchoring never affects other nodes' sizes, and a submenu anchored to an item of an
anchored menu works because the menu was placed first.

## Layout hooks and the second pass

Hooks are context-wide (`rd_ctx_on_layout`) or node-scoped (`rd_node_on_layout`). A node-scoped hook sets
`RD_FLAG_HAS_HOOK` on its slot; freeing the slot drops its hooks. A hook can destroy its own node while hooks are
running, so dropping only marks the hook dead, and `sweep_hooks` runs destroy callbacks and compacts the list after
the run. The loop copies each hook before calling it because a hook may add hooks, reallocating the array. Hooks
whose node is detached are skipped.

`rd_ctx_layout` runs the hooks after placing anchors. If any hook left the root dirty (layout or style), it runs
one more pass. Hooks don't run on that pass, which bounds the work at two passes per frame. Virtualized lists bind
rows here; a hook that would need a third pass is doing something layout should do.

## Scrolling

Scroll offsets live in `rd_node_layout.scroll` and are clamped to the content (`rd_node_content_size`: the padding
box plus children's margin boxes) during layout and whenever set. Offsets don't dirty layout: drawing and
hit-testing subtract them, so scrolling is cheap.

## Natural height

`rd__natural_height` measures a node as if `height` were auto at its current width, for transitions to
`height: auto`. It temporarily changes the computed height, measures, restores it, and marks the node dirty,
because the measurement went into the caches under a key that doesn't include style.
