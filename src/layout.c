#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "layout_internal.h"
#include "text_internal.h"

static bool feq(float a, float b) { return a == b || (isnan(a) && isnan(b)); }

static bool key_eq(const rd_layout_key* a, const rd_layout_key* b) {
  return a->wm == b->wm && a->hm == b->hm && feq(a->aw, b->aw) && feq(a->ah, b->ah) && feq(a->cbw, b->cbw) &&
         feq(a->cbh, b->cbh);
}

static rd_size layout_leaf(rd_ctx* ctx, uint32_t i, const rd_layout_key* k) {
  rd_node_layout* L  = &ctx->layouts[i];
  const rd_style* st = &ctx->styles[i].computed;
  float           pb[2], mn[2], mx[2];
  style_pb(st, k->cbw, pb);
  size_bounds(st, pb, k->cbw, k->cbh, mn, mx);
  if (k->wm == RD_MEASURE_EXACTLY && k->hm == RD_MEASURE_EXACTLY) return (rd_size){k->aw, k->ah};

  float           avail[2] = {k->aw, k->ah};
  rd_measure_mode mode[2]  = {k->wm, k->hm};
  float           inner[2];
  for (int a = 0; a < 2; a++) {
    bool sized = mode[a] == RD_MEASURE_EXACTLY || mode[a] == RD_MEASURE_AT_MOST;
    inner[a]   = sized ? fmaxf(0, avail[a] - pb[a]) : NAN;
    if (mode[a] != RD_MEASURE_EXACTLY && mode[a] != RD_MEASURE_MIN_CONTENT && isfinite(mx[a])) {
      float cap = fmaxf(0, mx[a] - pb[a]);
      inner[a]  = mode[a] == RD_MEASURE_UNDEFINED ? cap : fminf(inner[a], cap);
      mode[a]   = RD_MEASURE_AT_MOST;
    }
  }
  rd_size m = L->measure_fn(L->measure_data, inner[0], mode[0], inner[1], mode[1]);
  return (rd_size){
      finalize_axis(m.width + pb[0], k->wm, k->aw, mn[0], mx[0]),
      finalize_axis(m.height + pb[1], k->hm, k->ah, mn[1], mx[1]),
  };
}

typedef struct flex_item {
  uint32_t        idx;
  const rd_style* st;
  float           margin[4];
  bool            auto_margin[4];
  float           pb[2];
  float           min[2], max[2];
  float           size_def[2];
  float           grow, shrink;
  float           base, hyp, target, cross;
  float           violation;
  float           baseline;  // from the cross-start margin edge
  float           pos[2];
  rd_align        align;
  bool            frozen;
} flex_item;

typedef struct flex_line {
  uint32_t start, end;
  float    cross, pos, baseline;
} flex_line;

static float outer_main(const flex_item* it, int ma, float size) {
  return size + it->margin[AX_START[ma]] + it->margin[AX_END[ma]];
}

static bool cross_auto_margin(const flex_item* it, int ca) {
  return it->auto_margin[AX_START[ca]] || it->auto_margin[AX_END[ca]];
}

static void cross_avail(const flex_item* it, int ca, const float inner[2], const float limit[2], bool allow_stretch,
                        bool min_content, float* av, rd_measure_mode* mode) {
  if (!isnan(it->size_def[ca])) {
    *av   = clampf(it->size_def[ca], it->min[ca], it->max[ca]);
    *mode = RD_MEASURE_EXACTLY;
    return;
  }
  if (isnan(limit[ca])) {
    *av   = NAN;
    *mode = min_content ? RD_MEASURE_MIN_CONTENT : RD_MEASURE_UNDEFINED;
    return;
  }
  *av          = fmaxf(0, limit[ca] - it->margin[AX_START[ca]] - it->margin[AX_END[ca]]);
  bool stretch = allow_stretch && it->align == RD_ALIGN_STRETCH && !isnan(inner[ca]) && !cross_auto_margin(it, ca);
  if (stretch) *av = clampf(*av, it->min[ca], it->max[ca]);
  *mode = stretch ? RD_MEASURE_EXACTLY : RD_MEASURE_AT_MOST;
}

static rd_layout_key axis_key(int ma, float main, rd_measure_mode mm, float cross, rd_measure_mode cm, const float cb[2]) {
  rd_layout_key k = {.cbw = cb[0], .cbh = cb[1]};
  if (ma == 0) {
    k.aw = main, k.wm = mm, k.ah = cross, k.hm = cm;
  } else {
    k.ah = main, k.hm = mm, k.aw = cross, k.wm = cm;
  }
  return k;
}

