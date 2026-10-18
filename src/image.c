#include <stdlib.h>
#include <string.h>

#include "internal.h"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_LINEAR
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include "external/stb_image.h"

// nanosvg's public functions renamed, so an application linking its own copy doesn't clash.
#define nsvgParseFromFile     rd__nsvgParseFromFile
#define nsvgParse             rd__nsvgParse
#define nsvgDuplicatePath     rd__nsvgDuplicatePath
#define nsvgDelete            rd__nsvgDelete
#define nsvgCreateRasterizer  rd__nsvgCreateRasterizer
#define nsvgRasterize         rd__nsvgRasterize
#define nsvgDeleteRasterizer  rd__nsvgDeleteRasterizer
#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#include <math.h>
#include <stdio.h>

#include "external/nanosvg.h"
#include "external/nanosvgrast.h"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// Stands in for currentColor while parsing; nanosvg doesn't know it.
#define CURRENT_COLOR_SENTINEL "#fe01fd"
static const unsigned int SENTINEL = 0xfd01feu;  // NSVG_RGB(0xfe, 0x01, 0xfd)

static rd__image* get(const rd_ctx* ctx, rd_image image) {
  if (!ctx || image.id == 0 || image.id > ctx->image_count) return NULL;
  rd__image* im = &ctx->images[image.id - 1];
  return im->alive ? im : NULL;
}

rd__image* rd__image_get(const rd_ctx* ctx, uint32_t id) { return get(ctx, (rd_image){id}); }

// Ids aren't reused, so a handle to an unloaded image stays null.
static rd_image add(rd_ctx* ctx, unsigned char* pixels, int w, int h) {
  if (ctx->image_count == ctx->image_cap) {
    uint32_t   cap = ctx->image_cap ? ctx->image_cap * 2 : 8;
    rd__image* p   = realloc(ctx->images, cap * sizeof *p);
    if (!p) {
      free(pixels);
      return (rd_image){0};
    }
    ctx->images    = p;
    ctx->image_cap = cap;
  }
  ctx->images[ctx->image_count++] = (rd__image){.pixels       = pixels,
                                                .w            = w,
                                                .h            = h,
                                                .natural_w    = (float)w,
                                                .natural_h    = (float)h,
                                                .raster_scale = 1,
                                                .alive        = true,
                                                .dirty        = true};
  return (rd_image){ctx->image_count};
}

// SVG: an XML document whose root is <svg>, possibly after a declaration, comments and whitespace.
static bool looks_like_svg(const unsigned char* d, size_t n) {
  size_t i = 0;
  if (n >= 3 && d[0] == 0xef && d[1] == 0xbb && d[2] == 0xbf) i = 3;  // BOM
  while (i < n && (d[i] == ' ' || d[i] == '\t' || d[i] == '\r' || d[i] == '\n')) i++;
  if (i < n && d[i] != '<') return false;
  size_t end = n < i + 4096 ? n : i + 4096;
  for (size_t j = i; j + 4 <= end; j++)
    if (memcmp(d + j, "<svg", 4) == 0) return true;
  return false;
}

static rd_image add_svg(rd_ctx* ctx, const char* data, size_t size) {
  // nanosvg parses in place and needs a terminator; currentColor becomes a sentinel color.
  size_t      count = 0;
  const char* cc    = "currentColor";
  for (size_t j = 0; j + 12 <= size; j++)
    if (memcmp(data + j, cc, 12) == 0) count++;
  char* text = malloc(size + 1);
  if (!text) return (rd_image){0};
  size_t o = 0;
  for (size_t j = 0; j < size;) {
    if (j + 12 <= size && memcmp(data + j, cc, 12) == 0) {
      memcpy(text + o, CURRENT_COLOR_SENTINEL, 7);
      o += 7;
      j += 12;
    } else {
      text[o++] = data[j++];
    }
  }
  text[o]            = 0;
  struct NSVGimage* svg = nsvgParse(text, "px", 96);
  free(text);
  if (!svg) return (rd_image){0};
  if (svg->width <= 0 || svg->height <= 0) {
    nsvgDelete(svg);
    return (rd_image){0};
  }
  bool current = false;
  for (NSVGshape* sh = svg->shapes; sh && count; sh = sh->next) {
    if (sh->fill.type == NSVG_PAINT_COLOR && (sh->fill.color & 0xffffffu) == SENTINEL) {
      sh->fill.color = (sh->fill.color & 0xff000000u) | 0xffffffu;
      current        = true;
    }
    if (sh->stroke.type == NSVG_PAINT_COLOR && (sh->stroke.color & 0xffffffu) == SENTINEL) {
      sh->stroke.color = (sh->stroke.color & 0xff000000u) | 0xffffffu;
      current          = true;
    }
  }
  rd_image img = add(ctx, NULL, 0, 0);
  if (!img.id) {
    nsvgDelete(svg);
    return img;
  }
  rd__image* im     = &ctx->images[img.id - 1];
  im->svg           = svg;
  im->natural_w     = svg->width;
  im->natural_h     = svg->height;
  im->raster_scale  = 0;  // nothing rasterized yet
  im->current_color = current;
  im->dirty         = false;
  return img;
}

