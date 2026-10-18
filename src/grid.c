#include <stdlib.h>
#include <string.h>

#include "layout_internal.h"

enum { FN_FIXED,
       FN_AUTO,
       FN_MIN_CONTENT,
       FN_MAX_CONTENT,
       FN_FLEX };

typedef enum sizing { SIZE_DEFINITE,
                      SIZE_MAX_CONTENT,
                      SIZE_MIN_CONTENT } sizing;

typedef struct grid_track {
  uint8_t min_fn, max_fn;
  float   min_v, max_v;  // px for FN_FIXED, factor for FN_FLEX
  float   base, limit;
  bool    limit_set;
  bool    inflexible;
  float   pos;
} grid_track;

typedef struct grid_item {
  uint32_t        idx;
  const rd_style* st;
  float           margin[4];
  bool            auto_margin[4];
  float           pb[2], min[2], max[2], size_def[2];
  int32_t         start[2], span[2];
  bool            definite[2];
  rd_align        align[2];
  float           contrib_min, contrib_max;
  float           area[2], size[2], pos[2];
} grid_item;

static bool is_intrinsic(uint8_t fn) { return fn == FN_AUTO || fn == FN_MIN_CONTENT || fn == FN_MAX_CONTENT; }

static void classify(rd_val v, float ref, bool allow_flex, uint8_t* fn, float* out) {
  *out = 0;
  switch (v.unit) {
    case RD_UNIT_PX:
      *fn  = FN_FIXED;
      *out = fmaxf(0, v.value);
      return;
    case RD_UNIT_PCT:
      if (isnan(ref)) break;
      *fn  = FN_FIXED;
      *out = fmaxf(0, v.value * ref / 100.0f);
      return;
    case RD_UNIT_FR:
      if (!allow_flex) break;
      *fn  = FN_FLEX;
      *out = fmaxf(0, v.value);
      return;
    case RD_UNIT_MIN_CONTENT:
      *fn = FN_MIN_CONTENT;
      return;
    case RD_UNIT_MAX_CONTENT:
      *fn = FN_MAX_CONTENT;
      return;
    default:
      break;
  }
  *fn = FN_AUTO;
}

// Placement.

static bool    is_line(rd_grid_line l) { return l.kind == RD_GRID_LINE_INDEX && l.value != 0; }
static int32_t span_of(rd_grid_line l) { return l.kind == RD_GRID_LINE_SPAN && l.value > 0 ? l.value : 1; }

// Lines are numbered 1..count+1; returns the 0-based boundary index.
static int32_t line_index(int16_t v, int32_t count) {
  int32_t b = v > 0 ? v - 1 : count + 1 + v;
  return b < 0 ? 0 : b;
}

static void resolve_span(rd_grid_line s, rd_grid_line e, int32_t count, int32_t* start, int32_t* span, bool* definite) {
  *definite = true;
  if (is_line(s) && is_line(e)) {
    int32_t a = line_index(s.value, count), b = line_index(e.value, count);
    if (a > b) {
      int32_t t = a;
      a         = b;
      b         = t;
    }
    *start = a;
    *span  = b > a ? b - a : 1;
  } else if (is_line(s)) {
    *start = line_index(s.value, count);
    *span  = span_of(e);
  } else if (is_line(e)) {
    int32_t b = line_index(e.value, count), a = b - span_of(s);
    if (a < 0) a = 0;
    *start = a;
    *span  = b > a ? b - a : 1;
  } else {
    *definite = false;
    *start    = 0;
    *span     = s.kind == RD_GRID_LINE_SPAN ? span_of(s) : span_of(e);
  }
}

typedef struct occupancy {
  uint8_t* cells;  // major-major order: cells[major * minor_count + minor]
  int32_t  minor_count, major_cap;
} occupancy;

static bool occ_reserve(occupancy* o, int32_t major) {
  if (major <= o->major_cap) return true;
  int32_t cap = o->major_cap ? o->major_cap : 8;
  while (cap < major) cap *= 2;
  uint8_t* c = realloc(o->cells, (size_t)cap * (size_t)o->minor_count);
  if (!c) return false;
  memset(c + (size_t)o->major_cap * (size_t)o->minor_count, 0, (size_t)(cap - o->major_cap) * (size_t)o->minor_count);
  o->cells     = c;
  o->major_cap = cap;
  return true;
}

