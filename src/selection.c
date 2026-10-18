#include <stdlib.h>
#include <string.h>

#include "input_internal.h"
#include "layout_internal.h"
#include "selection_internal.h"

#ifdef __APPLE__
static const uint8_t SHORTCUT_MOD = RD_MOD_SUPER;
#else
static const uint8_t SHORTCUT_MOD = RD_MOD_CTRL;
#endif

// Text that drag selection can reach: displayed, not an input, user_select: auto.
static bool selectable(const rd_ctx* ctx, uint32_t i) {
  if (!rd__is_text(ctx, i) || rd__text_get(ctx, i)->input) return false;
  if (ctx->styles[i].computed.user_select == RD_USER_SELECT_NONE) return false;
  for (uint32_t n = i; n; n = ctx->slots[n].parent)
    if (ctx->styles[n].computed.display == RD_DISPLAY_NONE) return false;
  return true;
}

static bool has_selection(const rd_ctx* ctx) {
  return rd__slot(ctx, ctx->sel_anchor) && rd__slot(ctx, ctx->sel_focus) &&
         !(rd_node_eq(ctx->sel_anchor, ctx->sel_focus) && ctx->sel_anchor_off == ctx->sel_focus_off);
}

static void changed(rd_ctx* ctx) {
  ctx->paint_dirty = true;
  rd__fire(ctx, RD_EVENT_SELECTION_CHANGE, rd_ctx_root(ctx));
}

static void set(rd_ctx* ctx, rd_node anchor, size_t aoff, rd_node focus, size_t foff) {
  bool before = has_selection(ctx);
  bool same   = rd_node_eq(anchor, ctx->sel_anchor) && rd_node_eq(focus, ctx->sel_focus) &&
              aoff == ctx->sel_anchor_off && foff == ctx->sel_focus_off;
  ctx->sel_anchor     = anchor;
  ctx->sel_focus      = focus;
  ctx->sel_anchor_off = aoff;
  ctx->sel_focus_off  = foff;
  if (!same && (before || has_selection(ctx))) changed(ctx);
}

void rd_ctx_clear_selection(rd_ctx* ctx) {
  if (!ctx) return;
  ctx->selecting = false;
  set(ctx, (rd_node){0}, 0, (rd_node){0}, 0);
}

static size_t clamp_offset(const rd_ctx* ctx, rd_node n, size_t off) {
  size_t len = rd__text_get(ctx, n.index)->len;
  return off < len ? off : len;
}

bool rd_ctx_select(rd_ctx* ctx, rd_node anchor, size_t anchor_offset, rd_node focus, size_t focus_offset) {
  if (!rd__slot(ctx, anchor) || !rd__slot(ctx, focus) || !rd__is_text(ctx, anchor.index) ||
      !rd__is_text(ctx, focus.index))
    return false;
  set(ctx, anchor, clamp_offset(ctx, anchor, anchor_offset), focus, clamp_offset(ctx, focus, focus_offset));
  return true;
}

bool rd_ctx_selection(const rd_ctx* ctx, rd_node* anchor, size_t* anchor_offset, rd_node* focus,
                      size_t* focus_offset) {
  if (!ctx || !has_selection(ctx)) return false;
  if (anchor) *anchor = ctx->sel_anchor;
  if (anchor_offset) *anchor_offset = ctx->sel_anchor_off;
  if (focus) *focus = ctx->sel_focus;
  if (focus_offset) *focus_offset = ctx->sel_focus_off;
  return true;
}

// Ordering.

// Numbers attached nodes in pre-order; detached ones get UINT32_MAX.
static void number(rd_ctx* ctx) {
  for (uint32_t i = 1; i < ctx->len; i++) ctx->layouts[i].order = UINT32_MAX;
  uint32_t next = 0;
  for (uint32_t i = ctx->root; i;) {
    ctx->layouts[i].order = next++;
    if (ctx->slots[i].first_child) {
      i = ctx->slots[i].first_child;
      continue;
    }
    while (i && !ctx->slots[i].next_sibling) i = ctx->slots[i].parent;
    if (i) i = ctx->slots[i].next_sibling;
  }
}

bool rd__selection_prepare(rd_ctx* ctx) {
  ctx->sel_active = false;
  if (!has_selection(ctx)) return false;
  number(ctx);
  uint32_t a = ctx->layouts[ctx->sel_anchor.index].order, f = ctx->layouts[ctx->sel_focus.index].order;
  if (a == UINT32_MAX || f == UINT32_MAX) return false;
  bool forward       = a < f || (a == f && ctx->sel_anchor_off <= ctx->sel_focus_off);
  ctx->sel_start     = forward ? a : f;
  ctx->sel_end       = forward ? f : a;
  ctx->sel_start_off = forward ? ctx->sel_anchor_off : ctx->sel_focus_off;
  ctx->sel_end_off   = forward ? ctx->sel_focus_off : ctx->sel_anchor_off;
  ctx->sel_active    = true;
  return true;
}

bool rd__selection_range(const rd_ctx* ctx, uint32_t i, size_t* lo, size_t* hi) {
  if (!ctx->sel_active || !selectable(ctx, i)) return false;
  uint32_t o = ctx->layouts[i].order;
  if (o < ctx->sel_start || o > ctx->sel_end) return false;
  size_t len = rd__text_get(ctx, i)->len;
  *lo        = o == ctx->sel_start ? ctx->sel_start_off : 0;
  *hi        = o == ctx->sel_end ? ctx->sel_end_off : len;
  if (*hi > len) *hi = len;
  return *lo < *hi;
}