// CSS Flexbox §9.7.
static void resolve_flexible(flex_item* items, const flex_line* line, int ma, float main_inner, float gap) {
  uint32_t n    = line->end - line->start;
  float    gaps = n > 1 ? gap * (float)(n - 1) : 0;
  float    sum  = gaps;
  for (uint32_t j = line->start; j < line->end; j++) sum += outer_main(&items[j], ma, items[j].hyp);
  bool growing = sum < main_inner;

  for (uint32_t j = line->start; j < line->end; j++) {
    flex_item* it = &items[j];
    float      f  = growing ? it->grow : it->shrink;
    it->target    = it->base;
    it->frozen    = f == 0 || (growing && it->base > it->hyp) || (!growing && it->base < it->hyp);
    if (it->frozen) it->target = it->hyp;
  }

  float initial_free = main_inner - gaps;
  for (uint32_t j = line->start; j < line->end; j++)
    initial_free -= outer_main(&items[j], ma, items[j].frozen ? items[j].target : items[j].base);

  for (;;) {
    float used = gaps, sum_f = 0, sum_scaled = 0;
    bool  any = false;
    for (uint32_t j = line->start; j < line->end; j++) {
      flex_item* it = &items[j];
      if (it->frozen) {
        used += outer_main(it, ma, it->target);
      } else {
        any = true;
        used += outer_main(it, ma, it->base);
        sum_f += growing ? it->grow : it->shrink;
        sum_scaled += it->shrink * it->base;
      }
    }
    if (!any) break;

    float free = main_inner - used;
    if (sum_f < 1) {
      float t = initial_free * sum_f;
      if (fabsf(t) < fabsf(free)) free = t;
    }

    float total = 0;
    for (uint32_t j = line->start; j < line->end; j++) {
      flex_item* it = &items[j];
      if (it->frozen) continue;
      float t = it->base;
      if (growing && sum_f > 0)
        t += free * it->grow / sum_f;
      else if (!growing && sum_scaled > 0)
        t += free * it->shrink * it->base / sum_scaled;
      float c       = clampf(t, it->min[ma], it->max[ma]);
      it->violation = c - t;
      it->target    = c;
      total += it->violation;
    }

    for (uint32_t j = line->start; j < line->end; j++) {
      flex_item* it = &items[j];
      if (it->frozen) continue;
      if (total == 0 || (total > 0 && it->violation > 0) || (total < 0 && it->violation < 0)) it->frozen = true;
    }
  }
}

void rd__justify_offsets(rd_align justify, float free, uint32_t n, float* off, float* between) {
  *off = *between = 0;
  switch (justify) {
    case RD_ALIGN_END:
      *off = free;
      break;
    case RD_ALIGN_CENTER:
      *off = free / 2;
      break;
    case RD_ALIGN_SPACE_BETWEEN:
      if (n > 1 && free > 0) *between = free / (float)(n - 1);
      break;
    case RD_ALIGN_SPACE_AROUND:
      if (free > 0) {
        *between = free / (float)n;
        *off     = *between / 2;
      } else {
        *off = free / 2;
      }
      break;
    case RD_ALIGN_SPACE_EVENLY:
      if (free > 0) {
        *between = free / (float)(n + 1);
        *off     = *between;
      } else {
        *off = free / 2;
      }
      break;
    default:
      break;
  }
}

void rd__apply_relative(const rd_style* st, rd_node_layout* L, const float cb[2]) {
  if (st->position != RD_POSITION_RELATIVE) return;
  float l = resolve(st->inset.left, cb[0]), r = resolve(st->inset.right, cb[0]);
  float t = resolve(st->inset.top, cb[1]), b = resolve(st->inset.bottom, cb[1]);
  L->rect.x += !isnan(l) ? l : !isnan(r) ? -r
                                         : 0;
  L->rect.y += !isnan(t) ? t : !isnan(b) ? -b
                                         : 0;
}