static bool occ_free(const occupancy* o, int32_t maj, int32_t maj_span, int32_t min, int32_t min_span) {
  if (min + min_span > o->minor_count) return false;
  if (!o->cells) return true;
  for (int32_t r = maj; r < maj + maj_span && r < o->major_cap; r++)
    for (int32_t c = min; c < min + min_span; c++)
      if (o->cells[(size_t)r * (size_t)o->minor_count + (size_t)c]) return false;
  return true;
}

static bool occ_mark(occupancy* o, int32_t maj, int32_t maj_span, int32_t min, int32_t min_span) {
  if (!occ_reserve(o, maj + maj_span)) return false;
  for (int32_t r = maj; r < maj + maj_span; r++)
    for (int32_t c = min; c < min + min_span && c < o->minor_count; c++)
      o->cells[(size_t)r * (size_t)o->minor_count + (size_t)c] = 1;
  return true;
}

// CSS Grid §8.5, sparse packing.
static bool auto_place(grid_item* items, uint32_t n, int major, const int32_t explicit_count[2]) {
  int       minor = 1 - major;
  occupancy o     = {NULL, explicit_count[minor], 0};
  for (uint32_t j = 0; j < n; j++) {
    const grid_item* it  = &items[j];
    int32_t          end = it->definite[minor] ? it->start[minor] + it->span[minor] : it->span[minor];
    if (end > o.minor_count) o.minor_count = end;
  }
  if (o.minor_count == 0) o.minor_count = 1;

  bool ok = true;
  for (uint32_t j = 0; j < n && ok; j++) {
    grid_item* it = &items[j];
    if (it->definite[0] && it->definite[1]) ok = occ_mark(&o, it->start[major], it->span[major], it->start[minor], it->span[minor]);
  }
  for (uint32_t j = 0; j < n && ok; j++) {
    grid_item* it = &items[j];
    if (!it->definite[major] || it->definite[minor]) continue;
    int32_t c = 0;
    while (c + it->span[minor] <= o.minor_count && !occ_free(&o, it->start[major], it->span[major], c, it->span[minor])) c++;
    if (c + it->span[minor] > o.minor_count) c = 0;
    it->start[minor]    = c;
    it->definite[minor] = true;
    ok                  = occ_mark(&o, it->start[major], it->span[major], c, it->span[minor]);
  }
  int32_t cur_maj = 0, cur_min = 0;
  for (uint32_t j = 0; j < n && ok; j++) {
    grid_item* it = &items[j];
    if (it->definite[major]) continue;
    if (it->definite[minor]) {
      if (it->start[minor] < cur_min) cur_maj++;
      cur_min = it->start[minor];
      while (!occ_free(&o, cur_maj, it->span[major], cur_min, it->span[minor])) cur_maj++;
    } else {
      for (;;) {
        if (cur_min + it->span[minor] > o.minor_count) {
          cur_maj++;
          cur_min = 0;
        }
        if (occ_free(&o, cur_maj, it->span[major], cur_min, it->span[minor])) break;
        cur_min++;
      }
      it->start[minor] = cur_min;
    }
    it->start[major]    = cur_maj;
    it->definite[major] = it->definite[minor] = true;
    ok                                        = occ_mark(&o, cur_maj, it->span[major], it->start[minor], it->span[minor]);
    cur_min                                   = it->start[minor] + it->span[minor];
  }
  free(o.cells);
  return ok;
}

// Track sizing.

static void init_tracks(grid_track* t, uint32_t nt, const rd_track* list, uint32_t explicit_count, rd_track implicit,
                        float inner) {
  for (uint32_t j = 0; j < nt; j++) {
    rd_track d = j < explicit_count ? list[j] : implicit;
    classify(d.min, inner, false, &t[j].min_fn, &t[j].min_v);
    classify(d.max, inner, true, &t[j].max_fn, &t[j].max_v);
    t[j].base       = t[j].min_fn == FN_FIXED ? t[j].min_v : 0;
    t[j].limit_set  = t[j].max_fn == FN_FIXED;
    t[j].limit      = t[j].limit_set ? fmaxf(t[j].max_v, t[j].base) : INFINITY;
    t[j].inflexible = false;
  }
}

