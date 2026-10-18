# Limits

What raydom doesn't do in this version. Behavior outside these limits is unspecified.

## Layout

- No `display: block` or `inline`; every container is flex or grid. Text nodes are leaves.
- Grid: no named lines, no named areas, no `dense` auto-flow, no `repeat(auto-fill)`.
- No `position: fixed` or `sticky`.
- No `z-index`; children draw in tree order, later siblings on top.
- No `aspect_ratio`, no `transform`.
- `min_width`/`min_height` `auto` is 0, not the content size.

## Text

- One font per text node. No rich text inside a node; use several text nodes in a row.
- No shaping: no ligatures, no contextual forms, no bidirectional text. Pair kerning from the font is applied. Latin, Cyrillic,
  Greek and CJK display; Arabic and Indic scripts don't shape correctly.
- No hyphenation. Words longer than the line overflow.
- No double- or triple-click word or line selection.

## Input

- No IME composition events. Text input is one codepoint at a time from raylib.
- No touch or gesture input beyond what raylib reports as the mouse.
- No accessibility tree.

## Rendering

- No images or textures as node backgrounds; use a canvas.
- Shadows are outer box shadows only.
- No gradients.

## Threads

A context is single-threaded. Separate contexts on separate threads don't share state, but raylib drawing must
happen on the thread that owns the window.
