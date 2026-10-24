# Limits

What raydom doesn't do in this version. Behavior outside these limits is unspecified.

## Layout

- No `display: block` or `inline`; every container is flex or grid. Text nodes are leaves.
- Grid: no named lines, no named areas, no `dense` auto-flow; an auto repeat must be the whole track list.
- No `position: fixed` or `sticky`, and no `z-index`. Children draw in tree order, later siblings on top; content
  that must sit above everything goes in the [overlay](../guide/overlay.md).
- No `aspect_ratio`, no `transform`. Animate `inset` to move a node without reflowing its neighbors.
- Transitions don't animate `width: auto`, unit changes (px to %), or `display`; those change at once.
- `min_width`/`min_height` `auto` is 0, not the content size.

## Text

- One font per text node. No rich text inside a node; use several text nodes in a row.
- No shaping: no ligatures, no contextual forms, no bidirectional text. Pair kerning from the font is applied. Latin, Cyrillic,
  Greek and CJK display; Arabic and Indic scripts don't shape correctly.
- No hyphenation. Words longer than the line overflow.
- No double- or triple-click word or line selection in text (`ev->clicks` reports double clicks to listeners).

## Input

- No IME composition events. Text input is one codepoint at a time from raylib.
- No touch or gesture input beyond what raylib reports as the mouse.
- No accessibility tree.

## Rendering

- SVG: no text, filters, masks, clip paths, patterns or embedded images. Gradient coordinates written as unitless
  fractions (`x2="1"`) render as a flat color; use percentages (`x2="100%"`). `currentColor` tints the whole SVG.
- No background images; an image is its own node. Images are static (no GIF animation) unless updated with
  `rd_image_update`.
- Shadows are outer box shadows only.
- No gradients.

## Router

Paths have no query strings or fragments, and segments aren't percent-decoded. In WebAssembly builds the router
doesn't read or write the browser's URL or history; it keeps the browser from leaving the page on back and forward
input instead. There are no transforms, so slide transitions move pages
with `inset`. Layout caches keep that cheap: a page's contents aren't measured again while it slides.

## Threads

A context is single-threaded. Separate contexts on separate threads don't share state, but raylib drawing must
happen on the thread that owns the window.