static float spanned_gaps(int32_t span, float gap) { return gap * (float)(span - 1); }

static bool spans_flex(const grid_track* t, const grid_item* it, int a) {
  for (int32_t j = it->start[a]; j < it->start[a] + it->span[a]; j++)
    if (t[j].max_fn == FN_FLEX) return true;
  return false;
}

static float effective_limit(const grid_track* t) { return t->limit_set ? fmaxf(t->limit, t->base) : t->base; }

static void grow_limit(grid_track* t, float v) {
  t->limit     = t->limit_set ? fmaxf(t->limit, v) : v;
  t->limit_set = true;
}

// CSS Grid §11.5: intrinsic track sizes.
static void size_intrinsic(grid_track* t, uint32_t nt, const grid_item* items, uint32_t n, int a, sizing mode, float gap) {
  int32_t max_span = 1;
  for (uint32_t j = 0; j < n; j++) {
    const grid_item* it = &items[j];
    if (it->span[a] > max_span) max_span = it->span[a];
    if (it->span[a] != 1) continue;
    grid_track* tr = &t[it->start[a]];
    if (tr->min_fn == FN_AUTO)
      tr->base = fmaxf(tr->base, mode == SIZE_MAX_CONTENT ? it->contrib_max : it->contrib_min);
    else if (tr->min_fn == FN_MIN_CONTENT)
      tr->base = fmaxf(tr->base, it->contrib_min);
    else if (tr->min_fn == FN_MAX_CONTENT)
      tr->base = fmaxf(tr->base, it->contrib_max);
    if (tr->max_fn == FN_MIN_CONTENT)
      grow_limit(tr, it->contrib_min);
    else if (tr->max_fn == FN_AUTO || tr->max_fn == FN_MAX_CONTENT)
      grow_limit(tr, it->contrib_max);
  }

  for (int32_t span = 2; span <= max_span; span++) {
    for (uint32_t j = 0; j < n; j++) {
      const grid_item* it = &items[j];
      if (it->span[a] != span || spans_flex(t, it, a)) continue;
      int32_t  s0 = it->start[a], s1 = it->start[a] + span;
      float    sum = spanned_gaps(span, gap);
      uint32_t cnt = 0;
      for (int32_t q = s0; q < s1; q++) {
        sum += t[q].base;
        cnt += is_intrinsic(t[q].min_fn);
      }
      float extra = (mode == SIZE_MAX_CONTENT ? it->contrib_max : it->contrib_min) - sum;
      for (int32_t q = s0; q < s1 && extra > 0; q++)
        if (is_intrinsic(t[q].min_fn)) t[q].base += extra / (float)cnt;

      sum = spanned_gaps(span, gap);
      cnt = 0;
      for (int32_t q = s0; q < s1; q++) {
        sum += effective_limit(&t[q]);
        cnt += is_intrinsic(t[q].max_fn);
      }
      extra = it->contrib_max - sum;
      for (int32_t q = s0; q < s1 && extra > 0; q++)
        if (is_intrinsic(t[q].max_fn)) grow_limit(&t[q], effective_limit(&t[q]) + extra / (float)cnt);
    }
  }

  for (uint32_t j = 0; j < nt; j++) {
    if (t[j].max_fn == FN_FLEX) continue;
    if (!t[j].limit_set) t[j].limit = t[j].base;
    if (t[j].limit < t[j].base) t[j].limit = t[j].base;
  }
}

