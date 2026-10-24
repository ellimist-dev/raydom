#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "text_internal.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "external/stb_truetype.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

enum { PAGE_BITS  = 8,
       PAGE_SIZE  = 1 << PAGE_BITS,
       PAGE_COUNT = 0x110000 >> PAGE_BITS };

typedef struct glyph_entry {
  int32_t glyph;  // -1 until looked up
  int32_t advance;
} glyph_entry;

struct rd__font {
  uint32_t       id;
  stbtt_fontinfo info;
  uint8_t*       owned;
  float          em_scale;  // pixels per font unit at 1px
  int            ascent, descent, line_gap;
  bool           kern;
  glyph_entry*   pages[PAGE_COUNT];
};

rd_font rd_font_load(rd_ctx* ctx, const void* data, size_t size, bool copy) {
  if (!ctx || !data || !size || size > INT32_MAX) return (rd_font){0};
  if (ctx->font_count == ctx->font_cap) {
    uint32_t   cap = ctx->font_cap ? ctx->font_cap * 2 : 4;
    rd__font** f   = (rd__font**)realloc((void*)ctx->fonts, cap * sizeof *f);
    if (!f) return (rd_font){0};
    ctx->fonts    = f;
    ctx->font_cap = cap;
  }
  rd__font* f = calloc(1, sizeof *f);
  if (!f) return (rd_font){0};
  const uint8_t* bytes = data;
  if (copy) {
    f->owned = malloc(size);
    if (!f->owned) {
      free(f);
      return (rd_font){0};
    }
    memcpy(f->owned, data, size);
    bytes = f->owned;
  }
  int offset = stbtt_GetFontOffsetForIndex(bytes, 0);
  if (offset < 0 || !stbtt_InitFont(&f->info, bytes, offset)) {
    free(f->owned);
    free(f);
    return (rd_font){0};
  }
  f->em_scale = stbtt_ScaleForMappingEmToPixels(&f->info, 1.0f);
  stbtt_GetFontVMetrics(&f->info, &f->ascent, &f->descent, &f->line_gap);
  f->kern                       = f->info.kern || f->info.gpos;
  f->id                         = ctx->font_count + 1;
  ctx->fonts[ctx->font_count++] = f;
  if (!ctx->default_font) rd_ctx_set_default_font(ctx, (rd_font){ctx->font_count, true});  // relayouts text
  return (rd_font){ctx->font_count, true};
}

rd_font rd_font_load_file(rd_ctx* ctx, const char* path) {
  FILE* fp = fopen(path, "rb");
  if (!fp) return (rd_font){0};
  rd_font font = {0};
  if (fseek(fp, 0, SEEK_END) == 0) {
    long size = ftell(fp);
    if (size > 0 && fseek(fp, 0, SEEK_SET) == 0) {
      uint8_t* buf = malloc((size_t)size);
      if (buf && fread(buf, 1, (size_t)size, fp) == (size_t)size) font = rd_font_load(ctx, buf, (size_t)size, true);
      free(buf);
    }
  }
  fclose(fp);
  return font;
}

void rd_ctx_set_default_font(rd_ctx* ctx, rd_font font) {
  if (ctx && font.id && font.id <= ctx->font_count) {
    ctx->default_font = font.id;
    for (uint32_t i = 1; i < ctx->len; i++)
      if (ctx->slots[i].flags & RD_SLOT_ALIVE) rd__mark_layout_dirty(ctx, i);
  }
}

rd__font* rd__font_get(const rd_ctx* ctx, rd_font font) {
  if (!ctx) return NULL;
  uint32_t id = font.id ? font.id : ctx->default_font;
  return id && id <= ctx->font_count ? ctx->fonts[id - 1] : NULL;
}

float rd__font_scale(const rd__font* f, float px) { return f->em_scale * px; }

void rd__font_vmetrics(const rd__font* f, float px, float* ascent, float* descent, float* line_gap) {
  float s   = rd__font_scale(f, px);
  *ascent   = (float)f->ascent * s;
  *descent  = (float)-f->descent * s;
  *line_gap = (float)f->line_gap * s;
}

