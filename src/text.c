#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "input_internal.h"
#include "layout_internal.h"

static bool collapses(rd_white_space ws) { return ws == RD_WHITE_SPACE_NORMAL || ws == RD_WHITE_SPACE_NOWRAP; }
static bool wraps(rd_white_space ws) { return ws == RD_WHITE_SPACE_NORMAL || ws == RD_WHITE_SPACE_PRE_WRAP; }

static bool is_space(uint32_t c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'; }

// Break opportunities around each character, as for CJK text without spaces.
static bool is_cjk(uint32_t c) {
  return (c >= 0x2E80 && c <= 0x9FFF) || (c >= 0xF900 && c <= 0xFAFF) || (c >= 0xFF00 && c <= 0xFFEF) ||
         (c >= 0x20000 && c <= 0x3FFFF);
}

static bool push_token(rd_text* t, rd__token tok) {
  if (t->token_count == t->token_cap) {
    uint32_t   cap = t->token_cap ? t->token_cap * 2 : 16;
    rd__token* p   = realloc(t->tokens, cap * sizeof *p);
    if (!p) return false;
    t->tokens    = p;
    t->token_cap = cap;
  }
  t->tokens[t->token_count++] = tok;
  return true;
}

static float space_width(rd__font* f, float scale) {
  int32_t adv;
  rd__font_glyph(f, ' ', &adv);
  return (float)adv * scale;
}

static bool tokenize(rd_text* t, rd__font* f, float size, rd_white_space ws) {
  if (t->tok_valid && t->tok_font == f && t->tok_size == size && t->tok_ws == ws) return true;
  t->token_count  = 0;
  t->lines_valid  = false;
  float    scale  = rd__font_scale(f, size);
  float    sw     = space_width(f, scale);
  bool     coll   = collapses(ws);
  bool     ok     = true;
  int64_t  units  = 0;
  int32_t  prev   = -1;
  uint32_t wstart = UINT32_MAX;

#define FLUSH_WORD()                                                                    \
  do {                                                                                  \
    if (wstart != UINT32_MAX) {                                                         \
      ok &= push_token(t, (rd__token){wstart, j, (float)units * scale, RD_TOKEN_WORD}); \
      wstart = UINT32_MAX;                                                              \
      units  = 0;                                                                       \
      prev   = -1;                                                                      \
    }                                                                                   \
  } while (0)

  for (uint32_t j = 0; j < t->cp_count && ok; j++) {
    uint32_t c = t->cps[j];
    if (c == '\n' && !coll) {
      FLUSH_WORD();
      ok &= push_token(t, (rd__token){j, j + 1, 0, RD_TOKEN_NEWLINE});
    } else if (is_space(c)) {
      FLUSH_WORD();
      if (c == '\r' && !coll) continue;
      float      w    = coll ? sw : c == '\t' ? 4 * sw
                                              : sw;
      rd__token* last = t->token_count ? &t->tokens[t->token_count - 1] : NULL;
      if (last && last->kind == RD_TOKEN_SPACE && last->end == j) {
        last->end = j + 1;
        if (!coll) last->width += w;
      } else {
        ok &= push_token(t, (rd__token){j, j + 1, w, RD_TOKEN_SPACE});
      }
    } else {
      int32_t adv, g = rd__font_glyph(f, c, &adv);
      if (is_cjk(c)) {
        FLUSH_WORD();
        ok &= push_token(t, (rd__token){j, j + 1, (float)adv * scale, RD_TOKEN_WORD});
        continue;
      }
      if (wstart == UINT32_MAX) wstart = j;
      units += adv + (prev >= 0 ? rd__font_kern(f, prev, g) : 0);
      prev = g;
    }
  }
  uint32_t j = t->cp_count;
  FLUSH_WORD();
#undef FLUSH_WORD

  t->tok_font  = f;
  t->tok_size  = size;
  t->tok_ws    = ws;
  t->tok_valid = ok;
  return ok;
}

typedef struct break_result {
  uint32_t lines;
  float    max_width;
} break_result;

// Greedy breaking over tokens. A line's tokens run up to the next line's first token, so trailing
// spaces belong to it, but its width excludes them: they hang. Collapsible spaces at a line start
// are dropped.
static break_result break_lines(rd_text* t, float limit, bool wrap, bool collapse, rd__line* out) {
  break_result r    = {0, 0};
  float        line = 0, pending = 0;
  bool         has   = false;
  uint32_t     start = 0;

#define EMIT(stop)                                                                  \
  do {                                                                              \
    if (out) {                                                                      \
      uint32_t cs  = start < t->token_count ? t->tokens[start].start : t->cp_count; \
      uint32_t ce  = (stop) > start ? t->tokens[(stop) - 1].end : cs;               \
      out[r.lines] = (rd__line){start, (stop), line, cs, ce};                       \
    }                                                                               \
    r.lines++;                                                                      \
    r.max_width = fmaxf(r.max_width, line);                                         \
  } while (0)

  for (uint32_t k = 0; k < t->token_count; k++) {
    const rd__token* tok = &t->tokens[k];
    if (tok->kind == RD_TOKEN_NEWLINE) {
      EMIT(k);
      line = pending = 0;
      has            = false;
      start          = k + 1;
    } else if (tok->kind == RD_TOKEN_SPACE) {
      if (!has && collapse)
        start = k + 1;
      else
        pending += tok->width;
    } else {
      if (wrap && has && line + pending + tok->width > limit + EPS) {
        EMIT(k);
        line = pending = 0;
        start          = k;
      }
      line += pending + tok->width;
      pending = 0;
      has     = true;
    }
  }
  bool trailing_newline = t->token_count && t->tokens[t->token_count - 1].kind == RD_TOKEN_NEWLINE;
  if (has || trailing_newline || (!collapse && start < t->token_count)) EMIT(t->token_count);
#undef EMIT
  return r;
}

// Inputs keep every space and newline regardless of style.
static rd_white_space text_ws(const rd_text* t, const rd_style* st) {
  if (!t->input) return st->white_space;
  return rd__input_multiline(t->input) ? RD_WHITE_SPACE_PRE_WRAP : RD_WHITE_SPACE_PRE;
}

static float line_height(const rd_style* st);

float rd__text_line_height(const rd_style* st) { return line_height(st); }

float rd__text_align_offset(const rd_style* st, float width, float line_width) {
  float a = st->text_align == RD_TEXT_ALIGN_CENTER ? 0.5f : st->text_align == RD_TEXT_ALIGN_END ? 1
                                                                                                : 0;
  return fmaxf(0, width - line_width) * a;
}

static float line_height(const rd_style* st) {
  float size = st->font_size.value;
  switch (st->line_height.unit) {
    case RD_UNIT_PX:
      return st->line_height.value;
    case RD_UNIT_PCT:
      return st->line_height.value * size / 100.0f;
    case RD_UNIT_NUM:
      return st->line_height.value * size;
    default:
      return size * 1.2f;
  }
}

static const rd_style* text_style(const rd_text* t) { return &t->ctx->styles[t->node].computed; }

static rd__font* prepare(rd_text* t) {
  const rd_style* st = text_style(t);
  rd__font*       f  = rd__font_get(t->ctx, st->font);
  if (!f || !tokenize(t, f, st->font_size.value, text_ws(t, st))) return NULL;
  return f;
}

static rd_size text_measure(void* data, float w, rd_measure_mode wm, float h, rd_measure_mode hm) {
  (void)h;
  (void)hm;
  rd_text*        t  = data;
  const rd_style* st = text_style(t);
  if (!prepare(t)) return (rd_size){0, t->input ? line_height(st) : 0};
  rd_white_space ws    = text_ws(t, st);
  float          limit = wm == RD_MEASURE_UNDEFINED ? INFINITY : wm == RD_MEASURE_MIN_CONTENT ? 0
                                                                                              : w;
  break_result   r     = break_lines(t, limit, wraps(ws), collapses(ws), NULL);
  uint32_t       clamp = rd__line_clamp(st);
  if (clamp && !t->input && r.lines > clamp) r.lines = clamp;
  if (t->input) {
    if (r.lines == 0) r.lines = 1;
    if (wm == RD_MEASURE_MIN_CONTENT && !rd__input_multiline(t->input)) r.max_width = 0;
  }
  return (rd_size){wm == RD_MEASURE_EXACTLY ? w : r.max_width, (float)r.lines * line_height(st)};
}

static void text_destroy(void* data) {
  rd_text* t = data;
  rd__input_free(t->input);
  free(t->highlights);
  free(t->utf8);
  free(t->cps);
  free(t->offsets);
  free(t->tokens);
  free(t->lines);
  free(t);
}

bool rd__is_text(const rd_ctx* ctx, uint32_t i) { return ctx->layouts[i].measure_fn == text_measure; }

static rd_text* text_of(const rd_ctx* ctx, rd_node node) {
  if (!rd__slot(ctx, node) || !rd__is_text(ctx, node.index)) return NULL;
  return ctx->layouts[node.index].measure_data;
}

bool rd_node_set_text(rd_ctx* ctx, rd_node node, const char* utf8, size_t len) {
  if (!rd__slot(ctx, node)) return false;
  if (!utf8) utf8 = "";
  if (len == RD_TEXT_NUL) len = strlen(utf8);
  if (len >= UINT32_MAX) return false;

  char*     copy    = malloc(len + 1);
  uint32_t* cps     = malloc((len + 1) * sizeof *cps);
  uint32_t* offsets = malloc((len + 1) * sizeof *offsets);
  if (!copy || !cps || !offsets) {
    free(copy);
    free(cps);
    free(offsets);
    return false;
  }
  memcpy(copy, utf8, len);
  copy[len]  = 0;
  uint32_t n = 0;
  for (size_t i = 0; i < len && n < len;) {
    offsets[n] = (uint32_t)i;
    cps[n++]   = rd__utf8_decode(copy, len, &i);
  }
  offsets[n] = (uint32_t)len;

  rd_text* t = text_of(ctx, node);
  if (!t) {
    t = calloc(1, sizeof *t);
    if (!t || !rd_node_set_measure(ctx, node, text_measure, t, text_destroy)) {
      free(t);
      free(copy);
      free(cps);
      free(offsets);
      return false;
    }
    t->ctx  = ctx;
    t->node = node.index;
  }
  if (t->input) rd__input_before_set(t, copy, len);
  free(t->utf8);
  free(t->cps);
  free(t->offsets);
  t->utf8        = copy;
  t->len         = len;
  t->cps         = cps;
  t->offsets     = offsets;
  t->cp_count    = n;
  t->tok_valid   = false;
  t->lines_valid = false;
  if (t->input) rd__input_clamp(t);
  rd__mark_layout_dirty(ctx, node.index);
  return true;
}

const char* rd_node_text(const rd_ctx* ctx, rd_node node, size_t* len) {
  rd_text* t = text_of(ctx, node);
  if (len) *len = t ? t->len : 0;
  return t ? t->utf8 : NULL;
}

static float first_baseline(rd__font* f, const rd_style* st) {
  float ascent, descent, gap;
  rd__font_vmetrics(f, st->font_size.value, &ascent, &descent, &gap);
  return (line_height(st) - (ascent + descent)) / 2 + ascent;
}

float rd__text_baseline(rd_ctx* ctx, uint32_t i) {
  rd_text*  t = ctx->layouts[i].measure_data;
  rd__font* f = rd__font_get(ctx, text_style(t)->font);
  return f ? first_baseline(f, text_style(t)) : NAN;
}

bool rd__text_layout_get(rd_ctx* ctx, uint32_t i, float width, rd__text_layout* out) {
  if (!rd__is_text(ctx, i)) return false;
  rd_text*  t = ctx->layouts[i].measure_data;
  rd__font* f = prepare(t);
  if (!f) return false;
  const rd_style* st   = text_style(t);
  rd_white_space  ws   = text_ws(t, st);
  bool            coll = collapses(ws);
  if (!t->lines_valid || t->lines_width != width) {
    bool         wrap = wraps(ws);
    break_result r    = break_lines(t, width, wrap, coll, NULL);
    uint32_t     need = r.lines ? r.lines : 1;
    if (need > t->line_cap) {
      rd__line* p = realloc(t->lines, need * sizeof *p);
      if (!p) return false;
      t->lines    = p;
      t->line_cap = need;
    }
    break_lines(t, width, wrap, coll, t->lines);
    if (!r.lines && t->input) {
      t->lines[0] = (rd__line){0, 0, 0, 0, 0};
      r.lines     = 1;
    }
    t->line_count  = r.lines;
    t->lines_width = width;
    t->lines_valid = true;
  }
  *out = (rd__text_layout){
      .cps         = t->cps,
      .offsets     = t->offsets,
      .tokens      = t->tokens,
      .lines       = t->lines,
      .line_count  = t->line_count,
      .font        = f,
      .size        = st->font_size.value,
      .line_height = line_height(st),
      .baseline    = first_baseline(f, st),
      .units       = rd__font_scale(f, st->font_size.value),
      .space_width = space_width(f, rd__font_scale(f, st->font_size.value)),
      .cp_count    = t->cp_count,
      .collapse    = coll,
  };
  return true;
}

size_t rd__text_byte_at(rd_ctx* ctx, uint32_t i, float x, float y, float dx) {
  rd_text* t = rd__text_get(ctx, i);
  if (!t) return 0;
  const rd_node_layout* L     = &ctx->layouts[i];
  const rd_style*       st    = &ctx->styles[i].computed;
  float                 width = fmaxf(0, L->rect.width - L->border[EDGE_L] - L->border[EDGE_R] - L->padding[EDGE_L] -
                                             L->padding[EDGE_R]);
  rd__text_layout       tl;
  if (!rd__text_layout_get(ctx, i, width, &tl) || !tl.line_count) return 0;
  rd_rect b     = rd_node_bounds(ctx, rd__handle(ctx, i));
  float   lx    = x - b.x - L->border[EDGE_L] - L->padding[EDGE_L] + dx;
  float   ly    = y - b.y - L->border[EDGE_T] - L->padding[EDGE_T];
  int     shown = (int)rd__text_visible_lines(st, &tl);
  int     line  = tl.line_height > 0 ? (int)fminf(floorf(ly / tl.line_height), (float)INT32_MAX) : 0;  // a 0px line height stacks every line at the top
  if (line < 0) return 0;
  if (line >= shown) return shown < (int)tl.line_count ? t->offsets[tl.lines[shown - 1].cp_end] : t->len;
  float off = rd__text_align_offset(st, width, tl.lines[line].width);
  return t->offsets[rd__text_cp_at(&tl, (uint32_t)line, lx - off)];
}

// line_clamp as a count; values beyond uint32_t mean no limit to speak of.
uint32_t rd__line_clamp(const rd_style* st) {
  float v = st->line_clamp.value;
  return v > 0 ? (v < (float)UINT32_MAX ? (uint32_t)v : UINT32_MAX) : 0;
}

uint32_t rd__text_visible_lines(const rd_style* st, const rd__text_layout* tl) {
  uint32_t clamp = rd__line_clamp(st);
  return clamp && clamp < tl->line_count ? clamp : tl->line_count;
}

bool rd_node_set_highlights(rd_ctx* ctx, rd_node node, const rd_highlight* ranges, size_t count) {
  rd_text* t = text_of(ctx, node);
  if (!t) return false;
  rd_highlight* copy = NULL;
  if (ranges && count) {
    copy = malloc(count * sizeof *copy);
    if (!copy) return false;
    memcpy(copy, ranges, count * sizeof *copy);
  }
  free(t->highlights);
  t->highlights      = copy;
  t->highlight_count = copy ? count : 0;
  ctx->paint_dirty   = true;
  return true;
}

rd_text* rd__text_get(const rd_ctx* ctx, uint32_t i) {
  return rd__is_text(ctx, i) ? ctx->layouts[i].measure_data : NULL;
}

uint32_t rd__text_cp_of_byte(const rd_text* t, size_t byte) {
  uint32_t lo = 0, hi = t->cp_count;
  while (lo < hi) {
    uint32_t mid = (lo + hi) / 2;
    if (t->offsets[mid] < byte)
      lo = mid + 1;
    else
      hi = mid;
  }
  return lo;
}

// Calls fn for each codepoint of a line with its x offset and advance, in order.
typedef bool (*cp_visit)(void* user, uint32_t cp, float x, float w);

static void walk_line(const rd__text_layout* tl, uint32_t li, cp_visit fn, void* user) {
  const rd__line* ln = &tl->lines[li];
  float           x  = 0;
  for (uint32_t k = ln->tok_start; k < ln->tok_end; k++) {
    const rd__token* tok = &tl->tokens[k];
    if (tok->kind == RD_TOKEN_NEWLINE) return;
    int32_t prev = -1;
    for (uint32_t j = tok->start; j < tok->end; j++) {
      float w;
      if (tok->kind == RD_TOKEN_SPACE) {
        w = tl->collapse ? (j == tok->start ? tok->width : 0) : tl->cps[j] == '\t' ? 4 * tl->space_width
                                                                                   : tl->space_width;
      } else {
        int32_t adv, g = rd__font_glyph(tl->font, tl->cps[j], &adv);
        w    = ((float)adv + (prev >= 0 ? (float)rd__font_kern(tl->font, prev, g) : 0)) * tl->units;
        prev = g;
      }
      if (!fn(user, j, x, w)) return;
      x += w;
    }
  }
}

typedef struct find_x {
  uint32_t cp;
  float    x;
} find_x;

static bool visit_x(void* user, uint32_t cp, float x, float w) {
  find_x* f = user;
  if (cp >= f->cp) return false;
  f->x = x + w;
  return true;
}

float rd__text_cp_x(const rd__text_layout* tl, uint32_t line, uint32_t cp) {
  if (line >= tl->line_count) return 0;
  find_x f = {cp, 0};
  walk_line(tl, line, visit_x, &f);
  return f.x;
}

// By binary search: lines start in order.
uint32_t rd__text_line_of(const rd__text_layout* tl, uint32_t cp) {
  uint32_t lo = 0, hi = tl->line_count;  // the first line starting after cp
  while (lo < hi) {
    uint32_t mid = lo + (hi - lo) / 2;
    if (tl->lines[mid].cp_start <= cp)
      lo = mid + 1;
    else
      hi = mid;
  }
  return lo ? lo - 1 : 0;
}

typedef struct find_cp {
  float    x;
  uint32_t cp;
  bool     found;
} find_cp;

static bool visit_cp(void* user, uint32_t cp, float x, float w) {
  find_cp* f = user;
  if (f->x < x + w / 2) {
    f->cp    = cp;
    f->found = true;
    return false;
  }
  return true;
}

uint32_t rd__text_cp_at(const rd__text_layout* tl, uint32_t line, float x) {
  if (line >= tl->line_count) return tl->cp_count;
  find_cp f = {x, 0, false};
  walk_line(tl, line, visit_cp, &f);
  return f.found ? f.cp : tl->lines[line].cp_end;
}

float rd_node_baseline(const rd_ctx* ctx, rd_node node) {
  return rd__slot(ctx, node) ? ctx->layouts[node.index].baseline : NAN;
}
