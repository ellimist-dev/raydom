#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "layout_internal.h"

// Properties by index, from the X-macro.

enum {
#define X(name, type, kind, inherited, layout) P_##name,
  RD_STYLE_PROPS(X)
#undef X
      P_COUNT
};

enum { K_OTHER, K_VAL, K_COLOR, K_EDGES, K_CORNERS, K_SHADOW };
#define K_ENUM      K_OTHER
#define K_TRACKS    K_OTHER
#define K_TRACK     K_OTHER
#define K_GRID_LINE K_OTHER
#define K_FONT      K_OTHER

typedef struct prop {
  uint16_t offset, size;
  uint8_t  kind;
  bool     inherited, layout;
} prop;

static const prop PROPS[P_COUNT] = {
#define X(name, type, kind, inherited, layout) {offsetof(rd_style, name), sizeof(type), K_##kind, inherited, layout},
    RD_STYLE_PROPS(X)
#undef X
};

static const uint8_t GROUP[P_COUNT] = {
    [P_background] = RD_TRANSITION_COLOR,      [P_border_color] = RD_TRANSITION_COLOR,
    [P_color] = RD_TRANSITION_COLOR,           [P_caret_color] = RD_TRANSITION_COLOR,
    [P_selection_color] = RD_TRANSITION_COLOR, [P_scrollbar_color] = RD_TRANSITION_COLOR,
    [P_scrollbar_track_color] = RD_TRANSITION_COLOR,
    [P_opacity] = RD_TRANSITION_OPACITY,       [P_box_shadow] = RD_TRANSITION_SHADOW,
    [P_border_radius] = RD_TRANSITION_RADIUS,
    [P_width] = RD_TRANSITION_SIZE,            [P_height] = RD_TRANSITION_SIZE,
    [P_min_width] = RD_TRANSITION_SIZE,        [P_min_height] = RD_TRANSITION_SIZE,
    [P_max_width] = RD_TRANSITION_SIZE,        [P_max_height] = RD_TRANSITION_SIZE,
    [P_flex_grow] = RD_TRANSITION_SIZE,        [P_flex_shrink] = RD_TRANSITION_SIZE,
    [P_flex_basis] = RD_TRANSITION_SIZE,
    [P_margin] = RD_TRANSITION_SPACING,        [P_padding] = RD_TRANSITION_SPACING,
    [P_border_width] = RD_TRANSITION_SPACING,  [P_row_gap] = RD_TRANSITION_SPACING,
    [P_column_gap] = RD_TRANSITION_SPACING,    [P_inset] = RD_TRANSITION_INSET,
    [P_font_size] = RD_TRANSITION_TEXT,        [P_line_height] = RD_TRANSITION_TEXT,
};

static void*       field(rd_style* s, int p) { return (char*)s + PROPS[p].offset; }
static const void* cfield(const rd_style* s, int p) { return (const char*)s + PROPS[p].offset; }
static void        copy(int p, rd_style* dst, const void* v) { memcpy(field(dst, p), v, PROPS[p].size); }

// Per-node animation state, allocated while something animates or an explicit animation is pending.
typedef struct rd__anim {
  rd_style from, to;
  double   start[P_COUNT];
  float    duration[P_COUNT];
  uint8_t  easing[P_COUNT];
  bool     active[P_COUNT];
  bool     to_auto[P_COUNT];  // height animating to auto: the end is re-measured every frame
  uint32_t count;             // active properties
  // rd_node_animate, applied at the node's next style computation.
  bool     pending, pending_from_set;
  rd_style pending_from;
  bool     pending_mask[P_COUNT];
  float    pending_duration;
  double   pending_start;  // when rd_node_animate was called, so `done` lines up with the end
  uint8_t  pending_easing;
} rd__anim;

// Values.