int32_t rd__font_glyph(rd__font* f, uint32_t cp, int32_t* advance) {
  if (cp >= 0x110000) cp = 0xFFFD;
  glyph_entry** page = &f->pages[cp >> PAGE_BITS];
  if (!*page) {
    *page = malloc(PAGE_SIZE * sizeof **page);
    if (!*page) {
      int g = stbtt_FindGlyphIndex(&f->info, (int)cp), adv = 0, lsb = 0;
      stbtt_GetGlyphHMetrics(&f->info, g, &adv, &lsb);
      *advance = adv;
      return g;
    }
    for (int j = 0; j < PAGE_SIZE; j++) (*page)[j].glyph = -1;
  }
  glyph_entry* e = &(*page)[cp & (PAGE_SIZE - 1)];
  if (e->glyph < 0) {
    int adv = 0, lsb = 0;
    e->glyph = stbtt_FindGlyphIndex(&f->info, (int)cp);
    stbtt_GetGlyphHMetrics(&f->info, e->glyph, &adv, &lsb);
    e->advance = adv;
  }
  *advance = e->advance;
  return e->glyph;
}

int32_t rd__font_kern(const rd__font* f, int32_t g1, int32_t g2) {
  return f->kern && g1 > 0 && g2 > 0 ? stbtt_GetGlyphKernAdvance(&f->info, g1, g2) : 0;
}

uint32_t rd__font_id(const rd__font* f) { return f->id; }

void rd__font_glyph_box(const rd__font* f, int32_t glyph, float scale, int* x0, int* y0, int* x1, int* y1) {
  stbtt_GetGlyphBitmapBox(&f->info, glyph, scale, scale, x0, y0, x1, y1);
}

void rd__font_glyph_render(const rd__font* f, int32_t glyph, float scale, uint8_t* out, int w, int h, int stride) {
  stbtt_MakeGlyphBitmap(&f->info, out, w, h, stride, scale, scale, glyph);
}

void rd__font_ctx_free(rd_ctx* ctx) {
  for (uint32_t j = 0; j < ctx->font_count; j++) {
    rd__font* f = ctx->fonts[j];
    for (int p = 0; p < PAGE_COUNT; p++) free(f->pages[p]);
    free(f->owned);
    free(f);
  }
  free((void*)ctx->fonts);
  ctx->fonts      = NULL;
  ctx->font_count = ctx->font_cap = 0;
}

rd_font_metrics rd_font_get_metrics(const rd_ctx* ctx, rd_font font, float size) {
  rd_font_metrics m = {0};
  rd__font*       f = rd__font_get(ctx, font);
  if (f) rd__font_vmetrics(f, size, &m.ascent, &m.descent, &m.line_gap);
  return m;
}

uint32_t rd__utf8_decode(const char* s, size_t len, size_t* i) {
  const uint8_t* p = (const uint8_t*)s + *i;
  size_t         n = len - *i;
  uint32_t       c = p[0];
  int            extra;
  uint32_t       min;
  if (c < 0x80) {
    *i += 1;
    return c;
  } else if ((c & 0xE0) == 0xC0) {
    extra = 1, min = 0x80, c &= 0x1F;
  } else if ((c & 0xF0) == 0xE0) {
    extra = 2, min = 0x800, c &= 0x0F;
  } else if ((c & 0xF8) == 0xF0) {
    extra = 3, min = 0x10000, c &= 0x07;
  } else {
    *i += 1;
    return 0xFFFD;
  }
  if ((size_t)extra >= n) {
    *i += 1;
    return 0xFFFD;
  }
  for (int j = 1; j <= extra; j++) {
    if ((p[j] & 0xC0) != 0x80) {
      *i += (size_t)j;
      return 0xFFFD;
    }
    c = (c << 6) | (p[j] & 0x3F);
  }
  *i += (size_t)extra + 1;
  if (c < min || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) return 0xFFFD;
  return c;
}

float rd_font_measure(const rd_ctx* ctx, rd_font font, float size, const char* utf8, size_t len) {
  rd__font* f = rd__font_get(ctx, font);
  if (!f || !utf8) return 0;
  if (len == RD_TEXT_NUL) len = strlen(utf8);
  int64_t units = 0;
  int32_t prev  = -1;
  for (size_t i = 0; i < len;) {
    int32_t adv, g = rd__font_glyph(f, rd__utf8_decode(utf8, len, &i), &adv);
    units += adv + (prev >= 0 ? rd__font_kern(f, prev, g) : 0);
    prev = g;
  }
  return (float)units * rd__font_scale(f, size);
}