// Pointer.

typedef struct nearest {
  float    x, y, dy, dx;
  uint32_t node;
} nearest;

static float gap(float v, float lo, float hi) { return v < lo ? lo - v : v > hi ? v - hi
                                                                                : 0; }

// The selectable text in a subtree closest to a point: vertical distance first, then horizontal.
static void find_nearest(const rd_ctx* ctx, uint32_t i, nearest* best) {
  if (ctx->styles[i].computed.display == RD_DISPLAY_NONE) return;
  if (selectable(ctx, i)) {
    rd_rect r  = rd_node_bounds(ctx, rd__handle(ctx, i));
    float   dy = gap(best->y, r.y, r.y + r.height), dx = gap(best->x, r.x, r.x + r.width);
    if (!best->node || dy < best->dy || (dy == best->dy && dx < best->dx)) *best = (nearest){best->x, best->y, dy, dx, i};
    return;
  }
  for (uint32_t c = ctx->slots[i].first_child; c; c = ctx->slots[c].next_sibling) find_nearest(ctx, c, best);
}

static bool position_at(rd_ctx* ctx, uint32_t hit, float x, float y, rd_node* node, size_t* off) {
  nearest best = {x, y, 0, 0, 0};
  find_nearest(ctx, hit ? hit : ctx->root, &best);
  if (!best.node) return false;
  *node = rd__handle(ctx, best.node);
  *off  = rd__text_byte_at(ctx, best.node, x, y, 0);
  return true;
}

void rd__selection_begin(rd_ctx* ctx, uint32_t hit, float x, float y) {
  rd_node n;
  size_t  off;
  if (!hit || !position_at(ctx, hit, x, y, &n, &off)) {
    rd_ctx_clear_selection(ctx);
    return;
  }
  set(ctx, n, off, n, off);
  ctx->selecting = true;
}

void rd__selection_drag(rd_ctx* ctx, uint32_t hit, float x, float y) {
  rd_node n;
  size_t  off;
  if (!ctx->selecting || !rd__slot(ctx, ctx->sel_anchor)) return;
  if (position_at(ctx, hit, x, y, &n, &off)) set(ctx, ctx->sel_anchor, ctx->sel_anchor_off, n, off);
}

// Text and copying.

typedef struct buffer {
  char*  data;
  size_t len, cap;
} buffer;

static void append(buffer* b, const char* s, size_t n) {
  if (b->len + n + 1 > b->cap) {
    size_t cap = b->cap ? b->cap * 2 : 256;
    while (cap < b->len + n + 1) cap *= 2;
    char* p = realloc(b->data, cap);
    if (!p) return;
    b->data = p;
    b->cap  = cap;
  }
  if (!b->data) return;
  memcpy(b->data + b->len, s, n);
  b->len += n;
  b->data[b->len] = 0;
}

static buffer collect(rd_ctx* ctx) {
  buffer b = {0};
  if (!rd__selection_prepare(ctx)) return b;
  bool first = true;
  for (uint32_t i = ctx->root; i;) {
    size_t lo, hi;
    if (rd__selection_range(ctx, i, &lo, &hi)) {
      if (!first) append(&b, "\n", 1);
      append(&b, rd__text_get(ctx, i)->utf8 + lo, hi - lo);
      first = false;
    }
    if (ctx->slots[i].first_child) {
      i = ctx->slots[i].first_child;
      continue;
    }
    while (i && !ctx->slots[i].next_sibling) i = ctx->slots[i].parent;
    if (i) i = ctx->slots[i].next_sibling;
  }
  return b;
}

size_t rd_ctx_selection_text(rd_ctx* ctx, char* buf, size_t cap) {
  if (!ctx) return 0;
  buffer b   = collect(ctx);
  size_t len = b.len;
  if (buf && cap) {
    size_t n = len < cap - 1 ? len : cap - 1;
    if (n) memcpy(buf, b.data, n);
    buf[n] = 0;
  }
  free(b.data);
  return len;
}

static void select_all(rd_ctx* ctx) {
  uint32_t first = 0, last = 0;
  for (uint32_t i = ctx->root; i;) {
    if (selectable(ctx, i)) {
      if (!first) first = i;
      last = i;
    }
    if (ctx->slots[i].first_child) {
      i = ctx->slots[i].first_child;
      continue;
    }
    while (i && !ctx->slots[i].next_sibling) i = ctx->slots[i].parent;
    if (i) i = ctx->slots[i].next_sibling;
  }
  if (first) set(ctx, rd__handle(ctx, first), 0, rd__handle(ctx, last), rd__text_get(ctx, last)->len);
}

bool rd__selection_key(rd_ctx* ctx, const rd_event* ev) {
  bool shortcut = ev->mods & SHORTCUT_MOD;
  if (shortcut && ev->key == RD_KEY_C && has_selection(ctx)) {
    buffer b = collect(ctx);
    if (b.data) rd__clipboard_set(ctx, b.data);
    free(b.data);
    return true;
  }
  if (shortcut && ev->key == RD_KEY_A) {
    select_all(ctx);
    return true;
  }
  if (ev->key == RD_KEY_ESCAPE && has_selection(ctx)) rd_ctx_clear_selection(ctx);
  return false;
}