void rd__layout_absolute(rd_ctx* ctx, uint32_t c, const rd_node_layout* P, const float size[2]) {
  const rd_style* st    = &ctx->styles[c].computed;
  rd_node_layout* L     = &ctx->layouts[c];
  float           cb[2] = {
      fmaxf(0, size[0] - P->border[EDGE_L] - P->border[EDGE_R]),
      fmaxf(0, size[1] - P->border[EDGE_T] - P->border[EDGE_B]),
  };
  float in[4] = {
      resolve(st->inset.top, cb[1]),
      resolve(st->inset.right, cb[0]),
      resolve(st->inset.bottom, cb[1]),
      resolve(st->inset.left, cb[0]),
  };
  float m[4];
  bool  am[4];
  resolve_margins(&st->margin, cb[0], m, am);
  float pb[2], mn[2], mx[2];
  style_pb(st, cb[0], pb);
  size_bounds(st, pb, cb[0], cb[1], mn, mx);

  float sz[2] = {resolve(st->width, cb[0]), resolve(st->height, cb[1])};
  for (int a = 0; a < 2; a++) {
    int s = AX_START[a], e = AX_END[a];
    if (isnan(sz[a]) && !isnan(in[s]) && !isnan(in[e])) sz[a] = cb[a] - in[s] - in[e] - m[s] - m[e];
    if (!isnan(sz[a])) sz[a] = clampf(sz[a], mn[a], mx[a]);
  }
  if (isnan(sz[0]) || isnan(sz[1])) {
    rd_layout_key k = {.cbw = cb[0], .cbh = cb[1]};
    float         av[2];
    for (int a = 0; a < 2; a++) {
      int s = AX_START[a], e = AX_END[a];
      av[a] = !isnan(sz[a]) ? sz[a]
                            : fmaxf(0, cb[a] - (isnan(in[s]) ? 0 : in[s]) - (isnan(in[e]) ? 0 : in[e]) - m[s] - m[e]);
    }
    k.aw      = av[0];
    k.ah      = av[1];
    k.wm      = isnan(sz[0]) ? RD_MEASURE_AT_MOST : RD_MEASURE_EXACTLY;
    k.hm      = isnan(sz[1]) ? RD_MEASURE_AT_MOST : RD_MEASURE_EXACTLY;
    rd_size r = rd__layout_node(ctx, c, k, false);
    if (isnan(sz[0])) sz[0] = r.width;
    if (isnan(sz[1])) sz[1] = r.height;
  }
  rd__layout_node(ctx, c, (rd_layout_key){sz[0], sz[1], cb[0], cb[1], RD_MEASURE_EXACTLY, RD_MEASURE_EXACTLY}, true);

  float pos[2];
  for (int a = 0; a < 2; a++) {
    int   s = AX_START[a], e = AX_END[a];
    float origin = P->border[s];
    if (!isnan(in[s]))
      pos[a] = origin + in[s] + m[s];
    else if (!isnan(in[e]))
      pos[a] = origin + cb[a] - in[e] - m[e] - sz[a];
    else
      pos[a] = origin + P->padding[s] + m[s];
  }
  L->rect.x = pos[0];
  L->rect.y = pos[1];
  memcpy(L->margin, m, sizeof m);
}

