#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

#define PX0 \
  { 0, RD_UNIT_PX }
#define AUTO \
  { 0, RD_UNIT_AUTO }

static const rd_style default_style = {
    .display               = RD_DISPLAY_FLEX,
    .position              = RD_POSITION_RELATIVE,
    .overflow_x            = RD_OVERFLOW_VISIBLE,
    .overflow_y            = RD_OVERFLOW_VISIBLE,
    .flex_direction        = RD_FLEX_COLUMN,
    .flex_wrap             = RD_FLEX_NOWRAP,
    .justify_content       = RD_ALIGN_NORMAL,
    .align_items           = RD_ALIGN_STRETCH,
    .align_self            = RD_ALIGN_AUTO,
    .align_content         = RD_ALIGN_NORMAL,
    .justify_items         = RD_ALIGN_STRETCH,
    .justify_self          = RD_ALIGN_AUTO,
    .grid_template_columns = {0, true},
    .grid_template_rows    = {0, true},
    .grid_auto_columns     = {AUTO, AUTO},
    .grid_auto_rows        = {AUTO, AUTO},
    .grid_auto_flow        = RD_GRID_FLOW_ROW,
    .grid_column_start     = {0, RD_GRID_LINE_AUTO},
    .grid_column_end       = {0, RD_GRID_LINE_AUTO},
    .grid_row_start        = {0, RD_GRID_LINE_AUTO},
    .grid_row_end          = {0, RD_GRID_LINE_AUTO},
    .flex_grow             = {0, RD_UNIT_NUM},
    .flex_shrink           = {0, RD_UNIT_NUM},
    .flex_basis            = AUTO,
    .width                 = AUTO,
    .height                = AUTO,
    .min_width             = AUTO,
    .min_height            = AUTO,
    .max_width             = AUTO,
    .max_height            = AUTO,
    .margin                = {PX0, PX0, PX0, PX0},
    .padding               = {PX0, PX0, PX0, PX0},
    .border_width          = {PX0, PX0, PX0, PX0},
    .inset                 = {AUTO, AUTO, AUTO, AUTO},
    .row_gap               = PX0,
    .column_gap            = PX0,
    .background            = {0, 0, 0, 0, true},
    .border_color          = {0, 0, 0, 0, true},
    .border_radius         = {PX0, PX0, PX0, PX0},
    .opacity               = {1, RD_UNIT_NUM},
    .box_shadow            = {PX0, PX0, PX0, PX0, {0, 0, 0, 0, true}},
    .object_fit            = RD_OBJECT_FIT_FILL,
    .color                 = {0, 0, 0, 255, true},
    .font_size             = {16, RD_UNIT_PX},
    .line_height           = {1.2f, RD_UNIT_NUM},
    .text_align            = RD_TEXT_ALIGN_START,
    .font                  = {0, true},
    .white_space           = RD_WHITE_SPACE_NORMAL,
    .text_overflow         = RD_TEXT_OVERFLOW_CLIP,
    .user_select           = RD_USER_SELECT_AUTO,
    .line_clamp            = {0, RD_UNIT_NUM},
    .pointer_events        = RD_POINTER_EVENTS_AUTO,
    .focusable             = RD_FOCUSABLE_NONE,
    .cursor                = RD_CURSOR_AUTO,
    .scrollbar_width       = {8, RD_UNIT_PX},
    .scrollbar_color       = {128, 128, 128, 120, true},
    .scrollbar_track_color = {0, 0, 0, 0, true},
    .transition_duration   = {0, RD_UNIT_NUM},
    .transition_delay      = {0, RD_UNIT_NUM},
    .transition_easing     = RD_EASING_EASE,
    .transition_properties = RD_TRANSITION_ALL,
    .caret_color           = {0, 0, 0, 0, true},
    .selection_color       = {91, 140, 255, 90, true},
};

