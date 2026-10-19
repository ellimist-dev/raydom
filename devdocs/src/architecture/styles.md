# Styles

## One list of properties

Every style property is one line of the `RD_STYLE_PROPS(X)` X-macro in `include/raydom/style.h`:

```c
X(name, type, kind, inherited, affects_layout)
```

Everything that needs a per-property case expands this list instead of naming properties:

| Expansion                    | Where              | Produces                                           |
| ---------------------------- | ------------------ | -------------------------------------------------- |
| `RD_STYLE_FIELD_`            | `style.h`          | The `rd_style` struct                              |
| `MERGE_##kind`               | `style.c`          | `rd_style_merge`: copy set fields                  |
| `EQ_##kind`, `style_diff`    | `style.c`          | Change detection, flagged inherited and layout     |
| `SET_##kind`                 | `style.c` (debug)  | `rd__style_check_defaults`: the default style sets every field |
| inherited copy in `build`    | `style.c`          | Inheritance from the parent's computed style       |
| `P_##name`, `PROPS[]`        | `anim.c`           | Property ids, offsets, sizes and kinds for transitions |
| `RD_HH_SETTER_##kind`        | `raydom.hh`        | One chainable setter per property, plus overloads per kind |
| regex over the macro         | `docs/tools/preprocess.py` | The property reference                    |

The **kind** says how a property is merged, compared and animated: `VAL` (`rd_val`), `COLOR`, `EDGES`,
`CORNERS`, `SHADOW`, `ENUM`, `TRACKS`, `TRACK`, `GRID_LINE`, `FONT`. Adding a kind means adding a case to each
expansion; adding a property of an existing kind is one line plus a default. See
[Adding a style property](../guides/property.md).

## Zero means unset

Every field type reserves zero for "unset": `RD_UNIT_UNSET`, enum value 0, `rd_color.set == false`, an
`rd_grid_line` with kind 0. That single rule gives:

- partial styles from designated initializers (`&(rd_style){.width = rd_px(10)}`),
- `rd_style_merge` that copies only what's set,
- classes and variants that layer without knowing about each other.

The cost is that "the value 0" needs a unit or a `set` flag: `rd_px(0)` is set, `{0}` is not; transparent is
`rd_rgba(0,0,0,0)` with `set = true`. `default_style` in `style.c` sets every field, which a debug-build assertion
checks at context creation, so a new property without a default fails immediately.

## Computing a style

`build(ctx, i, parent, out)`:

1. `out = default_style`.
2. Copy inherited properties from the parent's **computed** style.
3. Merge each class's base style, in the order classes were added.
4. For each state bit in precedence order (bit order of `rd_state`), merge each class's variant for that state.
   The states used are the node's own, or'ed with its parents' while `share_states` chains upward.
5. Merge the inline style.

Then `rd__transition` (see [Time](time.md)) may replace animated properties with their current values, and the
result is compared with the previous computed style.

## Dirty tracking

- `RD_FLAG_STYLE_DIRTY` on a node: its own inputs changed (inline style, classes, states, parent).
- `RD_FLAG_SUBTREE_STYLE_DIRTY` on its ancestors: something below needs computing. `rd__mark_style_dirty` stops
  climbing at the first ancestor that already has it.

`compute_rec` walks from the root, descending only into subtrees flagged dirty, and recomputes flagged nodes. The
diff decides what else happens:

- any change: paint dirty;
- a layout-affecting change: `rd__mark_layout_dirty` on the node;
- an inherited change: children recompute even if not flagged.

State changes are cheap when nothing cares: `rd_node_set_states` marks the node dirty only if one of its classes
has a variant for the changed state (and marks sharing children). Hovering over a tree of unstyled nodes recomputes
nothing.

Changing a class (`rd_class_set`, `rd_class_set_state`) scans every slot for nodes using it: O(nodes), acceptable
because classes change rarely, usually at startup.

## Storage

- Inline styles are heap-allocated per node only when set.
- Classes live in `ctx->classes` for the context's lifetime; a node keeps a small array of class ids.
- Track lists are interned by content into an append-only pool: `rd_tracks_create` returns the existing handle for an
  identical list (including its repeat mode), so rebuilding lists is free. A style holds the handle by value, which is
  why lists can't be freed or reference counted: raydom can't see the copies in inline styles, classes and variants.

## Computed style access

`rd_node_computed_style` computes pending styles first, then returns a pointer into `ctx->styles`, valid until the
next node creation (which may reallocate the array). Internally, code reads `ctx->styles[i].computed` directly and
must call `rd_ctx_compute_styles` first when it can run outside layout: `rd_ctx_focus_next` and `rd_node_focus` do,
because a node created this frame has no computed `focusable` yet.
