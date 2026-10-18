# Styles and classes

`rd_style` is a struct with one field per property. Every field type reserves zero for "unset", so a
compound literal names only the properties it sets. The [property reference](../reference/properties.md) lists them
all.

## Inline styles

```c
{{#include ../../examples/styles.c:inline}}
```

`rd_node_set_style` replaces the inline style. `rd_node_merge_style` copies the set fields over it and leaves the
rest. Passing `NULL` to `rd_node_set_style` clears it.

Value constructors: `rd_px`, `rd_pct`, `rd_num`, `rd_fr`, `rd_auto()`, `rd_min_content()`, `rd_max_content()`.
Colors: `rd_hex(0xRRGGBB)`, `rd_rgb`, `rd_rgba`. Box sides: `rd_edges_all`, `rd_edges_vh`, or designated
`.padding.left`. See [Values and units](../reference/values.md).

## Classes

A class is a reusable style with optional variants per state. Classes live until the context is destroyed;
`rd_class_set` replaces a class's base style and restyles every node that uses it.

```c
{{#include ../../examples/styles.c:classes}}
```

```c
{{#include ../../examples/styles.c:computed}}
```

## Precedence

For each property, the last of these that sets it wins:

1. Defaults (the reference's Default column).
2. The parent's computed value, for inherited properties.
3. Class base styles, in the order the classes were added.
4. Class state variants for the node's current states, ordered by state: `hover`, `focus_within`, `focus`,
   `focus_visible`, `active`, `disabled`. Within one state, later classes win.
5. The inline style.

Inline styles beat every class and every state variant. Put anything that should change on hover in a class, not
inline.

## States

raydom sets `hover`, `active`, `focus`, `focus_visible` and `focus_within` from input. `disabled` is set only by the
application.

```c
{{#include ../../examples/styles.c:states}}
```

A disabled node and its descendants receive no pointer down, up or click events and can't be focused. Hover and
pointer move still apply.

`focus_visible` is set when focus came from the keyboard, or when the focused node is a text input. Use it, not
`focus`, for focus rings.

## Inheritance

```c
{{#include ../../examples/styles.c:inherit}}
```

Inherited properties: `color`, `font`, `font_size`, `line_height`, `white_space`, `user_select`, `pointer_events`,
`caret_color`, `selection_color`, `text_align`. The rest start from the default on every node.

## Computed styles

`rd_node_computed_style` returns the fully resolved style, with every field set. It computes styles if they are
dirty. The pointer is valid until the next node is created.