static void merge_val(rd_val* d, const rd_val* s) {
  if (s->unit != RD_UNIT_UNSET) *d = *s;
}
static void merge_color(rd_color* d, const rd_color* s) {
  if (s->set) *d = *s;
}
static void merge_edges(rd_edges* d, const rd_edges* s) {
  merge_val(&d->top, &s->top);
  merge_val(&d->right, &s->right);
  merge_val(&d->bottom, &s->bottom);
  merge_val(&d->left, &s->left);
}
static void merge_corners(rd_corners* d, const rd_corners* s) {
  merge_val(&d->top_left, &s->top_left);
  merge_val(&d->top_right, &s->top_right);
  merge_val(&d->bottom_right, &s->bottom_right);
  merge_val(&d->bottom_left, &s->bottom_left);
}

static void merge_tracks(rd_tracks* d, const rd_tracks* s) {
  if (s->set) *d = *s;
}
static void merge_track(rd_track* d, const rd_track* s) {
  if (s->min.unit != RD_UNIT_UNSET || s->max.unit != RD_UNIT_UNSET) *d = *s;
}
static void merge_grid_line(rd_grid_line* d, const rd_grid_line* s) {
  if (s->kind != RD_GRID_LINE_UNSET) *d = *s;
}

static void merge_font(rd_font* d, const rd_font* s) {
  if (s->set) *d = *s;
}

static void merge_shadow(rd_shadow* d, const rd_shadow* s) {
  if (s->color.set) *d = *s;
}

#define MERGE_FONT(d, s)      merge_font(&(d), &(s));
#define MERGE_SHADOW(d, s)    merge_shadow(&(d), &(s));
#define MERGE_TRACKS(d, s)    merge_tracks(&(d), &(s));
#define MERGE_TRACK(d, s)     merge_track(&(d), &(s));
#define MERGE_GRID_LINE(d, s) merge_grid_line(&(d), &(s));
#define MERGE_ENUM(d, s) \
  if (s) (d) = (s);
#define MERGE_VAL(d, s)     merge_val(&(d), &(s));
#define MERGE_COLOR(d, s)   merge_color(&(d), &(s));
#define MERGE_EDGES(d, s)   merge_edges(&(d), &(s));
#define MERGE_CORNERS(d, s) merge_corners(&(d), &(s));

void rd_style_merge(rd_style* dst, const rd_style* src) {
  if (!dst || !src) return;
#define X(name, type, kind, inherited, layout) MERGE_##kind(dst->name, src->name)
  RD_STYLE_PROPS(X)
#undef X
}

static bool eq_val(rd_val a, rd_val b) { return a.unit == b.unit && a.value == b.value; }
static bool eq_color(rd_color a, rd_color b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a && a.set == b.set;
}
static bool eq_edges(rd_edges a, rd_edges b) {
  return eq_val(a.top, b.top) && eq_val(a.right, b.right) && eq_val(a.bottom, b.bottom) &&
         eq_val(a.left, b.left);
}
static bool eq_corners(rd_corners a, rd_corners b) {
  return eq_val(a.top_left, b.top_left) && eq_val(a.top_right, b.top_right) &&
         eq_val(a.bottom_right, b.bottom_right) && eq_val(a.bottom_left, b.bottom_left);
}

static bool eq_tracks(rd_tracks a, rd_tracks b) { return a.id == b.id && a.set == b.set; }
static bool eq_track(rd_track a, rd_track b) { return eq_val(a.min, b.min) && eq_val(a.max, b.max); }
static bool eq_grid_line(rd_grid_line a, rd_grid_line b) { return a.kind == b.kind && a.value == b.value; }

static bool eq_font(rd_font a, rd_font b) { return a.id == b.id && a.set == b.set; }

static bool eq_shadow(rd_shadow a, rd_shadow b) {
  return eq_val(a.x, b.x) && eq_val(a.y, b.y) && eq_val(a.blur, b.blur) && eq_val(a.spread, b.spread) &&
         eq_color(a.color, b.color);
}

