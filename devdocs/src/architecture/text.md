# Text

## Fonts

`font.c` wraps stb_truetype. A font is loaded once into the context (copied or borrowed), gets an id, and caches
codepoint-to-glyph lookups and advances. Kerning comes from the font's `kern`/GPOS pair tables. There is no
shaping: no ligatures, contextual forms or bidirectional reordering. That's the biggest limit of the text stack;
see [Known limits](../limits.md).

## Text nodes

`rd_node_set_text` attaches an `rd_text` to the node as its measure data. It holds the UTF-8, the decoded
codepoints, and byte offsets per codepoint.

Two caches sit in `rd_text`:

- **Tokens** for the current font, size and `white_space`: words, spaces and newlines with widths. CJK characters are
  single-character words, which gives them a break opportunity on each side.
- **Lines** for the last width: greedy breaking (`break_lines`) over the tokens. Trailing spaces belong to a line but
  hang outside its width; collapsible spaces at a line start are dropped.

Measuring (`text_measure`) runs line breaking at the available width, or with no limit for max-content; min-content
is the widest word. `line_clamp` caps the measured line count; `text_overflow` adds the ellipsis when drawing.

Byte offsets are the public unit everywhere (selection, highlights, input carets) because applications index their
own strings in bytes; codepoint indices stay internal.

## Inputs

`rd_node_make_input` adds an `rd__input` to the text: anchor and caret (bytes), a goal column for vertical movement,
horizontal scroll for single-line inputs, and undo/redo stacks of snapshots. Consecutive typing merges into one
undo step. Inputs keep all whitespace regardless of style. Key handling (`rd__input_key`) runs as the default action
of `KEY_DOWN` on a focused input, so a listener can `prevent_default` any key.

## The document selection

`selection.c` keeps an anchor and focus (node and byte offset) across text nodes. Ordering across nodes uses a
pre-order numbering computed lazily while a selection exists (`rd__selection_prepare`). Dragging maps the pointer to
the nearest selectable text (vertical distance first), and copying collects the selected text one line per node.

## Drawing text

Text is drawn by the renderer from the same line layout, glyph by glyph from the atlas (see
[Rendering](rendering.md#glyph-atlas)), at baselines snapped to device pixels.
