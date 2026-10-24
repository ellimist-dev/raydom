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
| Router         | No browser history or URL sync on the web; no query strings or percent-decoding; slide moves `inset` (no transforms in the core); cached layout keeps it to the wrapper and its ancestors per frame |
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
- **Linear registries.** The popup and toast lists are linear scans. Fine for the numbers of popups a UI has open.
- **Glyph atlas cap.** When a frame overflows the 2048² atlas it is emptied once, then grown to 4096² (64 MB as
  RGBA). Glyphs that still don't fit are skipped, and an empty atlas is retried every 60 frames. Pages, or a
  single-channel atlas, would lift the cap and quarter the memory.
- **Shader after rd_draw.** rlgl keeps the active shader private, so `rd_draw` can't restore an app's shader and
  exits with raylib's default. Blend state is read from GL and restored.
- **`rd_ctx` padding.** clang-tidy's opt-in padding check flags 33 bytes; reorder fields when convenient.
- **nanosvg gradients** with unitless fractional coordinates render as a flat color (upstream bug; use
  percentages).