static bool eq_val(rd_val a, rd_val b) { return a.unit == b.unit && a.value == b.value; }
static bool numeric(rd_val v) {
  return v.unit == RD_UNIT_PX || v.unit == RD_UNIT_PCT || v.unit == RD_UNIT_NUM || v.unit == RD_UNIT_FR;
}
static bool lerpable_val(rd_val a, rd_val b) { return eq_val(a, b) || (a.unit == b.unit && numeric(a)); }
static rd_val lerp_val(rd_val a, rd_val b, float t) {
  if (!lerpable_val(a, b)) return b;
  return (rd_val){a.value + (b.value - a.value) * t, b.unit};
}
static bool eq_color(rd_color a, rd_color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }
// Premultiplied, so fading to or from transparent doesn't pass through black.
static rd_color lerp_color(rd_color a, rd_color b, float t) {
  float aa = (float)a.a / 255.0f, ba = (float)b.a / 255.0f, alpha = aa + (ba - aa) * t;
  rd_color c = {0, 0, 0, (uint8_t)lroundf(alpha * 255), true};
  if (alpha <= 0) return c;
  float ch[3][2] = {{(float)a.r, (float)b.r}, {(float)a.g, (float)b.g}, {(float)a.b, (float)b.b}};
  uint8_t* out[3] = {&c.r, &c.g, &c.b};
  for (int j = 0; j < 3; j++) {
    float v = (ch[j][0] * aa + (ch[j][1] * ba - ch[j][0] * aa) * t) / alpha;
    *out[j] = (uint8_t)lroundf(fminf(255, fmaxf(0, v)));
  }
  return c;
}

static bool equal(int p, const void* a, const void* b) {
  switch (PROPS[p].kind) {
    case K_VAL:
      return eq_val(*(const rd_val*)a, *(const rd_val*)b);
    case K_COLOR:
      return eq_color(*(const rd_color*)a, *(const rd_color*)b);
    case K_EDGES:
    case K_CORNERS:
      for (int j = 0; j < 4; j++)
        if (!eq_val(((const rd_val*)a)[j], ((const rd_val*)b)[j])) return false;
      return true;
    case K_SHADOW: {
      const rd_shadow *x = a, *y = b;
      return eq_val(x->x, y->x) && eq_val(x->y, y->y) && eq_val(x->blur, y->blur) && eq_val(x->spread, y->spread) &&
             eq_color(x->color, y->color);
    }
    default:
      return true;  // not animatable: never starts a transition
  }
}

static bool lerpable(int p, const void* a, const void* b) {
  switch (PROPS[p].kind) {
    case K_VAL:
      return lerpable_val(*(const rd_val*)a, *(const rd_val*)b);
    case K_COLOR:
      return true;
    case K_EDGES:
    case K_CORNERS:
      for (int j = 0; j < 4; j++)
        if (!lerpable_val(((const rd_val*)a)[j], ((const rd_val*)b)[j])) return false;
      return true;
    case K_SHADOW: {
      const rd_shadow *x = a, *y = b;
      return lerpable_val(x->x, y->x) && lerpable_val(x->y, y->y) && lerpable_val(x->blur, y->blur) &&
             lerpable_val(x->spread, y->spread);
    }
    default:
      return false;
  }
}

static void lerp(int p, void* out, const void* a, const void* b, float t) {
  switch (PROPS[p].kind) {
    case K_VAL:
      *(rd_val*)out = lerp_val(*(const rd_val*)a, *(const rd_val*)b, t);
      break;
    case K_COLOR:
      *(rd_color*)out = lerp_color(*(const rd_color*)a, *(const rd_color*)b, t);
      break;
    case K_EDGES:
    case K_CORNERS:
      for (int j = 0; j < 4; j++) ((rd_val*)out)[j] = lerp_val(((const rd_val*)a)[j], ((const rd_val*)b)[j], t);
      break;
    case K_SHADOW: {
      const rd_shadow *x = a, *y = b;
      *(rd_shadow*)out = (rd_shadow){lerp_val(x->x, y->x, t), lerp_val(x->y, y->y, t), lerp_val(x->blur, y->blur, t),
                                     lerp_val(x->spread, y->spread, t), lerp_color(x->color, y->color, t)};
      break;
    }
    default:
      break;
  }
}

// Easing: cubic-bezier(x1, y1, x2, y2) from (0,0) to (1,1), solved for x by Newton then bisection.
static float bezier(float a1, float a2, float t) { return ((1 - 3 * a2 + 3 * a1) * t + (3 * a2 - 6 * a1)) * t * t + 3 * a1 * t; }
static float bezier_slope(float a1, float a2, float t) {
  return 3 * (1 - 3 * a2 + 3 * a1) * t * t + 2 * (3 * a2 - 6 * a1) * t + 3 * a1;
}
static float cubic(float x1, float y1, float x2, float y2, float x) {
  float t = x;
  for (int j = 0; j < 8; j++) {
    float err = bezier(x1, x2, t) - x, d = bezier_slope(x1, x2, t);
    if (fabsf(err) < 1e-5f) return bezier(y1, y2, t);
    if (fabsf(d) < 1e-6f) break;
    t -= err / d;
  }
  float lo = 0, hi = 1;
  t = x;
  for (int j = 0; j < 30; j++) {
    float v = bezier(x1, x2, t);
    if (fabsf(v - x) < 1e-5f) break;
    if (v < x)
      lo = t;
    else
      hi = t;
    t = (lo + hi) / 2;
  }
  return bezier(y1, y2, t);
}