// CSS Grid §11.7: fr tracks.
static void expand_flexible(grid_track* t, uint32_t nt, const grid_item* items, uint32_t n, int a, float inner,
                            sizing mode, float gap) {
  bool has_flex = false;
  for (uint32_t j = 0; j < nt; j++) has_flex |= t[j].max_fn == FN_FLEX;
  if (!has_flex || mode == SIZE_MIN_CONTENT) return;

  float fr = 0;
  if (mode == SIZE_DEFINITE) {
    for (;;) {
      float left = inner - spanned_gaps((int32_t)nt, gap), sum_f = 0;
      for (uint32_t j = 0; j < nt; j++) {
        if (t[j].max_fn == FN_FLEX && !t[j].inflexible)
          sum_f += t[j].max_v;
        else
          left -= t[j].base;
      }
      fr           = fmaxf(0, left) / fmaxf(sum_f, 1);
      bool changed = false;
      for (uint32_t j = 0; j < nt; j++) {
        if (t[j].max_fn != FN_FLEX || t[j].inflexible || fr * t[j].max_v >= t[j].base) continue;
        t[j].inflexible = true;
        changed         = true;
      }
      if (!changed) break;
    }
  } else {
    for (uint32_t j = 0; j < nt; j++)
      if (t[j].max_fn == FN_FLEX) fr = fmaxf(fr, t[j].max_v > 1 ? t[j].base / t[j].max_v : t[j].base);
    for (uint32_t j = 0; j < n; j++) {
      const grid_item* it = &items[j];
      if (!spans_flex(t, it, a)) continue;
      float c = it->contrib_max - spanned_gaps(it->span[a], gap), f = 0;
      for (int32_t k = it->start[a]; k < it->start[a] + it->span[a]; k++) {
        if (t[k].max_fn == FN_FLEX)
          f += t[k].max_v;
        else
          c -= t[k].base;
      }
      fr = fmaxf(fr, c / fmaxf(f, 1));
    }
  }
  for (uint32_t j = 0; j < nt; j++)
    if (t[j].max_fn == FN_FLEX && !t[j].inflexible) t[j].base = fmaxf(t[j].base, fr * t[j].max_v);
}

static float tracks_total(const grid_track* t, uint32_t nt, float gap) {
  float sum = nt ? spanned_gaps((int32_t)nt, gap) : 0;
  for (uint32_t j = 0; j < nt; j++) sum += t[j].base;
  return sum;
}

static float size_tracks(grid_track* t, uint32_t nt, const grid_item* items, uint32_t n, int a, float inner, sizing mode,
                         float gap, rd_align content_align) {
  size_intrinsic(t, nt, items, n, a, mode, gap);

  // §11.6: maximize tracks.
  if (mode == SIZE_MAX_CONTENT) {
    for (uint32_t j = 0; j < nt; j++)
      if (t[j].max_fn != FN_FLEX) t[j].base = t[j].limit;
  } else if (mode == SIZE_DEFINITE) {
    float free = inner - tracks_total(t, nt, gap);
    while (free > EPS) {
      uint32_t cnt = 0;
      for (uint32_t j = 0; j < nt; j++) cnt += t[j].max_fn != FN_FLEX && t[j].base < t[j].limit - EPS;
      if (!cnt) break;
      float share = free / (float)cnt;
      for (uint32_t j = 0; j < nt; j++) {
        if (t[j].max_fn == FN_FLEX || t[j].base >= t[j].limit - EPS) continue;
        float add = fminf(share, t[j].limit - t[j].base);
        t[j].base += add;
        free -= add;
      }
    }
  }

  expand_flexible(t, nt, items, n, a, inner, mode, gap);

  // §11.8: stretch auto tracks.
  if (mode == SIZE_DEFINITE && (content_align == RD_ALIGN_NORMAL || content_align == RD_ALIGN_STRETCH)) {
    float    free = inner - tracks_total(t, nt, gap);
    uint32_t cnt  = 0;
    for (uint32_t j = 0; j < nt; j++) cnt += t[j].max_fn == FN_AUTO;
    if (free > EPS && cnt)
      for (uint32_t j = 0; j < nt; j++)
        if (t[j].max_fn == FN_AUTO) t[j].base += free / (float)cnt;
  }
  return tracks_total(t, nt, gap);
}

