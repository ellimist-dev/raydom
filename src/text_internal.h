#ifndef RAYDOM_TEXT_INTERNAL_H
#define RAYDOM_TEXT_INTERNAL_H

#include "internal.h"

typedef struct rd__font rd__font;

// Resolves {0, true} to the context default. Null if there is no such font.
rd__font* rd__font_get(const rd_ctx* ctx, rd_font font);
float     rd__font_scale(const rd__font* f, float px);
void      rd__font_vmetrics(const rd__font* f, float px, float* ascent, float* descent, float* line_gap);
// Glyph index for a codepoint, and its advance in font units. Cached per font.
int32_t rd__font_glyph(rd__font* f, uint32_t cp, int32_t* advance);
// Kerning between two glyphs in font units.
int32_t  rd__font_kern(const rd__font* f, int32_t g1, int32_t g2);
void     rd__font_ctx_free(rd_ctx* ctx);
uint32_t rd__font_id(const rd__font* f);
// Glyph bitmap bounds relative to the pen position, y down, at a pixel scale (pixels per unit).
void rd__font_glyph_box(const rd__font* f, int32_t glyph, float scale, int* x0, int* y0, int* x1, int* y1);
// 8-bit coverage into out.
void rd__font_glyph_render(const rd__font* f, int32_t glyph, float scale, uint8_t* out, int w, int h, int stride);

uint32_t rd__utf8_decode(const char* s, size_t len, size_t* i);

enum { RD_TOKEN_WORD,
       RD_TOKEN_SPACE,
       RD_TOKEN_NEWLINE };

typedef struct rd__token {
  uint32_t start, end;  // codepoint range
  float    width;
  uint8_t  kind;
} rd__token;

typedef struct rd__line {
  uint32_t tok_start, tok_end;
  float    width;             // excludes trailing spaces
  uint32_t cp_start, cp_end;  // codepoints the line covers, trailing spaces included
} rd__line;

struct rd__input;

typedef struct rd_text {
  rd_ctx*   ctx;
  uint32_t  node;
  char*     utf8;
  size_t    len;
  uint32_t* cps;
  uint32_t* offsets;
  uint32_t  cp_count;

  // Tokens for the current font, size and white_space.
  rd__token*     tokens;
  uint32_t       token_count, token_cap;
  rd__font*      tok_font;
  float          tok_size;
  rd_white_space tok_ws;
  bool           tok_valid;

  // Lines for the last rendered width.
  rd__line* lines;
  uint32_t  line_count, line_cap;
  float     lines_width;
  bool      lines_valid;

  struct rd__input* input;  // Non-null for editable text.
  rd_highlight*     highlights;
  size_t            highlight_count;
} rd_text;

// A laid-out text leaf, for rendering and hit testing.
typedef struct rd__text_layout {
  const uint32_t*  cps;
  const uint32_t*  offsets;  // byte offset of each codepoint
  const rd__token* tokens;
  const rd__line*  lines;
  uint32_t         line_count;
  rd__font*        font;
  float            size, line_height, baseline;  // baseline: from the line top
  float            units;                        // pixels per font unit
  float            space_width;
  uint32_t         cp_count;
  bool             collapse;
} rd__text_layout;

// Position of a codepoint from the start of its line, before alignment.
float rd__text_cp_x(const rd__text_layout* tl, uint32_t line, uint32_t cp);
// The line showing a codepoint; at a soft wrap the later line.
uint32_t rd__text_line_of(const rd__text_layout* tl, uint32_t cp);
// Nearest caret position to an x offset from the line start.
uint32_t rd__text_cp_at(const rd__text_layout* tl, uint32_t line, float x);
rd_text* rd__text_get(const rd_ctx* ctx, uint32_t i);
float    rd__text_line_height(const rd_style* st);
uint32_t rd__text_cp_of_byte(const rd_text* t, size_t byte);
// Byte offset nearest a root-space point; above the text is 0, below is the end of the last shown
// line. dx shifts content horizontally, for scrolled inputs.
size_t rd__text_byte_at(rd_ctx* ctx, uint32_t i, float x, float y, float dx);
// Lines drawn under line_clamp.
uint32_t rd__text_visible_lines(const rd_style* st, const rd__text_layout* tl);

bool rd__input_multiline(const struct rd__input* in);
void rd__input_free(struct rd__input* in);
void rd__input_clamp(rd_text* t);

bool rd__is_text(const rd_ctx* ctx, uint32_t i);
// Lays the node's text out at a content-box width.
bool rd__text_layout_get(rd_ctx* ctx, uint32_t i, float width, rd__text_layout* out);
// First baseline from the content-box top; NaN without a font.
float rd__text_baseline(rd_ctx* ctx, uint32_t i);

#endif