bool rd__image_rasterize(rd__image* im, float scale) {
  if (!im->svg) return false;
  float side = fmaxf(im->natural_w, im->natural_h);
  if (side * scale > RD__MAX_RASTER) scale = RD__MAX_RASTER / side;
  int w = (int)ceilf(im->natural_w * scale), h = (int)ceilf(im->natural_h * scale);
  if (w <= 0 || h <= 0) return false;
  unsigned char*  px = calloc((size_t)w * (size_t)h, 4);
  NSVGrasterizer* r  = px ? nsvgCreateRasterizer() : NULL;
  if (!r) {
    free(px);
    return false;
  }
  nsvgRasterize(r, im->svg, 0, 0, scale, px, w, h, w * 4);
  nsvgDeleteRasterizer(r);
  free(im->pixels);
  im->pixels       = px;
  im->w            = w;
  im->h            = h;
  im->raster_scale = scale;
  im->dirty        = true;
  return true;
}

static unsigned char* copy_pixels(const void* rgba, int w, int h) {
  if (!rgba || w <= 0 || h <= 0 || (size_t)w > SIZE_MAX / 4 / (size_t)h) return NULL;
  size_t         n = (size_t)w * (size_t)h * 4;
  unsigned char* p = malloc(n);
  if (p) memcpy(p, rgba, n);
  return p;
}

rd_image rd_image_load(rd_ctx* ctx, const void* data, size_t size) {
  if (!ctx || !data || size == 0 || size > INT32_MAX) return (rd_image){0};
  if (looks_like_svg(data, size)) return add_svg(ctx, data, size);
  int            w, h, n;
  unsigned char* px = stbi_load_from_memory(data, (int)size, &w, &h, &n, 4);
  if (!px) return (rd_image){0};
  // stb's allocator is malloc, so the pixels are freed like the rest.
  return add(ctx, px, w, h);
}

rd_image rd_image_load_file(rd_ctx* ctx, const char* path) {
  if (!ctx || !path) return (rd_image){0};
  FILE* f = fopen(path, "rb");
  if (!f) return (rd_image){0};
  unsigned char* data = NULL;
  long           size = fseek(f, 0, SEEK_END) == 0 ? ftell(f) : -1;
  if (size > 0 && size <= INT32_MAX && fseek(f, 0, SEEK_SET) == 0) data = malloc((size_t)size);
  if (data && fread(data, 1, (size_t)size, f) != (size_t)size) {
    free(data);
    data = NULL;
  }
  fclose(f);
  rd_image img = data ? rd_image_load(ctx, data, (size_t)size) : (rd_image){0};
  free(data);
  return img;
}

rd_image rd_image_from_pixels(rd_ctx* ctx, const void* rgba, int width, int height) {
  if (!ctx) return (rd_image){0};
  unsigned char* px = copy_pixels(rgba, width, height);
  return px ? add(ctx, px, width, height) : (rd_image){0};
}

typedef struct image_leaf {
  rd_ctx*  ctx;
  uint32_t id;
} image_leaf;