// Sizes one axis to its final inner size. An indefinite axis is sized to content first, then
// clamped and re-run at the clamped size when that differs.
static float size_axis(grid_track* t, uint32_t nt, const rd_track* list, uint32_t explicit_count, rd_track implicit,
                       const grid_item* items, uint32_t n, int a, const rd_layout_key* k, const float pb[2],
                       const float mn[2], const float mx[2], float gap, rd_align content_align) {
  float           avail = a ? k->ah : k->aw;
  rd_measure_mode mode  = a ? k->hm : k->wm;
  if (mode == RD_MEASURE_EXACTLY) {
    float inner = fmaxf(0, avail - pb[a]);
    init_tracks(t, nt, list, explicit_count, implicit, inner);
    size_tracks(t, nt, items, n, a, inner, SIZE_DEFINITE, gap, content_align);
    return inner;
  }
  sizing s = mode == RD_MEASURE_MIN_CONTENT ? SIZE_MIN_CONTENT : SIZE_MAX_CONTENT;
  init_tracks(t, nt, list, explicit_count, implicit, NAN);
  float total = size_tracks(t, nt, items, n, a, NAN, s, gap, content_align);
  float inner = finalize_axis(total + pb[a], mode, avail, mn[a], mx[a]) - pb[a];
  if (fabsf(inner - total) > EPS) {
    init_tracks(t, nt, list, explicit_count, implicit, inner);
    size_tracks(t, nt, items, n, a, inner, SIZE_DEFINITE, gap, content_align);
  }
  return inner;
}

static void position_tracks(grid_track* t, uint32_t nt, float inner, float gap, rd_align align, float origin) {
  float off, between;
  rd__justify_offsets(align, inner - tracks_total(t, nt, gap), nt, &off, &between);
  float p = origin + off;
  for (uint32_t j = 0; j < nt; j++) {
    t[j].pos = p;
    p += t[j].base + gap + between;
  }
}

static float area_size(const grid_track* t, const grid_item* it, int a) {
  int32_t last = it->start[a] + it->span[a] - 1;
  return t[last].pos + t[last].base - t[it->start[a]].pos;
}

// Item metrics against its grid area; NaN refs while the area is unknown.
static void resolve_item(grid_item* it, float cbw, float cbh) {
  const rd_style* cs = it->st;
  resolve_margins(&cs->margin, cbw, it->margin, it->auto_margin);
  style_pb(cs, cbw, it->pb);
  size_bounds(cs, it->pb, cbw, cbh, it->min, it->max);
  it->size_def[0] = resolve(cs->width, cbw);
  it->size_def[1] = resolve(cs->height, cbh);
}

static float margins(const grid_item* it, int a) { return it->margin[AX_START[a]] + it->margin[AX_END[a]]; }

static bool has_auto_margin(const grid_item* it, int a) {
  return it->auto_margin[AX_START[a]] || it->auto_margin[AX_END[a]];
}

static rd_layout_key item_key(const grid_item* it, int a, float size, rd_measure_mode mode, const float cb[2]) {
  int             o  = 1 - a;
  float           ov = it->size_def[o];
  rd_measure_mode om = RD_MEASURE_UNDEFINED;
  if (!isnan(ov)) {
    ov = clampf(ov, it->min[o], it->max[o]);
    om = RD_MEASURE_EXACTLY;
  } else if (!isnan(it->size[o])) {
    ov = it->size[o];
    om = RD_MEASURE_EXACTLY;
  }
  rd_layout_key k = {.cbw = cb[0], .cbh = cb[1]};
  if (a == 0) {
    k.aw = size, k.wm = mode, k.ah = ov, k.hm = om;
  } else {
    k.ah = size, k.hm = mode, k.aw = ov, k.wm = om;
  }
  return k;
}

static void contributions(rd_ctx* ctx, grid_item* it, int a, const float cb[2]) {
  float m = margins(it, a);
  if (!isnan(it->size_def[a])) {
    it->contrib_min = it->contrib_max = clampf(it->size_def[a], it->min[a], it->max[a]) + m;
    return;
  }
  rd_size mx      = rd__layout_node(ctx, it->idx, item_key(it, a, NAN, RD_MEASURE_UNDEFINED, cb), false);
  rd_size mn      = a == 0 ? rd__layout_node(ctx, it->idx, item_key(it, a, NAN, RD_MEASURE_MIN_CONTENT, cb), false) : mx;
  it->contrib_max = clampf(a ? mx.height : mx.width, it->min[a], it->max[a]) + m;
  it->contrib_min = clampf(a ? mn.height : mn.width, it->min[a], it->max[a]) + m;
}

