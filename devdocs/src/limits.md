# Known limits and debt

## Missing features

| Area           | Limit                                                                     |
| -------------- | ------------------------------------------------------------------------- |
| Text           | No shaping, bidirectional text, IME composition, hyphenation, or double-click word selection |
| Accessibility  | Roles are recorded but no platform backend reads them (AccessKit is the plan) |
| Layout         | No transforms, z-index, `position: fixed`/`sticky`, aspect ratio, grid named lines or areas, dense packing; auto repeats can't have fixed tracks around them |
| Animation      | No springs or keyframes; `width: auto`, unit changes and `display` don't animate; closing popups don't fade |
| Images         | GIFs show the first frame; SVG lacks text, filters, masks, clip paths and patterns |
| Input          | No touch or gestures beyond raylib's mouse                                |
| Views          | Fixed row heights; no column resizing; tree items are added up front      |
| Threads        | A context is single-threaded                                              |

## Debt

- **`paint_dirty` is written but never read.** It's the hook for skipping display list rebuilds on unchanged
  frames.
- **Track lists are never freed.** They're interned by content, so recreating the same lists is free, but code that
  creates ever-new distinct lists (track sizes computed from a live value) grows the pool until the context is
  destroyed. Auto-repeated tracks cover the common case (columns that follow the width); freeing would need styles to
  hold counted references.
- **Class changes scan every node.** `invalidate_class` is O(nodes). Fine at startup; a per-class node list would
  fix it if classes start changing at runtime.
- **Linear registries.** `rdx__lookup` and the popup and toast lists are linear scans. Fine for the numbers of
  components a UI has; controls already use an O(1) index.
- **Glyph atlas resets when full.** A UI showing many sizes of many glyphs at once could thrash it. Multiple atlas
  pages would fix it.
- **No CI for tests.** Only docs publishing runs in GitHub Actions.
- **`rd_ctx` padding.** clang-tidy's opt-in padding check flags 33 bytes; reorder fields when convenient.
- **nanosvg gradients** with unitless fractional coordinates render as a flat color (upstream bug; use
  percentages).
