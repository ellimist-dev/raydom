# Rendering

## The display list

`rd__display_list` walks the laid-out tree and emits commands into a reusable array:

| Command       | Draws                                                          |
| ------------- | -------------------------------------------------------------- |
| `SHADOW`      | A blurred rounded box                                          |
| `RECT`        | Background and border of a box, rounded or not                 |
| `TEXT`        | A text node's lines                                            |
| `PLACEHOLDER` | An empty input's placeholder                                   |
| `FILL`        | A plain rectangle: selection, highlight, caret                 |
| `CLIP`        | Sets the scissor; the enclosing clip is restored by a second `CLIP` |
| `PAINT`       | A canvas callback                                              |
| `IMAGE`       | An image placed by `object_fit`, with its source rectangle     |

The walk applies opacity multiplicatively down the tree, culls nodes outside the current clip, and draws scrollbars
after a scroll container's children. Commands carry everything the renderer needs except text layout and textures,
which it reads from the context.

The list is rebuilt every `rd_draw`. That's a deliberate simplicity: building it is a linear walk with no allocation
in the steady state, much cheaper than the GPU work, and it means there's no retained render state to invalidate.

Keeping the display list separate from raylib is also what lets `tests/test_render.c` check drawing headlessly.

## The renderer

`render.c` executes commands with raylib and `rlgl`:

- **Rounded boxes and shadows** use one fragment shader evaluating a signed distance to a rounded rectangle with
  per-corner radii and per-side borders, antialiased over one device pixel. Shadows are the same shape with a
  blur. Boxes without corner radii skip the shader and draw with raylib's batched rectangles, borders included.
- **Pixel snapping.** Rect edges and text baselines snap to device pixels (`snap(v, dpi)`), so borders and text stay
  crisp at fractional DPI.
- **Images** draw with `DrawTexturePro`, or through the same shader in a textured mode when the box has radii, so
  rounded corners clip them.
- **Clipping** is the GPU scissor, in screen pixels.
- **Canvases**: `PAINT` flushes the batch, ends any scissor, calls the application's callback with the content box
  and the clip, then restores state.

## Glyph atlas

Text is drawn glyph by glyph from one 2048×2048 RGBA texture. Glyphs rasterize on first use (stb_truetype coverage
into the alpha channel) at the device-pixel size, and are found in an open-addressing table keyed by font, size in
quarter pixels and glyph. Packing is shelf-based; when the atlas fills, it's cleared and refilled on demand. That
favors simplicity over long-lived caching: a UI rarely shows more distinct glyphs than fit, and clearing is
correct, just a hitch.

## Images and SVG

Decoding (stb_image) and SVG parsing (nanosvg) happen in `image.c` at load, with no GPU, so image nodes lay out
headlessly. The texture is created on first draw and the CPU pixels freed. SVGs keep their parsed form and
rasterize in `draw_image` at the drawn size times the display scale, again whenever drawn larger (capped at 4096
pixels per side); mipmaps cover smaller uses. Images with `currentColor` are drawn tinted by the node's color.

## GPU lifetime

The shader, atlas and textures belong to the context and are released in `rd_ctx_destroy` only if the window still
exists, which is why the user docs insist on destroying the context before `CloseWindow`.