static float ease(uint8_t e, float x) {
  switch (e) {
    case RD_EASING_LINEAR:
      return x;
    case RD_EASING_EASE_IN:
      return cubic(0.42f, 0, 1, 1, x);
    case RD_EASING_EASE_OUT:
      return cubic(0, 0, 0.58f, 1, x);
    case RD_EASING_EASE_IN_OUT:
      return cubic(0.42f, 0, 0.58f, 1, x);
    default:
      return cubic(0.25f, 0.1f, 0.25f, 1, x);
  }
}

// State.

static rd__anim* anim_get(rd_ctx* ctx, uint32_t i) {
  rd_node_style* ns = &ctx->styles[i];
  if (ns->anim) return ns->anim;
  ns->anim = calloc(1, sizeof *ns->anim);
  if (ns->anim && !rd__chain_push(&ctx->animating, rd__handle(ctx, i))) {
    free(ns->anim);
    ns->anim = NULL;
  }
  return ns->anim;
}

void rd__anim_free(rd_node_style* ns) {
  free(ns->anim);
  ns->anim = NULL;
}

bool rd_ctx_animating(const rd_ctx* ctx) {
  if (!ctx) return false;
  for (uint32_t j = 0; j < ctx->animating.len; j++) {
    rd_node n = ctx->animating.nodes[j];
    if (rd__slot(ctx, n) && ctx->styles[n.index].anim && ctx->styles[n.index].anim->count) return true;
  }
  return false;
}

static bool hidden(const rd_style* s) { return s->display == RD_DISPLAY_NONE; }

// Height to or from auto: from uses the laid-out height; to is measured each frame.
static bool auto_height(int p, rd_val from, rd_val to) {
  return p == P_height && (from.unit == RD_UNIT_AUTO) != (to.unit == RD_UNIT_AUTO) &&
         (from.unit == RD_UNIT_AUTO ? to.unit == RD_UNIT_PX : from.unit == RD_UNIT_PX);
}

static void stop(rd__anim* a, int p) {
  if (!a || !a->active[p]) return;
  a->active[p] = false;
  a->count--;
}

// Whether `to` (a partial style) sets property p.
static bool sets(int p, const rd_style* s) {
  const void* v = cfield(s, p);
  switch (PROPS[p].kind) {
    case K_VAL:
      return ((const rd_val*)v)->unit != RD_UNIT_UNSET;
    case K_COLOR:
      return ((const rd_color*)v)->set;
    case K_EDGES:
    case K_CORNERS:
      for (int j = 0; j < 4; j++)
        if (((const rd_val*)v)[j].unit != RD_UNIT_UNSET) return true;
      return false;
    case K_SHADOW:
      return ((const rd_shadow*)v)->color.set;
    default:
      return false;
  }
}

// Called while computing node i's style, with its new style in next. Starts or retargets transitions
// for properties whose value changed, and leaves in next what to show now.
void rd__transition(rd_ctx* ctx, uint32_t i, rd_style* next) {
  rd_node_style* ns    = &ctx->styles[i];
  rd__anim*      a     = ns->anim;
  bool           first = !ns->computed_once;
  bool           ex    = a && a->pending;
  ns->computed_once    = true;
  if (first && !ex) return;

  float   dur   = next->transition_duration.unit == RD_UNIT_NUM ? fmaxf(0, next->transition_duration.value) : 0;
  float   delay = next->transition_delay.unit == RD_UNIT_NUM ? fmaxf(0, next->transition_delay.value) : 0;
  uint8_t mask  = next->transition_properties;
  if (!a && (dur <= 0 || hidden(next))) return;

  // What each property starts from: what's shown (or, for a first style, the new value), with an
  // explicit from on top.
  rd_style base = first ? *next : ns->computed;
  if (ex && a->pending_from_set) rd_style_merge(&base, &a->pending_from);
  bool was_hidden = hidden(&base);

  for (int p = 0; p < P_COUNT; p++) {
    if (PROPS[p].kind == K_OTHER) continue;
    bool    exp    = ex && a->pending_mask[p];
    float   d      = exp ? a->pending_duration : (GROUP[p] & mask) ? dur : 0;
    double  start  = exp ? a->pending_start : ctx->now + delay;
    uint8_t e      = exp ? a->pending_easing : next->transition_easing;
    void*   target = field(next, p);

    // A transition already heading to this value carries on.
    if (a && a->active[p] && !exp && equal(p, cfield(&a->to, p), target)) {
      copy(p, next, cfield(&ns->computed, p));
      continue;
    }
    const void* from = cfield(&base, p);
    if (equal(p, from, target) || d <= 0 || hidden(next) || was_hidden) {
      stop(a, p);
      continue;
    }
    rd_val conv    = {0};
    bool   to_auto = false;
    if (PROPS[p].kind == K_VAL && auto_height(p, *(const rd_val*)from, *(rd_val*)target)) {
      to_auto = ((rd_val*)target)->unit == RD_UNIT_AUTO;
      if (!to_auto) {
        conv = rd_px(ctx->layouts[i].rect.height);
        from = &conv;
      }
    } else if (!lerpable(p, from, target)) {
      stop(a, p);
      continue;
    }
    if (!a) a = anim_get(ctx, i);
    if (!a) return;
    copy(p, &a->from, from);
    copy(p, &a->to, target);
    a->start[p]    = start;
    a->duration[p] = d;
    a->easing[p]   = e;
    a->to_auto[p]  = to_auto;
    if (!a->active[p]) {
      a->active[p] = true;
      a->count++;
    }
    copy(p, next, from);
    double el = ctx->now - start;  // an explicit animation may have started before this computation
    if (el > 0 && !to_auto)
      lerp(p, field(next, p), cfield(&a->from, p), cfield(&a->to, p), ease(e, d > 0 ? (float)fmin(1, el / d) : 1));
  }
  if (a) {
    a->pending          = false;
    a->pending_from_set = false;
    memset(a->pending_mask, 0, sizeof a->pending_mask);
  }
}

