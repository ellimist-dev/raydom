#ifndef RAYDOM_IMAGE_H
#define RAYDOM_IMAGE_H

#include <raydom/layout.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// An image owned by the context. Zero is null. Images are decoded when loaded and uploaded to the
// GPU on first draw, so loading and layout work without a window.
typedef struct rd_image {
  uint32_t id;
} rd_image;

// PNG, JPEG, BMP, GIF (first frame), TGA, PSD and SVG, from memory or a file; SVG is recognized by
// content. SVGs are rasterized at the size they're drawn, and ones using currentColor are tinted by the
// node's color. Null on failure.
rd_image rd_image_load(rd_ctx* ctx, const void* data, size_t size);
rd_image rd_image_load_file(rd_ctx* ctx, const char* path);
// width * height RGBA8 pixels, row by row, copied.
rd_image rd_image_from_pixels(rd_ctx* ctx, const void* rgba, int width, int height);
// Replaces the pixels, at the same or a new size. Nodes showing the image redraw, and relayout if the
// size changed. For content that changes every frame, such as a video or an emulator screen.
bool rd_image_update(rd_ctx* ctx, rd_image image, const void* rgba, int width, int height);
// Frees the pixels and texture. Nodes still showing it draw nothing.
void    rd_image_unload(rd_ctx* ctx, rd_image image);
// Size in pixels; zero for a null or unloaded image.
rd_size rd_image_size(const rd_ctx* ctx, rd_image image);

// Makes the node an image leaf, sized from the image as CSS sizes <img>: its natural size, or the
// other dimension from the aspect ratio when one is set. object_fit places it in the content box.
// A null image clears it.
bool     rd_node_set_image(rd_ctx* ctx, rd_node node, rd_image image);
rd_image rd_node_image(const rd_ctx* ctx, rd_node node);

#ifdef __cplusplus
}
#endif

#endif