static rd_size layout_flex(rd_ctx* ctx, uint32_t i, const rd_layout_key* k, bool perform) {
  rd_slot*        s  = &ctx->slots[i];
  rd_node_layout* L  = &ctx->layouts[i];
  const rd_style* st = &ctx->styles[i].computed;

  const bool row      = st->flex_direction == RD_FLEX_ROW || st->flex_direction == RD_FLEX_ROW_REVERSE;
  const bool main_rev = st->flex_direction == RD_FLEX_ROW_REVERSE || st->flex_direction == RD_FLEX_COLUMN_REVERSE;
  const bool wrap     = st->flex_wrap == RD_FLEX_WRAP || st->flex_wrap == RD_FLEX_WRAP_REVERSE;
  const bool wrap_rev = st->flex_wrap == RD_FLEX_WRAP_REVERSE;
  const int  ma = row ? 0 : 1, ca = 1 - ma;

  float pb_start[2] = {
      L->padding[EDGE_L] + L->border[EDGE_L],
      L->padding[EDGE_T] + L->border[EDGE_T],
  };
  float pb[2] = {
      pb_start[0] + L->padding[EDGE_R] + L->border[EDGE_R],
      pb_start[1] + L->padding[EDGE_B] + L->border[EDGE_B],
  };
  float           avail[2] = {k->aw, k->ah};
  rd_measure_mode mode[2]  = {k->wm, k->hm};
  float           mn[2], mx[2];
  size_bounds(st, pb, k->cbw, k->cbh, mn, mx);

  float inner[2], limit[2];
  bool  minc[2] = {mode[0] == RD_MEASURE_MIN_CONTENT, mode[1] == RD_MEASURE_MIN_CONTENT};
  for (int a = 0; a < 2; a++) {
    bool sized = mode[a] == RD_MEASURE_EXACTLY || mode[a] == RD_MEASURE_AT_MOST;
    inner[a]   = mode[a] == RD_MEASURE_EXACTLY ? fmaxf(0, avail[a] - pb[a]) : NAN;
    limit[a]   = sized ? fmaxf(0, avail[a] - pb[a]) : NAN;
    if (isfinite(mx[a]) && !minc[a]) {
      float cap = fmaxf(0, mx[a] - pb[a]);
      limit[a]  = isnan(limit[a]) ? cap : fminf(limit[a], cap);
    }
    if (!isnan(inner[a])) limit[a] = inner[a];
  }
  float gap[2] = {resolve0(st->column_gap, inner[0]), resolve0(st->row_gap, inner[1])};

  uint32_t n = 0;
  for (uint32_t c = s->first_child; c; c = ctx->slots[c].next_sibling)
    if (in_flow(&ctx->styles[c].computed)) n++;

  flex_item* items = n ? calloc(n, sizeof *items) : NULL;
  flex_line* lines = n ? calloc(n, sizeof *lines) : NULL;
  if (n && (!items || !lines)) {
    free(items);
    free(lines);
    return (rd_size){finalize_axis(pb[0], mode[0], avail[0], mn[0], mx[0]),
                     finalize_axis(pb[1], mode[1], avail[1], mn[1], mx[1])};
  }

  uint32_t m = 0;
  for (uint32_t c = s->first_child; c; c = ctx->slots[c].next_sibling) {
    const rd_style* cs = &ctx->styles[c].computed;
    if (!in_flow(cs)) continue;
    if (m == n) break;
    flex_item* it = &items[m++];
    it->idx       = c;
    it->st        = cs;
    resolve_margins(&cs->margin, inner[0], it->margin, it->auto_margin);
    style_pb(cs, inner[0], it->pb);
    size_bounds(cs, it->pb, inner[0], inner[1], it->min, it->max);
    it->size_def[0] = resolve(cs->width, inner[0]);
    it->size_def[1] = resolve(cs->height, inner[1]);
    it->grow        = cs->flex_grow.unit == RD_UNIT_NUM ? fmaxf(0, cs->flex_grow.value) : 0;
    it->shrink      = cs->flex_shrink.unit == RD_UNIT_NUM ? fmaxf(0, cs->flex_shrink.value) : 0;
    it->align       = item_align(cs->align_self, st->align_items);
    if ((cs->align_self == RD_ALIGN_AUTO ? st->align_items : cs->align_self) == RD_ALIGN_BASELINE && row && !wrap_rev)
      it->align = RD_ALIGN_BASELINE;

    float basis = resolve(cs->flex_basis, inner[ma]);
    if (isnan(basis)) basis = it->size_def[ma];
    if (isnan(basis)) {
      float           cav;
      rd_measure_mode cm;
      cross_avail(it, ca, inner, limit, !wrap, minc[ca], &cav, &cm);
      rd_measure_mode mm = minc[ma] ? RD_MEASURE_MIN_CONTENT : RD_MEASURE_UNDEFINED;
      rd_size         r  = rd__layout_node(ctx, c, axis_key(ma, NAN, mm, cav, cm, inner), false);
      basis              = ma ? r.height : r.width;
    }
    it->base = fmaxf(basis, it->pb[ma]);
    it->hyp  = clampf(it->base, it->min[ma], it->max[ma]);
  }

  uint32_t nl = 0;
  if (n) {
    float    sum   = 0;
    uint32_t count = 0;
    for (uint32_t j = 0; j < n; j++) {
      float outer = outer_main(&items[j], ma, items[j].hyp);
      if (wrap && count && (minc[ma] || (!isnan(limit[ma]) && sum + gap[ma] + outer > limit[ma] + EPS))) {
        lines[nl++].end = j;
        lines[nl].start = j;
        sum             = 0;
        count           = 0;
      }
      sum += (count ? gap[ma] : 0) + outer;
      count++;
    }
    lines[nl++].end = n;
  }

  float main_inner = inner[ma];
  if (isnan(main_inner)) {
    float content = 0;
    for (uint32_t l = 0; l < nl; l++) {
      float sum = gap[ma] * (float)(lines[l].end - lines[l].start - 1);
      for (uint32_t j = lines[l].start; j < lines[l].end; j++) sum += outer_main(&items[j], ma, items[j].hyp);
      content = fmaxf(content, sum);
    }
    main_inner = finalize_axis(content + pb[ma], mode[ma], avail[ma], mn[ma], mx[ma]) - pb[ma];
  }
  for (uint32_t l = 0; l < nl; l++) resolve_flexible(items, &lines[l], ma, main_inner, gap[ma]);

  float cb[2];
  cb[ma] = main_inner;
  cb[ca] = inner[ca];
  for (uint32_t j = 0; j < n; j++) {
    flex_item* it      = &items[j];
    bool       stretch = it->align == RD_ALIGN_STRETCH && isnan(it->size_def[ca]) && !cross_auto_margin(it, ca);
    if (!isnan(it->size_def[ca])) {
      it->cross = clampf(it->size_def[ca], it->min[ca], it->max[ca]);
    } else if (stretch && !wrap && !isnan(inner[ca])) {
      it->cross = clampf(inner[ca] - it->margin[AX_START[ca]] - it->margin[AX_END[ca]], it->min[ca], it->max[ca]);
    } else {
      float           cav;
      rd_measure_mode cm;
      cross_avail(it, ca, inner, limit, false, minc[ca], &cav, &cm);
      rd_size r = rd__layout_node(ctx, it->idx, axis_key(ma, it->target, RD_MEASURE_EXACTLY, cav, cm, cb), false);
      it->cross = clampf(ca ? r.height : r.width, it->min[ca], it->max[ca]);
    }
    if (it->align == RD_ALIGN_BASELINE) {
      if (cross_auto_margin(it, ca)) {
        it->align = RD_ALIGN_START;
      } else {
        rd__layout_node(ctx, it->idx,
                        (rd_layout_key){it->target, it->cross, cb[0], cb[1], RD_MEASURE_EXACTLY, RD_MEASURE_EXACTLY},
                        true);
        float b      = ctx->layouts[it->idx].baseline;
        it->baseline = it->margin[EDGE_T] + (isnan(b) ? it->cross : b);
      }
    }
  }

  float total_cross = 0;
  for (uint32_t l = 0; l < nl; l++) {
    float above = 0, below = 0;
    lines[l].cross = 0;
    for (uint32_t j = lines[l].start; j < lines[l].end; j++) {
      const flex_item* it    = &items[j];
      float            outer = outer_main(it, ca, it->cross);
      if (it->align == RD_ALIGN_BASELINE) {
        above = fmaxf(above, it->baseline);
        below = fmaxf(below, outer - it->baseline);
      } else {
        lines[l].cross = fmaxf(lines[l].cross, outer);
      }
    }
    lines[l].baseline = above;
    lines[l].cross    = fmaxf(lines[l].cross, above + below);
    total_cross += lines[l].cross + (l ? gap[ca] : 0);
  }

  float cross_inner = inner[ca];
  if (isnan(cross_inner)) cross_inner = finalize_axis(total_cross + pb[ca], mode[ca], avail[ca], mn[ca], mx[ca]) - pb[ca];
  if (!wrap && nl == 1) {
    total_cross    = cross_inner;
    lines[0].cross = cross_inner;
  }

  float off = 0, between = 0;
  if (wrap && nl) {
    float free = cross_inner - total_cross;
    if (st->align_content == RD_ALIGN_STRETCH || st->align_content == RD_ALIGN_NORMAL) {
      if (free > 0)
        for (uint32_t l = 0; l < nl; l++) lines[l].cross += free / (float)nl;
    } else {
      rd__justify_offsets(st->align_content, free, nl, &off, &between);
    }
  }
  for (uint32_t l = 0; l < nl; l++) {
    lines[l].pos = off;
    off += lines[l].cross + gap[ca] + between;
    if (wrap_rev) lines[l].pos = cross_inner - lines[l].pos - lines[l].cross;
    lines[l].pos += pb_start[ca];
  }

  for (uint32_t l = 0; l < nl; l++) {
    flex_line* ln    = &lines[l];
    uint32_t   cnt   = ln->end - ln->start;
    float      used  = gap[ma] * (float)(cnt - 1);
    uint32_t   autos = 0;
    for (uint32_t j = ln->start; j < ln->end; j++) {
      used += outer_main(&items[j], ma, items[j].target);
      autos += (uint32_t)items[j].auto_margin[AX_START[ma]] + (uint32_t)items[j].auto_margin[AX_END[ma]];
    }
    float free = main_inner - used, share = 0, joff = 0, jbetween = 0;
    if (autos && free > 0)
      share = free / (float)autos;
    else
      rd__justify_offsets(st->justify_content, free, cnt, &joff, &jbetween);

    int   lead  = main_rev ? AX_END[ma] : AX_START[ma];
    int   trail = main_rev ? AX_START[ma] : AX_END[ma];
    float p     = joff;
    for (uint32_t j = ln->start; j < ln->end; j++) {
      flex_item* it = &items[j];
      p += it->margin[lead] + (it->auto_margin[lead] ? share : 0);
      float pos = p;
      p += it->target + it->margin[trail] + (it->auto_margin[trail] ? share : 0) + gap[ma] + jbetween;
      if (main_rev) pos = main_inner - pos - it->target;
      it->pos[ma] = pb_start[ma] + pos;
    }

    int clead  = wrap_rev ? AX_END[ca] : AX_START[ca];
    int ctrail = wrap_rev ? AX_START[ca] : AX_END[ca];
    for (uint32_t j = ln->start; j < ln->end; j++) {
      flex_item* it = &items[j];
      if (it->align == RD_ALIGN_STRETCH && isnan(it->size_def[ca]) && !cross_auto_margin(it, ca))
        it->cross = clampf(ln->cross - it->margin[clead] - it->margin[ctrail], it->min[ca], it->max[ca]);
      float cfree = ln->cross - it->cross - it->margin[clead] - it->margin[ctrail];
      float o     = 0;
      bool  al = it->auto_margin[clead], at = it->auto_margin[ctrail];
      if (al || at) {
        if (cfree > 0) o = al && at ? cfree / 2 : al ? cfree
                                                     : 0;
      } else if (it->align == RD_ALIGN_BASELINE) {
        o = ln->baseline - it->baseline;
      } else if (it->align == RD_ALIGN_END) {
        o = cfree;
      } else if (it->align == RD_ALIGN_CENTER) {
        o = cfree / 2;
      }
      float pos = o + it->margin[clead];
      if (wrap_rev) pos = ln->cross - pos - it->cross;
      it->pos[ca] = ln->pos + pos;
    }
  }

  float size[2];
  size[ma] = main_inner + pb[ma];
  size[ca] = cross_inner + pb[ca];

  if (perform) {
    cb[ca]       = cross_inner;
    float ext[2] = {0, 0};
    for (uint32_t j = 0; j < n; j++) {
      flex_item* it = &items[j];
      float      sz[2];
      sz[ma] = it->target;
      sz[ca] = it->cross;
      rd__layout_node(ctx, it->idx, (rd_layout_key){sz[0], sz[1], cb[0], cb[1], RD_MEASURE_EXACTLY, RD_MEASURE_EXACTLY},
                      true);
      rd_node_layout* CL = &ctx->layouts[it->idx];
      CL->rect.x         = it->pos[0];
      CL->rect.y         = it->pos[1];
      memcpy(CL->margin, it->margin, sizeof it->margin);
      rd__apply_relative(it->st, CL, cb);
      ext[0] = fmaxf(ext[0], it->pos[0] + sz[0] + it->margin[EDGE_R]);
      ext[1] = fmaxf(ext[1], it->pos[1] + sz[1] + it->margin[EDGE_B]);
    }
    if (n) L->baseline = ctx->layouts[items[0].idx].rect.y + ctx->layouts[items[0].idx].baseline;
    for (uint32_t c = s->first_child; c; c = ctx->slots[c].next_sibling) {
      const rd_style* cs = &ctx->styles[c].computed;
      if (cs->display == RD_DISPLAY_NONE)
        ctx->layouts[c].rect = (rd_rect){0};
      else if (cs->position == RD_POSITION_ABSOLUTE)
        rd__layout_absolute(ctx, c, L, size);
    }
    L->content.width  = fmaxf(ext[0] + L->padding[EDGE_R], size[0] - L->border[EDGE_R]) - L->border[EDGE_L];
    L->content.height = fmaxf(ext[1] + L->padding[EDGE_B], size[1] - L->border[EDGE_B]) - L->border[EDGE_T];
    L->rect.width     = size[0];
    L->rect.height    = size[1];
    rd__clamp_scroll(L);
  }

  free(items);
  free(lines);
  return (rd_size){size[0], size[1]};
}