#define EQ_SHADOW(a, b)    eq_shadow(a, b)
#define EQ_FONT(a, b)      eq_font(a, b)
#define EQ_TRACKS(a, b)    eq_tracks(a, b)
#define EQ_TRACK(a, b)     eq_track(a, b)
#define EQ_GRID_LINE(a, b) eq_grid_line(a, b)
#define EQ_ENUM(a, b)      ((a) == (b))
#define EQ_VAL(a, b)       eq_val(a, b)
#define EQ_COLOR(a, b)     eq_color(a, b)
#define EQ_EDGES(a, b)     eq_edges(a, b)
#define EQ_CORNERS(a, b)   eq_corners(a, b)

enum { DIFF_ANY       = 1,
       DIFF_INHERITED = 2,
       DIFF_LAYOUT    = 4 };

static unsigned style_diff(const rd_style* a, const rd_style* b) {
  unsigned d = 0;
#define X(name, type, kind, inherited, layout) \
  if (!EQ_##kind(a->name, b->name))            \
    d |= DIFF_ANY | (inherited ? DIFF_INHERITED : 0) | (layout ? DIFF_LAYOUT : 0);

  RD_STYLE_PROPS(X)
#undef X
  return d;
}

#ifndef NDEBUG
static bool val_set(rd_val v) { return v.unit != RD_UNIT_UNSET; }

#define SET_ENUM(v)  ((v) != 0)
#define SET_VAL(v)   val_set(v)
#define SET_COLOR(v) ((v).set)
static bool edges_set(rd_edges e) {
  return val_set(e.top) && val_set(e.right) && val_set(e.bottom) && val_set(e.left);
}
static bool corners_set(rd_corners c) {
  return val_set(c.top_left) && val_set(c.top_right) && val_set(c.bottom_right) && val_set(c.bottom_left);
}

#define SET_SHADOW(v)    (val_set((v).x) && val_set((v).y) && val_set((v).blur) && val_set((v).spread) && (v).color.set)
#define SET_FONT(v)      ((v).set)
#define SET_TRACKS(v)    ((v).set)
#define SET_TRACK(v)     (val_set((v).min) && val_set((v).max))
#define SET_GRID_LINE(v) ((v).kind != RD_GRID_LINE_UNSET)
#define SET_EDGES(v)     edges_set(v)
#define SET_CORNERS(v)   corners_set(v)

static bool style_complete(const rd_style* s) {
#define X(name, type, kind, inherited, layout) \
  if (!SET_##kind(s->name)) return false;

  RD_STYLE_PROPS(X)
#undef X
  return true;
}
#endif

static void build(const rd_ctx* ctx, uint32_t i, const rd_style* parent, rd_style* out) {
  *out = default_style;
  if (parent) {
#define X(name, type, kind, inherited, layout) \
  if (inherited) out->name = parent->name;

    RD_STYLE_PROPS(X)
#undef X
  }
  assert(ctx->styles && i);
  const rd_node_style* ns = &ctx->styles[i];
  for (uint32_t c = 0; c < ns->class_count; c++)
    rd_style_merge(out, &ctx->classes[ns->classes[c] - 1].base);
  uint8_t states = ns->states;
  for (uint32_t j = i; ctx->styles[j].share_states && ctx->slots[j].parent;) {
    j = ctx->slots[j].parent;
    states |= ctx->styles[j].states;
  }
  for (uint32_t k = 0; k < RD_STATE_COUNT; k++) {
    if (!(states & (1u << k))) continue;
    for (uint32_t c = 0; c < ns->class_count; c++) {
      const rd_class_data* cd = &ctx->classes[ns->classes[c] - 1];
      if (cd->state_mask & (1u << k)) rd_style_merge(out, &cd->states[k]);
    }
  }
  if (ns->inline_style) rd_style_merge(out, ns->inline_style);
  // The overlay always covers the root, whatever its classes and inline style say.
  if (i == ctx->overlay.index && ctx->slots[i].gen == ctx->overlay.gen) {
    rd_val px0 = {0, RD_UNIT_PX}, autov = {0, RD_UNIT_AUTO};
    out->position = RD_POSITION_ABSOLUTE;
    out->inset = out->margin = (rd_edges){px0, px0, px0, px0};
    out->width = out->height = out->min_width = out->min_height = out->max_width = out->max_height = autov;
  }
}

static void compute_rec(rd_ctx* ctx, uint32_t i, const rd_style* parent, bool force) {
  rd_slot*       s              = &ctx->slots[i];
  rd_node_style* ns             = &ctx->styles[i];
  bool           force_children = false;
  if (force || (s->flags & RD_FLAG_STYLE_DIRTY)) {
    rd_style next;
    build(ctx, i, parent, &next);
    rd__transition(ctx, i, &next);
    unsigned d = style_diff(&next, &ns->computed);
    if (d) {
      ns->computed     = next;
      ctx->paint_dirty = true;
      if (d & DIFF_LAYOUT) rd__mark_layout_dirty(ctx, i);
      force_children = d & DIFF_INHERITED;
    }
  }
  bool descend = force_children || (s->flags & RD_FLAG_SUBTREE_STYLE_DIRTY);
  s->flags &= ~(RD_FLAG_STYLE_DIRTY | RD_FLAG_SUBTREE_STYLE_DIRTY);
  if (!descend) return;
  for (uint32_t c = s->first_child; c; c = ctx->slots[c].next_sibling)
    compute_rec(ctx, c, &ns->computed, force_children);
}

void rd__style_check_defaults(void) { assert(style_complete(&default_style)); }

void rd_ctx_compute_styles(rd_ctx* ctx) {
  if (!ctx) return;
  compute_rec(ctx, ctx->root, NULL, false);
}

const rd_style* rd_node_computed_style(rd_ctx* ctx, rd_node node) {
  if (!rd__slot(ctx, node)) return NULL;
  uint32_t top = node.index;
  while (ctx->slots[top].parent) top = ctx->slots[top].parent;
  compute_rec(ctx, top, NULL, false);
  return &ctx->styles[node.index].computed;
}

void rd__mark_style_dirty(rd_ctx* ctx, uint32_t i) {
  ctx->slots[i].flags |= RD_FLAG_STYLE_DIRTY;
  for (uint32_t p = ctx->slots[i].parent; p; p = ctx->slots[p].parent) {
    if (ctx->slots[p].flags & RD_FLAG_SUBTREE_STYLE_DIRTY) break;
    ctx->slots[p].flags |= RD_FLAG_SUBTREE_STYLE_DIRTY;
  }
}

// No early exit: subtrees layout never visits (display none, measured leaves, detached) keep their
// flags across passes, so an already-dirty node doesn't imply dirty ancestors.
void rd__mark_layout_dirty(rd_ctx* ctx, uint32_t i) {
  for (; i; i = ctx->slots[i].parent) ctx->slots[i].flags |= RD_FLAG_LAYOUT_DIRTY;
}

void rd__style_slot_free(rd_ctx* ctx, uint32_t i) {
  rd_node_style* ns = &ctx->styles[i];
  free(ns->inline_style);
  free(ns->classes);
  ns->inline_style = NULL;
  ns->classes      = NULL;
  ns->class_count = ns->class_cap = 0;
  rd__anim_free(ns);
  ns->computed_once = false;
}

void rd__style_ctx_free(rd_ctx* ctx) {
  free(ctx->track_pool);
  free(ctx->track_lists);
  free(ctx->classes);
  ctx->classes     = NULL;
  ctx->class_count = ctx->class_cap = 0;
}

static bool reserve(void** p, uint32_t* cap, uint32_t need, size_t elem) {
  if (need <= *cap) return true;
  uint32_t n = *cap ? *cap : 16;
  while (n < need) n *= 2;
  void* q = realloc(*p, n * elem);
  if (!q) return false;
  *p   = q;
  *cap = n;
  return true;
}

rd_tracks rd_tracks_create(rd_ctx* ctx, const rd_track* tracks, uint32_t count) {
  if (!ctx || (count && !tracks)) return (rd_tracks){0};
  if (!reserve((void**)&ctx->track_pool, &ctx->track_pool_cap, ctx->track_pool_len + count, sizeof *tracks) ||
      !reserve((void**)&ctx->track_lists, &ctx->track_list_cap, (ctx->track_list_count + 1) * 2, sizeof(uint32_t)))
    return (rd_tracks){0};
  if (count) memcpy(&ctx->track_pool[ctx->track_pool_len], tracks, count * sizeof *tracks);
  size_t at                = (size_t)ctx->track_list_count * 2;
  ctx->track_lists[at]     = ctx->track_pool_len;
  ctx->track_lists[at + 1] = count;
  ctx->track_pool_len += count;
  ctx->track_list_count++;
  return (rd_tracks){ctx->track_list_count, true};
}

const rd_track* rd_tracks_get(const rd_ctx* ctx, rd_tracks tracks, uint32_t* count) {
  *count = 0;
  if (!ctx || tracks.id == 0 || tracks.id > ctx->track_list_count) return NULL;
  size_t at = (size_t)(tracks.id - 1) * 2;
  *count    = ctx->track_lists[at + 1];
  return *count ? &ctx->track_pool[ctx->track_lists[at]] : NULL;
}

static rd_class_data* class_get(const rd_ctx* ctx, rd_class cls) {
  if (!ctx || cls.id == 0 || cls.id > ctx->class_count) return NULL;
  return &ctx->classes[cls.id - 1];
}

static bool node_has_class(const rd_node_style* ns, uint32_t id) {
  for (uint32_t c = 0; c < ns->class_count; c++)
    if (ns->classes[c] == id) return true;
  return false;
}

static void invalidate_class(rd_ctx* ctx, uint32_t id) {
  for (uint32_t i = 1; i < ctx->len; i++)
    if ((ctx->slots[i].flags & RD_SLOT_ALIVE) && node_has_class(&ctx->styles[i], id))
      rd__mark_style_dirty(ctx, i);
}

rd_class rd_class_create(rd_ctx* ctx, const rd_style* base) {
  if (!ctx) return (rd_class){0};
  if (ctx->class_count == ctx->class_cap) {
    uint32_t       cap = ctx->class_cap ? ctx->class_cap * 2 : 16;
    rd_class_data* c   = realloc(ctx->classes, cap * sizeof *c);
    if (!c) return (rd_class){0};
    ctx->classes   = c;
    ctx->class_cap = cap;
  }
  rd_class_data* cd = &ctx->classes[ctx->class_count++];
  memset(cd, 0, sizeof *cd);
  if (base) cd->base = *base;
  return (rd_class){ctx->class_count};
}

bool rd_class_set(rd_ctx* ctx, rd_class cls, const rd_style* base) {
  rd_class_data* cd = class_get(ctx, cls);
  if (!cd) return false;
  if (base)
    cd->base = *base;
  else
    memset(&cd->base, 0, sizeof cd->base);
  invalidate_class(ctx, cls.id);
  return true;
}

static int state_index(rd_state state) {
  for (int k = 0; k < RD_STATE_COUNT; k++)
    if (state == (1u << k)) return k;
  return -1;
}

bool rd_class_set_state(rd_ctx* ctx, rd_class cls, rd_state state, const rd_style* style) {
  rd_class_data* cd = class_get(ctx, cls);
  int            k  = state_index(state);
  if (!cd || k < 0) return false;
  if (style) {
    cd->states[k] = *style;
    cd->state_mask |= state;
  } else {
    memset(&cd->states[k], 0, sizeof cd->states[k]);
    cd->state_mask &= (uint8_t)~state;
  }
  invalidate_class(ctx, cls.id);
  return true;
}

bool rd_node_set_style(rd_ctx* ctx, rd_node node, const rd_style* style) {
  if (!rd__slot(ctx, node)) return false;
  rd_node_style* ns = &ctx->styles[node.index];
  if (!style) {
    free(ns->inline_style);
    ns->inline_style = NULL;
  } else {
    if (!ns->inline_style) {
      ns->inline_style = malloc(sizeof *ns->inline_style);
      if (!ns->inline_style) return false;
    }
    *ns->inline_style = *style;
  }
  rd__mark_style_dirty(ctx, node.index);
  return true;
}

bool rd_node_merge_style(rd_ctx* ctx, rd_node node, const rd_style* style) {
  if (!rd__slot(ctx, node) || !style) return false;
  rd_node_style* ns = &ctx->styles[node.index];
  if (!ns->inline_style) {
    ns->inline_style = calloc(1, sizeof *ns->inline_style);
    if (!ns->inline_style) return false;
  }
  rd_style_merge(ns->inline_style, style);
  rd__mark_style_dirty(ctx, node.index);
  return true;
}

bool rd_node_add_class(rd_ctx* ctx, rd_node node, rd_class cls) {
  if (!rd__slot(ctx, node) || !class_get(ctx, cls)) return false;
  rd_node_style* ns = &ctx->styles[node.index];
  if (node_has_class(ns, cls.id)) return true;
  if (ns->class_count == ns->class_cap) {
    uint32_t  cap = ns->class_cap ? ns->class_cap * 2 : 4;
    uint32_t* c   = realloc(ns->classes, cap * sizeof *c);
    if (!c) return false;
    ns->classes   = c;
    ns->class_cap = cap;
  }
  ns->classes[ns->class_count++] = cls.id;
  rd__mark_style_dirty(ctx, node.index);
  return true;
}

bool rd_node_remove_class(rd_ctx* ctx, rd_node node, rd_class cls) {
  if (!rd__slot(ctx, node)) return false;
  rd_node_style* ns = &ctx->styles[node.index];
  for (uint32_t c = 0; c < ns->class_count; c++) {
    if (ns->classes[c] != cls.id) continue;
    memmove(&ns->classes[c], &ns->classes[c + 1], (ns->class_count - c - 1) * sizeof *ns->classes);
    ns->class_count--;
    rd__mark_style_dirty(ctx, node.index);
    return true;
  }
  return false;
}

void rd_node_clear_classes(rd_ctx* ctx, rd_node node) {
  if (!rd__slot(ctx, node)) return;
  ctx->styles[node.index].class_count = 0;
  rd__mark_style_dirty(ctx, node.index);
}

bool rd_node_has_class(const rd_ctx* ctx, rd_node node, rd_class cls) {
  return rd__slot(ctx, node) && node_has_class(&ctx->styles[node.index], cls.id);
}

// Children sharing a node's states restyle when they change, and so on down.
static void mark_sharers(rd_ctx* ctx, uint32_t i) {
  for (uint32_t c = ctx->slots[i].first_child; c; c = ctx->slots[c].next_sibling) {
    if (!ctx->styles[c].share_states) continue;
    rd__mark_style_dirty(ctx, c);
    mark_sharers(ctx, c);
  }
}

void rd_node_set_share_states(rd_ctx* ctx, rd_node node, bool share) {
  if (!rd__slot(ctx, node) || ctx->styles[node.index].share_states == share) return;
  ctx->styles[node.index].share_states = share;
  rd__mark_style_dirty(ctx, node.index);
  mark_sharers(ctx, node.index);
}

void rd_node_set_states(rd_ctx* ctx, rd_node node, uint8_t states, bool on) {
  if (!rd__slot(ctx, node)) return;
  rd_node_style* ns   = &ctx->styles[node.index];
  uint8_t        next = on ? (ns->states | states) : (ns->states & (uint8_t)~states);
  uint8_t        diff = ns->states ^ next;
  ns->states          = next;
  if (!diff) return;
  mark_sharers(ctx, node.index);
  for (uint32_t c = 0; c < ns->class_count; c++) {
    if (ctx->classes[ns->classes[c] - 1].state_mask & diff) {
      rd__mark_style_dirty(ctx, node.index);
      return;
    }
  }
}

uint8_t rd_node_states(const rd_ctx* ctx, rd_node node) {
  return rd__slot(ctx, node) ? ctx->styles[node.index].states : 0;
}
