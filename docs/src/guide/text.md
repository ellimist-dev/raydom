# Text and fonts

## Fonts

```c
{{#include ../../examples/text.c:fonts}}
```

`rd_font_load` takes TrueType or OpenType data in memory. With `copy = false` the data must outlive the context;
with `true` raydom copies it. `rd_font_load_file` reads and copies a file. Both return a null handle (`id == 0`) on
failure. The first font loaded becomes the default; `rd_ctx_set_default_font` changes it. Either way, text already
in the tree is measured again with the new default at the next layout. In a style,
`(rd_font){0, true}` means "the default font".

`font` and `font_size` are inherited, so setting them on a container applies them to all the text inside it.
Glyphs are rasterized on demand into an atlas per font size and display scale.

## Text nodes

```c
{{#include ../../examples/text.c:text}}
```

`rd_node_set_text` makes the node a text leaf and copies the UTF-8 string. Pass a length, or `RD_TEXT_NUL` for a
NUL-terminated string. A text node's size comes from its text; it lays out no children. `rd_node_text` returns the
stored text, or `NULL` for a node that isn't a text leaf.

`line_height` accepts `rd_num` (a multiple of `font_size`), `rd_px`, or `rd_pct` (of `font_size`).

## Wrapping and overflow

```c
{{#include ../../examples/text.c:overflow}}
```

| `white_space` | Spaces and newlines | Wraps |
| ------------- | ------------------- | ----- |
| `NORMAL`      | Collapsed           | Yes   |
| `NOWRAP`      | Collapsed           | No    |
| `PRE`         | Preserved           | No    |
| `PRE_WRAP`    | Preserved           | Yes   |

Lines break at spaces, and between CJK characters. A word longer than the line isn't broken; it overflows, and
`text_overflow: ellipsis` cuts it. `text_overflow: ellipsis` replaces
the end of a line that doesn't fit with "…". With `line_clamp: n`, at most `n` lines are shown, and with ellipsis
the last shown line ends with "…" when text was cut.

`text_align` aligns each line within the node's content box: `START`, `CENTER` or `END`.

## Highlights

```c
{{#include ../../examples/text.c:highlights}}
```

Highlights are byte ranges with a background and an optional text color (alpha 0 keeps the node's color). They are
copied, kept across `rd_node_set_text`, and clamped to the text when drawn. Passing `NULL, 0` clears them. Use them
for search results or syntax marks; the user's selection is separate.

## Measuring

```c
{{#include ../../examples/text.c:measure}}
```

`rd_font_measure` returns the advance width of one unwrapped line. `rd_font_get_metrics` returns ascent, descent
(positive, below the baseline) and line gap in pixels. Both work without layout.

## Selection

Dragging over text whose `user_select` is `auto` (the default) selects it across text nodes, in tree order. The
selection is also settable from code:

```c
{{#include ../../examples/text.c:selection}}
```

Offsets are bytes. `rd_ctx_selection_text` writes the selected text, one line per text node, and returns its full
length, so a first call with `buf = NULL` sizes the buffer. `RD_EVENT_SELECTION_CHANGE` fires on the root when the
selection changes. Set `user_select: none` on buttons and labels that shouldn't start a selection.