static rd_size compute(rd_ctx* ctx, uint32_t i, const rd_layout_key* k, bool perform) {
  rd_node_layout* L  = &ctx->layouts[i];
  const rd_style* st = &ctx->styles[i].computed;
  resolve_edges(&st->padding, k->cbw, L->padding);
  resolve_edges(&st->border_width, NAN, L->border);
  if (perform) L->baseline = NAN;
  if (st->display == RD_DISPLAY_NONE) return (rd_size){0, 0};
  if (st->display == RD_DISPLAY_GRID && !L->measure_fn) return rd__layout_grid(ctx, i, k, perform);
  if (L->measure_fn) {
    rd_size r = layout_leaf(ctx, i, k);
    if (perform) {
      L->content = r;
      if (rd__is_text(ctx, i)) L->baseline = L->border[EDGE_T] + L->padding[EDGE_T] + rd__text_baseline(ctx, i);
    }
    return r;
  }
  return layout_flex(ctx, i, k, perform);
}

static bool is_pct(rd_val v) { return v.unit == RD_UNIT_PCT; }

// A node reads its containing block only through its own percentage padding and min/max sizes.
// Dropping an unused dimension from the key lets the parent's measure and perform passes share
// cache entries.
static void normalize_key(const rd_style* st, rd_layout_key* k) {
  if (k->wm == RD_MEASURE_UNDEFINED || k->wm == RD_MEASURE_MIN_CONTENT) k->aw = NAN;
  if (k->hm == RD_MEASURE_UNDEFINED || k->hm == RD_MEASURE_MIN_CONTENT) k->ah = NAN;
  bool w = is_pct(st->padding.top) || is_pct(st->padding.right) || is_pct(st->padding.bottom) ||
           is_pct(st->padding.left) || is_pct(st->min_width) || is_pct(st->max_width);
  bool h = is_pct(st->min_height) || is_pct(st->max_height);
  if (!w) k->cbw = NAN;
  if (!h) k->cbh = NAN;
}

