#include <math.h>
#include <stdlib.h>

#include "events_internal.h"
#include "layout_internal.h"

typedef struct rd__anchor {
  rd_anchor desc;
  rd_side   side;  // after flipping
} rd__anchor;

rd_node rd_ctx_overlay(rd_ctx* ctx) {
  if (!ctx) return (rd_node){0};
  if (rd__slot(ctx, ctx->overlay)) {
    rd_slot* s = &ctx->slots[ctx->overlay.index];
    if (s->parent != ctx->root || s->next_sibling) rd_node_append(ctx, rd_ctx_root(ctx), ctx->overlay);  // moved away
    return ctx->overlay;
  }
  rd_node n = rd_node_create(ctx);
  if (rd_node_is_null(n)) return n;
  rd_node_append(ctx, rd_ctx_root(ctx), n);
  ctx->overlay = n;
  return n;
}

bool rd_node_set_anchor(rd_ctx* ctx, rd_node node, const rd_anchor* anchor) {
  if (!rd__slot(ctx, node)) return false;
  rd_node_layout* L = &ctx->layouts[node.index];
  if (!anchor) {
    free(L->anchor);
    L->anchor = NULL;
    return true;
  }
  if (!L->anchor) {
    L->anchor = malloc(sizeof *L->anchor);
    if (!L->anchor) return false;
    bool listed = false;  // cleared and set again before a layout pruned it
    for (uint32_t j = 0; j < ctx->anchored.len && !listed; j++) listed = rd_node_eq(ctx->anchored.nodes[j], node);
    if (!listed && !rd__chain_push(&ctx->anchored, node)) {
      free(L->anchor);
      L->anchor = NULL;
      return false;
    }
  }
  *L->anchor = (rd__anchor){*anchor, anchor->side};
  rd__mark_layout_dirty(ctx, node.index);
  return true;
}

rd_side rd_node_anchor_side(const rd_ctx* ctx, rd_node node) {
  return rd__slot(ctx, node) && ctx->layouts[node.index].anchor ? ctx->layouts[node.index].anchor->side : RD_SIDE_BOTTOM;
}

static bool hidden(const rd_ctx* ctx, uint32_t i) {
  for (; i; i = ctx->slots[i].parent)
    if (ctx->styles[i].computed.display == RD_DISPLAY_NONE) return true;
  return false;
}

static bool attached(const rd_ctx* ctx, uint32_t i) {
  while (ctx->slots[i].parent) i = ctx->slots[i].parent;
  return i == ctx->root;
}

// Start of the node along one axis, aligned against the anchor span [a, a + len).
static float align_on(rd_align align, float a, float len, float size) {
  if (align == RD_ALIGN_CENTER) return a + (len - size) / 2;
  if (align == RD_ALIGN_END) return a + len - size;
  return a;
}

static float clamp_into(float v, float size, float limit) { return fmaxf(0, fminf(v, limit - size)); }

static void place(rd_ctx* ctx, uint32_t i, rd__anchor* an, rd_rect view) {
  const rd_anchor* d = &an->desc;
  rd_rect          a = {d->x, d->y, 0, 0};
  if (!rd_node_is_null(d->node)) {
    if (!rd__slot(ctx, d->node) || !attached(ctx, d->node.index)) return;
    a = rd_node_bounds(ctx, d->node);
  }
  rd_node_layout* L = &ctx->layouts[i];
  uint32_t        p = ctx->slots[i].parent;

  if (d->match_width) {
    // Resolve the node's own height and bounds the way absolute layout does, with the width fixed.
    const rd_node_layout* P     = &ctx->layouts[p];
    const rd_style*       st    = &ctx->styles[i].computed;
    float                 cb[2] = {fmaxf(0, P->rect.width - P->border[EDGE_L] - P->border[EDGE_R]),
                                   fmaxf(0, P->rect.height - P->border[EDGE_T] - P->border[EDGE_B])};
    float                 pb[2], mn[2], mx[2];
    style_pb(st, cb[0], pb);
    size_bounds(st, pb, cb[0], cb[1], mn, mx);
    float w = clampf(a.width, mn[0], mx[0]);
    if (fabsf(L->rect.width - w) > 0.01f) {
      float h = resolve(st->height, cb[1]);
      if (isnan(h))
        h = rd__layout_node(ctx, i, (rd_layout_key){w, view.height, cb[0], cb[1], RD_MEASURE_EXACTLY, RD_MEASURE_AT_MOST},
                            false)
                .height;
      h = clampf(h, mn[1], mx[1]);
      rd__layout_node(ctx, i, (rd_layout_key){w, h, cb[0], cb[1], RD_MEASURE_EXACTLY, RD_MEASURE_EXACTLY}, true);
    }
  }
  float w = L->rect.width, h = L->rect.height;

  // Main axis: below/above or right/left of the anchor; flip when only the other side has room.
  bool    vertical = d->side == RD_SIDE_BOTTOM || d->side == RD_SIDE_TOP;
  rd_side side     = d->side;
  float   before   = vertical ? a.y - view.y : a.x - view.x;
  float   after    = vertical ? view.y + view.height - (a.y + a.height) : view.x + view.width - (a.x + a.width);
  float   need     = (vertical ? h : w) + d->offset;
  bool    forward  = side == RD_SIDE_BOTTOM || side == RD_SIDE_RIGHT;
  if (!d->no_flip && (forward ? after : before) < need && (forward ? before : after) > (forward ? after : before))
    side = (rd_side)(side ^ 1);  // BOTTOM <-> TOP, RIGHT <-> LEFT
  an->side = side;

  float x, y;
  if (vertical) {
    y = side == RD_SIDE_BOTTOM ? a.y + a.height + d->offset : a.y - d->offset - h;
    x = align_on(d->align, a.x, a.width, w);
  } else {
    x = side == RD_SIDE_RIGHT ? a.x + a.width + d->offset : a.x - d->offset - w;
    y = align_on(d->align, a.y, a.height, h);
  }
  if (!d->no_shift) {
    x = view.x + clamp_into(x - view.x, w, view.width);
    y = view.y + clamp_into(y - view.y, h, view.height);
  }

  rd_rect pb = rd_node_bounds(ctx, rd__handle(ctx, p));
  L->rect.x  = x - pb.x + ctx->layouts[p].scroll[0];
  L->rect.y  = y - pb.y + ctx->layouts[p].scroll[1];
}

void rd__place_anchored(rd_ctx* ctx) {
  rd__chain* c    = &ctx->anchored;
  rd_rect    view = ctx->layouts[ctx->root].rect;
  uint32_t   kept = 0;
  for (uint32_t j = 0; j < c->len; j++) {
    rd_node n = c->nodes[j];
    if (rd__slot(ctx, n) && ctx->layouts[n.index].anchor) c->nodes[kept++] = n;  // else destroyed or cleared: drop it
  }
  c->len = kept;
  // Tree order, so a node anchored inside another anchored node is placed after it.
  if (c->len > 1) {
    rd__tree_number(ctx);
    for (uint32_t j = 1; j < c->len; j++) {
      rd_node  n = c->nodes[j];
      uint32_t o = ctx->layouts[n.index].order, k = j;
      for (; k && ctx->layouts[c->nodes[k - 1].index].order > o; k--) c->nodes[k] = c->nodes[k - 1];
      c->nodes[k] = n;
    }
  }
  for (uint32_t j = 0; j < c->len; j++) {
    uint32_t i = c->nodes[j].index;
    if (attached(ctx, i) && !hidden(ctx, i)) place(ctx, i, ctx->layouts[i].anchor, view);
  }
}