static void size_item(rd_ctx* ctx, grid_item* it, int a, const float cb[2]) {
  float avail = fmaxf(0, it->area[a] - margins(it, a));
  if (!isnan(it->size_def[a])) {
    it->size[a] = clampf(it->size_def[a], it->min[a], it->max[a]);
  } else if (it->align[a] == RD_ALIGN_STRETCH && !has_auto_margin(it, a)) {
    it->size[a] = clampf(avail, it->min[a], it->max[a]);
  } else {
    rd_size r   = rd__layout_node(ctx, it->idx, item_key(it, a, avail, RD_MEASURE_AT_MOST, cb), false);
    it->size[a] = clampf(a ? r.height : r.width, it->min[a], it->max[a]);
  }
}

static float item_offset(const grid_item* it, int a) {
  float free = it->area[a] - it->size[a] - margins(it, a);
  bool  s = it->auto_margin[AX_START[a]], e = it->auto_margin[AX_END[a]];
  if (s || e) return free > 0 ? (s && e ? free / 2 : s ? free
                                                       : 0)
                              : 0;
  if (it->align[a] == RD_ALIGN_END) return free;
  if (it->align[a] == RD_ALIGN_CENTER) return free / 2;
  return 0;
}

rd_size rd__layout_grid(rd_ctx* ctx, uint32_t i, const rd_layout_key* k, bool perform) {
  rd_slot*        s  = &ctx->slots[i];
  rd_node_layout* L  = &ctx->layouts[i];
  const rd_style* st = &ctx->styles[i].computed;

  float pb_start[2] = {L->padding[EDGE_L] + L->border[EDGE_L], L->padding[EDGE_T] + L->border[EDGE_T]};
  float pb[2]       = {
      pb_start[0] + L->padding[EDGE_R] + L->border[EDGE_R],
      pb_start[1] + L->padding[EDGE_B] + L->border[EDGE_B],
  };
  float mn[2], mx[2];
  size_bounds(st, pb, k->cbw, k->cbh, mn, mx);
  float definite[2] = {
      k->wm == RD_MEASURE_EXACTLY ? fmaxf(0, k->aw - pb[0]) : NAN,
      k->hm == RD_MEASURE_EXACTLY ? fmaxf(0, k->ah - pb[1]) : NAN,
  };
  float gap[2] = {resolve0(st->column_gap, definite[0]), resolve0(st->row_gap, definite[1])};

  uint32_t        explicit_count[2];
  const rd_track* list[2] = {
      rd_tracks_get(ctx, st->grid_template_columns, &explicit_count[0]),
      rd_tracks_get(ctx, st->grid_template_rows, &explicit_count[1]),
  };
  rd_track implicit[2] = {st->grid_auto_columns, st->grid_auto_rows};

  uint32_t n = 0;
  for (uint32_t c = s->first_child; c; c = ctx->slots[c].next_sibling) n += in_flow(&ctx->styles[c].computed);

  grid_item* items = n ? calloc(n, sizeof *items) : NULL;
  uint32_t   m     = 0;
  for (uint32_t c = s->first_child; c && items && m < n; c = ctx->slots[c].next_sibling) {
    const rd_style* cs = &ctx->styles[c].computed;
    if (!in_flow(cs)) continue;
    grid_item* it = &items[m++];
    it->idx       = c;
    it->st        = cs;
    it->align[0]  = item_align(cs->justify_self, st->justify_items);
    it->align[1]  = item_align(cs->align_self, st->align_items);
    it->size[0] = it->size[1] = NAN;
    resolve_span(cs->grid_column_start, cs->grid_column_end, (int32_t)explicit_count[0], &it->start[0], &it->span[0],
                 &it->definite[0]);
    resolve_span(cs->grid_row_start, cs->grid_row_end, (int32_t)explicit_count[1], &it->start[1], &it->span[1],
                 &it->definite[1]);
    resolve_item(it, NAN, NAN);
  }

  int32_t  ecount[2] = {(int32_t)explicit_count[0], (int32_t)explicit_count[1]};
  int      major     = st->grid_auto_flow == RD_GRID_FLOW_COLUMN ? 0 : 1;
  uint32_t nt[2]     = {explicit_count[0], explicit_count[1]};
  bool     ok        = !n || (items && auto_place(items, n, major, ecount));
  for (uint32_t j = 0; ok && j < n; j++)
    for (int a = 0; a < 2; a++)
      if ((uint32_t)(items[j].start[a] + items[j].span[a]) > nt[a]) nt[a] = (uint32_t)(items[j].start[a] + items[j].span[a]);

  grid_track* tracks[2] = {
      nt[0] ? calloc(nt[0], sizeof(grid_track)) : NULL,
      nt[1] ? calloc(nt[1], sizeof(grid_track)) : NULL,
  };
  if (!ok || (nt[0] && !tracks[0]) || (nt[1] && !tracks[1])) {
    free(items);
    free(tracks[0]);
    free(tracks[1]);
    return (rd_size){finalize_axis(pb[0], k->wm, k->aw, mn[0], mx[0]), finalize_axis(pb[1], k->hm, k->ah, mn[1], mx[1])};
  }

  rd_align content_align[2] = {st->justify_content, st->align_content};
  float    inner[2];
  for (int a = 0; a < 2; a++) {
    for (uint32_t j = 0; j < n; j++) {
      float cb[2] = {a ? items[j].area[0] : NAN, NAN};
      contributions(ctx, &items[j], a, cb);
    }
    inner[a] = size_axis(tracks[a], nt[a], list[a], explicit_count[a], implicit[a], items, n, a, k, pb, mn, mx, gap[a],
                         content_align[a]);
    position_tracks(tracks[a], nt[a], inner[a], gap[a], content_align[a], pb_start[a]);
    for (uint32_t j = 0; j < n; j++) {
      grid_item* it = &items[j];
      it->area[a]   = area_size(tracks[a], it, a);
      resolve_item(it, it->area[0], a ? it->area[1] : NAN);
      float cb[2] = {it->area[0], a ? it->area[1] : NAN};
      size_item(ctx, it, a, cb);
      it->pos[a] = tracks[a][it->start[a]].pos + it->margin[AX_START[a]] + item_offset(it, a);
    }
  }

  rd_size size = {inner[0] + pb[0], inner[1] + pb[1]};
  if (perform) {
    float ext[2] = {0, 0};
    for (uint32_t j = 0; j < n; j++) {
      grid_item* it = &items[j];
      rd__layout_node(ctx, it->idx,
                      (rd_layout_key){it->size[0], it->size[1], it->area[0], it->area[1], RD_MEASURE_EXACTLY,
                                      RD_MEASURE_EXACTLY},
                      true);
      rd_node_layout* CL = &ctx->layouts[it->idx];
      CL->rect.x         = it->pos[0];
      CL->rect.y         = it->pos[1];
      memcpy(CL->margin, it->margin, sizeof it->margin);
      rd__apply_relative(it->st, CL, it->area);
      ext[0] = fmaxf(ext[0], it->pos[0] + it->size[0] + it->margin[EDGE_R]);
      ext[1] = fmaxf(ext[1], it->pos[1] + it->size[1] + it->margin[EDGE_B]);
    }
    if (n) L->baseline = ctx->layouts[items[0].idx].rect.y + ctx->layouts[items[0].idx].baseline;
    float sz[2] = {size.width, size.height};
    for (uint32_t c = s->first_child; c; c = ctx->slots[c].next_sibling) {
      const rd_style* cs = &ctx->styles[c].computed;
      if (cs->display == RD_DISPLAY_NONE)
        ctx->layouts[c].rect = (rd_rect){0};
      else if (cs->position == RD_POSITION_ABSOLUTE)
        rd__layout_absolute(ctx, c, L, sz);
    }
    L->content.width  = fmaxf(ext[0] + L->padding[EDGE_R], sz[0] - L->border[EDGE_R]) - L->border[EDGE_L];
    L->content.height = fmaxf(ext[1] + L->padding[EDGE_B], sz[1] - L->border[EDGE_B]) - L->border[EDGE_T];
    L->rect.width     = sz[0];
    L->rect.height    = sz[1];
    rd__clamp_scroll(L);
  }

  free(items);
  free(tracks[0]);
  free(tracks[1]);
  return size;
}