rd_size rd__layout_node(rd_ctx* ctx, uint32_t i, rd_layout_key k, bool perform) {
  rd_slot*        s = &ctx->slots[i];
  rd_node_layout* L = &ctx->layouts[i];
  normalize_key(&ctx->styles[i].computed, &k);
  if (s->flags & RD_FLAG_LAYOUT_DIRTY) {
    L->layout.valid = false;
    for (int j = 0; j < RD_MEASURE_CACHE_SIZE; j++) L->measure[j].valid = false;
    s->flags &= ~RD_FLAG_LAYOUT_DIRTY;
  }
  if (L->layout.valid && key_eq(&L->layout.key, &k)) return L->layout.size;
  if (!perform) {
    for (int j = 0; j < RD_MEASURE_CACHE_SIZE; j++)
      if (L->measure[j].valid && key_eq(&L->measure[j].key, &k)) return L->measure[j].size;
  }

  rd_size r = compute(ctx, i, &k, perform);
  if (perform) {
    L->layout      = (rd_layout_cache){k, r, true};
    L->rect.width  = r.width;
    L->rect.height = r.height;
  } else {
    L->measure[L->measure_next] = (rd_layout_cache){k, r, true};
    L->measure_next             = (uint8_t)((L->measure_next + 1) % RD_MEASURE_CACHE_SIZE);
  }
  return r;
}