static rd_size image_measure(void* data, float w, rd_measure_mode wm, float h, rd_measure_mode hm) {
  const image_leaf* leaf = data;
  const rd__image*  im   = get(leaf->ctx, (rd_image){leaf->id});
  float             iw = im ? im->natural_w : 0, ih = im ? im->natural_h : 0;
  if (wm == RD_MEASURE_EXACTLY && hm == RD_MEASURE_EXACTLY) return (rd_size){w, h};
  if (iw <= 0 || ih <= 0) return (rd_size){wm == RD_MEASURE_EXACTLY ? w : 0, hm == RD_MEASURE_EXACTLY ? h : 0};
  if (wm == RD_MEASURE_EXACTLY) return (rd_size){w, w * ih / iw};
  if (hm == RD_MEASURE_EXACTLY) return (rd_size){h * iw / ih, h};
  return (rd_size){iw, ih};
}

static bool is_image(const rd_ctx* ctx, uint32_t i) { return ctx->layouts[i].measure_fn == image_measure; }

uint32_t rd__node_image(const rd_ctx* ctx, uint32_t i) {
  return is_image(ctx, i) ? ((const image_leaf*)ctx->layouts[i].measure_data)->id : 0;
}

bool rd_node_set_image(rd_ctx* ctx, rd_node node, rd_image image) {
  if (!rd__slot(ctx, node)) return false;
  if (image.id == 0) {
    if (is_image(ctx, node.index)) rd_node_set_measure(ctx, node, NULL, NULL, NULL);
    return true;
  }
  if (is_image(ctx, node.index)) {
    ((image_leaf*)ctx->layouts[node.index].measure_data)->id = image.id;
    rd_node_mark_layout_dirty(ctx, node);
    return true;
  }
  image_leaf* leaf = malloc(sizeof *leaf);
  if (!leaf) return false;
  *leaf = (image_leaf){ctx, image.id};
  if (!rd_node_set_measure(ctx, node, image_measure, leaf, free)) {
    free(leaf);
    return false;
  }
  return true;
}

rd_image rd_node_image(const rd_ctx* ctx, rd_node node) {
  return rd__slot(ctx, node) ? (rd_image){rd__node_image(ctx, node.index)} : (rd_image){0};
}

rd_size rd_image_size(const rd_ctx* ctx, rd_image image) {
  const rd__image* im = get(ctx, image);
  return im ? (rd_size){im->natural_w, im->natural_h} : (rd_size){0, 0};
}

static void relayout_users(rd_ctx* ctx, uint32_t id) {
  for (uint32_t i = 1; i < ctx->len; i++)
    if ((ctx->slots[i].flags & RD_SLOT_ALIVE) && rd__node_image(ctx, i) == id) rd__mark_layout_dirty(ctx, i);
}

bool rd_image_update(rd_ctx* ctx, rd_image image, const void* rgba, int width, int height) {
  rd__image*     im = get(ctx, image);
  unsigned char* px = im ? copy_pixels(rgba, width, height) : NULL;
  if (!px) return false;
  bool resized = (float)width != im->natural_w || (float)height != im->natural_h;
  if (im->svg) {  // pixels replace the SVG
    nsvgDelete(im->svg);
    im->svg           = NULL;
    im->current_color = false;
  }
  free(im->pixels);
  im->pixels       = px;
  im->w            = width;
  im->h            = height;
  im->natural_w    = (float)width;
  im->natural_h    = (float)height;
  im->raster_scale = 1;
  im->dirty        = true;
  ctx->paint_dirty = true;
  if (resized) relayout_users(ctx, image.id);
  return true;
}

void rd_image_unload(rd_ctx* ctx, rd_image image) {
  rd__image* im = get(ctx, image);
  if (!im) return;
  rd__image_release(ctx, im);
  free(im->pixels);
  if (im->svg) nsvgDelete(im->svg);
  *im              = (rd__image){0};
  ctx->paint_dirty = true;
  relayout_users(ctx, image.id);
}

void rd__images_free(rd_ctx* ctx) {
  for (uint32_t j = 0; j < ctx->image_count; j++) {
    rd__image_release(ctx, &ctx->images[j]);
    free(ctx->images[j].pixels);
    if (ctx->images[j].svg) nsvgDelete(ctx->images[j].svg);
  }
  free(ctx->images);
  ctx->images = NULL;
}
