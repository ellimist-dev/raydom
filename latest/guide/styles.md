# Styles and classes

`rd_style` is a struct with one field per property. Every field type reserves zero for "unset", so a
compound literal names only the properties it sets. The [property reference](../reference/properties.md) lists them
all.

## Inline styles

```c
rd_node_set_style(ctx, node,
                  &(rd_style){
                      .width         = rd_px(240),
                      .padding       = rd_edges_vh(rd_px(8), rd_px(12)),  // vertical, horizontal
                      .border_width  = rd_edges_all(rd_px(1)),
                      .border_color  = rd_hex(0x272d3a),
                      .border_radius = rd_corners_all(rd_px(6)),
                      .background    = rd_hex(0x1a1f2b),
                  });
rd_node_merge_style(ctx, node, &(rd_style){.padding.left = rd_px(20)});  // only the left side changes
```

`rd_node_set_style` replaces the inline style. `rd_node_merge_style` copies the set fields over it and leaves the
rest. Passing `NULL` to `rd_node_set_style` clears it.

Value constructors: `rd_px`, `rd_pct`, `rd_num`, `rd_fr`, `rd_auto()`, `rd_min_content()`, `rd_max_content()`.
Colors: `rd_hex(0xRRGGBB)`, `rd_rgb`, `rd_rgba`. Box sides: `rd_edges_all`, `rd_edges_vh`, or designated
`.padding.left`. See [Values and units](../reference/values.md).

## Classes

A class is a reusable style with optional variants per state. Classes live until the context is destroyed;
`rd_class_set` replaces a class's base style and restyles every node that uses it. `rd_class_merge` (and
`rd_class_merge_state` for a variant) changes only the properties set in its style, as `rd_node_merge_style` does for
a node: recolor a class without restating its layout. `rd_class_get` and `rd_class_get_state` copy a class's styles
out. In C++: `cls.merge(style)`, `cls.merge_on(state, style)`, `cls.base()` and `cls.state(state)`.

```c
rd_class button_style = rd_class_create(ctx, &(rd_style){.padding    = rd_edges_vh(rd_px(6), rd_px(14)),
                                                        .background = rd_hex(0x1d2230)});
rd_class primary      = rd_class_create(ctx, &(rd_style){.background = rd_hex(0x5b8cff)});
rd_class_set_state(ctx, button_style, RD_STATE_HOVER, &(rd_style){.background = rd_hex(0x252c3d)});
rd_class_set_state(ctx, button_style, RD_STATE_DISABLED, &(rd_style){.opacity = rd_num(0.4f)});

rd_node ok = rd_node_create(ctx);
rd_node_append(ctx, root, ok);
rd_node_add_class(ctx, ok, button_style);
rd_node_add_class(ctx, ok, primary);  // later classes win: the background is primary's
```

```c
const rd_style* s = rd_node_computed_style(ctx, ok);
// background from primary (the later class), padding from button_style (primary doesn't set it)
```

## Precedence

For each property, the last of these that sets it wins:

1. Defaults (the reference's Default column).
2. The parent's computed value, for inherited properties.
3. Class base styles, in the order the classes were added.
4. Class state variants for the node's current states, ordered by state: `hover`, `checked`, `focus_within`,
   `focus`, `focus_visible`, `active`, `disabled`. Within one state, later classes win.
5. The inline style.

Inline styles beat every class and every state variant. Put anything that should change on hover in a class, not
inline.

## States

raydom sets `hover`, `active`, `focus`, `focus_visible` and `focus_within` from input. `checked` and `disabled` are
set by the application, or by a control such as an extras checkbox.

```c
rd_node_set_states(ctx, ok, RD_STATE_HOVER, true);
// the hover variant of button_style now beats primary's base background
```

A disabled node and its descendants receive no pointer down, up or click events and can't be focused. Hover and
pointer move still apply.

`focus_visible` is set when focus came from the keyboard, or when the focused node is a text input. Use it, not
`focus`, for focus rings.

## Shared states

A control is usually several nodes: a checkbox is a row holding a box and a label. Its box should look hovered while
the pointer is anywhere on the row, like CSS's `.row:hover .box`. `rd_node_set_share_states` does that: the node's
class variants also apply for its parent's states, and the sharing chains through parents that share too.

```c
rd_node_set_share_states(ctx, box, true);   // box variants see the row's hover, checked, focus...
rd_node_set_share_states(ctx, mark, true);  // and the mark inside the box, through it
rd_node_set_states(ctx, row, RD_STATE_CHECKED, true);
```

Only class variants see shared states; `rd_node_states` still returns the node's own.

## Inheritance

```c
rd_node child = rd_node_create(ctx);
rd_node_append(ctx, node, child);
rd_node_merge_style(ctx, node, &(rd_style){.color = rd_hex(0xe4e7ee), .font_size = rd_px(18)});
// child's computed color and font_size come from node; its background doesn't
```

Inherited properties: `color`, `font`, `font_size`, `line_height`, `white_space`, `user_select`, `pointer_events`,
`caret_color`, `selection_color`, `text_align`. The rest start from the default on every node.

## Computed styles

`rd_node_computed_style` returns the fully resolved style, with every field set. It computes styles if they are
dirty. The pointer is valid until the next node is created.