static void layout_pass(rd_ctx* ctx, float width, float height) {
  rd_ctx_compute_styles(ctx);
  rd__layout_node(ctx, ctx->root, (rd_layout_key){width, height, width, height, RD_MEASURE_EXACTLY, RD_MEASURE_EXACTLY},
                  true);
  ctx->layouts[ctx->root].rect.x = 0;
  ctx->layouts[ctx->root].rect.y = 0;
  rd__place_anchored(ctx);
}

static bool tree_dirty(const rd_ctx* ctx) {
  const rd_slot* r = &ctx->slots[ctx->root];
  return r->flags & (RD_FLAG_LAYOUT_DIRTY | RD_FLAG_STYLE_DIRTY | RD_FLAG_SUBTREE_STYLE_DIRTY);
}

void rd_ctx_layout(rd_ctx* ctx, float width, float height) {
  if (!ctx) return;
  rd__anim_tick(ctx);
  layout_pass(ctx, width, height);
  for (uint32_t j = 0; j < ctx->hook_count; j++) ctx->hooks[j].fn(ctx, ctx->hooks[j].data);
  if (tree_dirty(ctx)) layout_pass(ctx, width, height);
}

bool rd_ctx_on_layout(rd_ctx* ctx, rd_layout_hook fn, void* data, rd_destroy_fn destroy) {
  if (!ctx || !fn) return false;
  rd__hook* p = realloc(ctx->hooks, (ctx->hook_count + 1) * sizeof *p);
  if (!p) return false;
  ctx->hooks                    = p;
  ctx->hooks[ctx->hook_count++] = (rd__hook){fn, data, destroy};
  return true;
}

