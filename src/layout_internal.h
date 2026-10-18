#ifndef RAYDOM_LAYOUT_INTERNAL_H
#define RAYDOM_LAYOUT_INTERNAL_H

#include <math.h>

#include "internal.h"

enum { EDGE_T,
       EDGE_R,
       EDGE_B,
       EDGE_L };

// Indexed by axis: 0 = x, 1 = y.
static const int AX_START[2] = {EDGE_L, EDGE_T};
static const int AX_END[2]   = {EDGE_R, EDGE_B};

#define EPS 0.001f

static inline float resolve(rd_val v, float ref) {
  switch (v.unit) {
    case RD_UNIT_PX:
      return v.value;
    case RD_UNIT_PCT:
      return isnan(ref) ? NAN : v.value * ref / 100.0f;
    default:
      return NAN;
  }
}

static inline float resolve0(rd_val v, float ref) {
  float r = resolve(v, ref);
  return isnan(r) ? 0 : r;
}

static inline float resolve_max(rd_val v, float ref) {
  float r = resolve(v, ref);
  return isnan(r) ? INFINITY : r;
}

static inline float clampf(float v, float lo, float hi) { return fmaxf(lo, fminf(v, hi)); }

static inline void resolve_edges(const rd_edges* e, float ref, float out[4]) {
  const rd_val* v[4] = {&e->top, &e->right, &e->bottom, &e->left};
  for (int j = 0; j < 4; j++) out[j] = fmaxf(0, resolve0(*v[j], ref));
}

static inline void resolve_margins(const rd_edges* e, float ref, float out[4], bool autos[4]) {
  const rd_val* v[4] = {&e->top, &e->right, &e->bottom, &e->left};
  for (int j = 0; j < 4; j++) {
    autos[j] = v[j]->unit == RD_UNIT_AUTO;
    out[j]   = resolve0(*v[j], ref);
  }
}

static inline void style_pb(const rd_style* st, float cbw, float pb[2]) {
  float pad[4], bor[4];
  resolve_edges(&st->padding, cbw, pad);
  resolve_edges(&st->border_width, NAN, bor);
  pb[0] = pad[EDGE_L] + pad[EDGE_R] + bor[EDGE_L] + bor[EDGE_R];
  pb[1] = pad[EDGE_T] + pad[EDGE_B] + bor[EDGE_T] + bor[EDGE_B];
}

static inline void size_bounds(const rd_style* st, const float pb[2], float cbw, float cbh, float mn[2], float mx[2]) {
  mn[0] = fmaxf(resolve0(st->min_width, cbw), pb[0]);
  mn[1] = fmaxf(resolve0(st->min_height, cbh), pb[1]);
  mx[0] = resolve_max(st->max_width, cbw);
  mx[1] = resolve_max(st->max_height, cbh);
}

static inline float finalize_axis(float content, rd_measure_mode mode, float avail, float mn, float mx) {
  if (mode == RD_MEASURE_EXACTLY) return avail;
  float v = clampf(content, mn, mx);
  if (mode == RD_MEASURE_AT_MOST && v > avail) v = fmaxf(avail, mn);
  return v;
}

static inline rd_align item_align(rd_align self, rd_align items) {
  rd_align a = self == RD_ALIGN_AUTO ? items : self;
  switch (a) {
    case RD_ALIGN_END:
    case RD_ALIGN_CENTER:
    case RD_ALIGN_STRETCH:
      return a;
    case RD_ALIGN_NORMAL:
      return RD_ALIGN_STRETCH;
    default:
      return RD_ALIGN_START;
  }
}

static inline bool in_flow(const rd_style* st) {
  return st->display != RD_DISPLAY_NONE && st->position != RD_POSITION_ABSOLUTE;
}

rd_size rd__layout_node(rd_ctx* ctx, uint32_t i, rd_layout_key k, bool perform);
void    rd__layout_absolute(rd_ctx* ctx, uint32_t c, const rd_node_layout* parent, const float size[2]);
void    rd__apply_relative(const rd_style* st, rd_node_layout* L, const float cb[2]);
void    rd__justify_offsets(rd_align justify, float free, uint32_t n, float* off, float* between);
rd_size rd__layout_grid(rd_ctx* ctx, uint32_t i, const rd_layout_key* k, bool perform);

#endif