// Advances every animation to ctx->now, writing the values into the shown (computed) styles.
void rd__anim_tick(rd_ctx* ctx) {
  rd__chain* c    = &ctx->animating;
  uint32_t   kept = 0;
  for (uint32_t j = 0; j < c->len; j++) {
    rd_node n = c->nodes[j];
    if (!rd__slot(ctx, n) || !ctx->styles[n.index].anim) continue;
    uint32_t       i  = n.index;
    rd_node_style* ns = &ctx->styles[i];
    rd__anim*      a  = ns->anim;
    bool           layout = false, inherited = false;
    for (int p = 0; p < P_COUNT && a->count; p++) {
      if (!a->active[p]) continue;
      double el = ctx->now - a->start[p];
      float  t  = el <= 0 ? 0 : a->duration[p] > 0 ? (float)fmin(1, el / a->duration[p]) : 1;
      void*  out = field(&ns->computed, p);
      if (t >= 1) {
        copy(p, &ns->computed, cfield(&a->to, p));
        stop(a, p);
      } else if (a->to_auto[p]) {
        rd_val end = rd_px(rd__natural_height(ctx, i));
        lerp(p, out, cfield(&a->from, p), &end, ease(a->easing[p], t));
      } else {
        lerp(p, out, cfield(&a->from, p), cfield(&a->to, p), ease(a->easing[p], t));
      }
      layout |= PROPS[p].layout;
      inherited |= PROPS[p].inherited;
    }
    ctx->paint_dirty = true;
    if (layout) rd__mark_layout_dirty(ctx, i);
    if (inherited)
      for (uint32_t ch = ctx->slots[i].first_child; ch; ch = ctx->slots[ch].next_sibling) rd__mark_style_dirty(ctx, ch);
    if (!a->count && !a->pending) {
      rd__anim_free(ns);
      continue;
    }
    c->nodes[kept++] = n;
  }
  c->len = kept;
}

bool rd_node_animate(rd_ctx* ctx, rd_node node, const rd_style* from, const rd_style* to, double seconds,
                     rd_easing easing, rd_timer_fn done, void* data) {
  if (!rd__slot(ctx, node) || !to) return false;
  rd__anim* a = anim_get(ctx, node.index);
  if (!a) return false;
  // Several calls before the next style computation add up.
  a->pending          = true;
  a->pending_duration = (float)fmax(0, seconds);
  a->pending_start    = ctx->now;
  a->pending_easing   = easing ? easing : RD_EASING_EASE;
  for (int p = 0; p < P_COUNT; p++) a->pending_mask[p] |= sets(p, to);
  if (from) {
    if (!a->pending_from_set) a->pending_from = (rd_style){0};
    rd_style_merge(&a->pending_from, from);
    a->pending_from_set = true;
  }
  rd_node_merge_style(ctx, node, to);
  if (done && !rd_ctx_set_timer(ctx, seconds, 0, done, data, NULL).index) return false;
  return true;
}