rd_rect rd_node_rect(const rd_ctx* ctx, rd_node node) {
  return rd__slot(ctx, node) ? ctx->layouts[node.index].rect : (rd_rect){0};
}

rd_rect rd_node_bounds(const rd_ctx* ctx, rd_node node) {
  if (!rd__slot(ctx, node)) return (rd_rect){0};
  rd_rect r = ctx->layouts[node.index].rect;
  for (uint32_t p = ctx->slots[node.index].parent; p; p = ctx->slots[p].parent) {
    r.x += ctx->layouts[p].rect.x - ctx->layouts[p].scroll[0];
    r.y += ctx->layouts[p].rect.y - ctx->layouts[p].scroll[1];
  }
  return r;
}

static rd_insets insets(const float e[4]) { return (rd_insets){e[EDGE_T], e[EDGE_R], e[EDGE_B], e[EDGE_L]}; }

rd_box rd_node_box(const rd_ctx* ctx, rd_node node) {
  if (!rd__slot(ctx, node)) return (rd_box){0};
  const rd_node_layout* L = &ctx->layouts[node.index];
  return (rd_box){insets(L->margin), insets(L->border), insets(L->padding)};
}

rd_size rd_node_content_size(const rd_ctx* ctx, rd_node node) {
  return rd__slot(ctx, node) ? ctx->layouts[node.index].content : (rd_size){0};
}

bool rd_node_set_measure(rd_ctx* ctx, rd_node node, rd_measure_fn fn, void* data, rd_destroy_fn destroy) {
  if (!rd__slot(ctx, node)) return false;
  rd_node_layout* L           = &ctx->layouts[node.index];
  void*           old_data    = L->measure_data;
  rd_destroy_fn   old_destroy = L->measure_destroy;
  L->measure_fn               = fn;
  L->measure_data             = fn ? data : NULL;
  L->measure_destroy          = fn ? destroy : NULL;
  if (old_destroy && (old_data != L->measure_data || !fn)) old_destroy(old_data);
  rd__mark_layout_dirty(ctx, node.index);
  return true;
}

void rd_node_mark_layout_dirty(rd_ctx* ctx, rd_node node) {
  if (rd__slot(ctx, node)) rd__mark_layout_dirty(ctx, node.index);
}

void rd__layout_slot_free(rd_ctx* ctx, uint32_t i) {
  rd_node_layout* L       = &ctx->layouts[i];
  rd_destroy_fn   destroy = L->measure_destroy;
  void*           data    = L->measure_data;
  L->measure_fn           = NULL;
  L->measure_data         = NULL;
  L->measure_destroy      = NULL;
  if (destroy) destroy(data);
  rd_destroy_fn paint_destroy = L->paint_destroy;
  void*         paint_data    = L->paint_data;
  L->paint_fn                 = NULL;
  L->paint_data               = NULL;
  L->paint_destroy            = NULL;
  if (paint_destroy) paint_destroy(paint_data);
  free(L->anchor);
  L->anchor = NULL;
}

float rd__natural_height(rd_ctx* ctx, uint32_t i) {
  rd_node_layout* L     = &ctx->layouts[i];
  rd_style*       st    = &ctx->styles[i].computed;
  rd_val          shown = st->height;
  st->height            = (rd_val){0, RD_UNIT_AUTO};
  rd_layout_key k       = L->layout.key;
  rd_size       s       = rd__layout_node(
      ctx, i, (rd_layout_key){L->rect.width, NAN, k.cbw, k.cbh, RD_MEASURE_EXACTLY, RD_MEASURE_UNDEFINED}, false);
  st->height = shown;
  rd__mark_layout_dirty(ctx, i);  // the measurement used a height that isn't shown
  return s.height;
}
