#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

static bool grow(rd_ctx* ctx) {
  uint32_t cap = ctx->cap * 2;
  rd_slot* s   = realloc(ctx->slots, cap * sizeof *s);
  if (!s) return false;
  ctx->slots        = s;
  rd_node_style* st = realloc(ctx->styles, cap * sizeof *st);
  if (!st) return false;
  ctx->styles       = st;
  rd_node_layout* l = realloc(ctx->layouts, cap * sizeof *l);
  if (!l) return false;
  ctx->layouts      = l;
  rd_node_events* e = realloc(ctx->events, cap * sizeof *e);
  if (!e) return false;
  ctx->events = e;
  ctx->cap    = cap;
  return true;
}

static uint32_t slot_alloc(rd_ctx* ctx) {
  uint32_t i;
  if (ctx->free_head) {
    i              = ctx->free_head;
    ctx->free_head = ctx->slots[i].next_sibling;
  } else {
    if (ctx->len == ctx->cap && !grow(ctx)) return 0;
    i                 = ctx->len++;
    ctx->slots[i].gen = 0;
  }
  ctx->slots[i] = (rd_slot){
      .gen   = ctx->slots[i].gen + 1,
      .flags = RD_SLOT_ALIVE | RD_FLAG_STYLE_DIRTY | RD_FLAG_LAYOUT_DIRTY,
  };
  memset(&ctx->styles[i], 0, sizeof ctx->styles[i]);
  memset(&ctx->layouts[i], 0, sizeof ctx->layouts[i]);
  ctx->layouts[i].baseline = NAN;
  memset(&ctx->events[i], 0, sizeof ctx->events[i]);
  ctx->live++;
  return i;
}

static void slot_free(rd_ctx* ctx, uint32_t i) {
  rd_slot*      s  = &ctx->slots[i];
  void*         ud = s->userdata;
  rd_destroy_fn fn = s->destroy;
  rd__style_slot_free(ctx, i);
  rd__layout_slot_free(ctx, i);
  rd__events_slot_free(ctx, i);
  s->flags        = 0;
  s->userdata     = NULL;
  s->destroy      = NULL;
  s->next_sibling = ctx->free_head;
  ctx->free_head  = i;
  ctx->live--;
  if (fn) fn(ud);
}

static void unlink_node(rd_ctx* ctx, uint32_t i) {
  rd_slot* s = &ctx->slots[i];
  if (!s->parent) return;
  rd_slot* p = &ctx->slots[s->parent];
  if (s->prev_sibling)
    ctx->slots[s->prev_sibling].next_sibling = s->next_sibling;
  else
    p->first_child = s->next_sibling;
  if (s->next_sibling)
    ctx->slots[s->next_sibling].prev_sibling = s->prev_sibling;
  else
    p->last_child = s->prev_sibling;
  p->child_count--;
  s->parent = s->next_sibling = s->prev_sibling = 0;
}

static void link_node(rd_ctx* ctx, uint32_t parent, uint32_t i, uint32_t ref) {
  rd_slot* s      = &ctx->slots[i];
  rd_slot* p      = &ctx->slots[parent];
  s->parent       = parent;
  s->next_sibling = ref;
  s->prev_sibling = ref ? ctx->slots[ref].prev_sibling : p->last_child;
  if (s->prev_sibling)
    ctx->slots[s->prev_sibling].next_sibling = i;
  else
    p->first_child = i;
  if (ref)
    ctx->slots[ref].prev_sibling = i;
  else
    p->last_child = i;
  p->child_count++;
}

static void destroy_rec(rd_ctx* ctx, uint32_t i) {
  uint32_t c = ctx->slots[i].first_child;
  while (c) {
    uint32_t next = ctx->slots[c].next_sibling;
    destroy_rec(ctx, c);
    c = next;
  }
  slot_free(ctx, i);
}

rd_ctx* rd_ctx_create(void) {
  rd_ctx* ctx = calloc(1, sizeof *ctx);
  if (!ctx) return NULL;
  ctx->cap     = 64;
  ctx->slots   = calloc(ctx->cap, sizeof *ctx->slots);
  ctx->styles  = calloc(ctx->cap, sizeof *ctx->styles);
  ctx->layouts = calloc(ctx->cap, sizeof *ctx->layouts);
  ctx->events  = calloc(ctx->cap, sizeof *ctx->events);
  if (!ctx->slots || !ctx->styles || !ctx->layouts || !ctx->events) {
    free(ctx->slots);
    free(ctx->styles);
    free(ctx->layouts);
    free(ctx->events);
    free(ctx);
    return NULL;
  }
  ctx->len  = 1;
  ctx->root = slot_alloc(ctx);
  rd__style_check_defaults();
  return ctx;
}

void rd_ctx_destroy(rd_ctx* ctx) {
  if (!ctx) return;
  for (uint32_t i = 1; i < ctx->len; i++) {
    rd_slot* s = &ctx->slots[i];
    if (!(s->flags & RD_SLOT_ALIVE)) continue;
    rd__style_slot_free(ctx, i);
    rd__layout_slot_free(ctx, i);
    rd__events_slot_free(ctx, i);
    if (s->destroy) s->destroy(s->userdata);
  }
  rd__style_ctx_free(ctx);
  rd__font_ctx_free(ctx);
  for (uint32_t i = 0; i < ctx->hook_count; i++)
    if (ctx->hooks[i].destroy) ctx->hooks[i].destroy(ctx->hooks[i].data);
  free(ctx->hooks);
  for (uint32_t i = 0; i < ctx->extension_count; i++)
    if (ctx->extensions[i].destroy) ctx->extensions[i].destroy(ctx->extensions[i].data);
  free(ctx->extensions);
  rd__images_free(ctx);
  rd__timers_free(ctx);
  rd__render_ctx_free(ctx);
  rd__events_ctx_free(ctx);
  free(ctx->events);
  free(ctx->layouts);
  free(ctx->styles);
  free(ctx->slots);
  free(ctx);
}

rd_node  rd_ctx_root(const rd_ctx* ctx) { return ctx ? rd__handle(ctx, ctx->root) : (rd_node){0}; }
uint32_t rd_ctx_node_count(const rd_ctx* ctx) { return ctx ? ctx->live : 0; }

rd_node rd_node_create(rd_ctx* ctx) { return ctx ? rd__handle(ctx, slot_alloc(ctx)) : (rd_node){0}; }

void rd_node_destroy(rd_ctx* ctx, rd_node node) {
  rd_slot* s = rd__slot(ctx, node);
  if (!s || node.index == ctx->root) return;
  if (s->parent) rd__mark_layout_dirty(ctx, s->parent);
  unlink_node(ctx, node.index);
  if (ctx->dispatch_depth)
    rd__chain_push(&ctx->pending_destroy, node);
  else
    destroy_rec(ctx, node.index);
}

bool rd_node_alive(const rd_ctx* ctx, rd_node node) { return rd__slot(ctx, node) != NULL; }

bool rd_node_append(rd_ctx* ctx, rd_node parent, rd_node child) {
  return rd_node_insert_before(ctx, parent, child, (rd_node){0, 0});
}

bool rd_node_insert_before(rd_ctx* ctx, rd_node parent, rd_node child, rd_node ref) {
  if (!rd__slot(ctx, parent) || !rd__slot(ctx, child) || child.index == ctx->root) return false;
  // The overlay stays the root's last child so it draws above, and is hit-tested before, everything else.
  if (rd_node_is_null(ref) && parent.index == ctx->root && rd__slot(ctx, ctx->overlay) && child.index != ctx->overlay.index)
    ref = ctx->overlay;
  if (!rd_node_is_null(ref)) {
    rd_slot* r = rd__slot(ctx, ref);
    if (!r || r->parent != parent.index) return false;
    if (ref.index == child.index) return true;
  }
  for (uint32_t a = parent.index; a; a = ctx->slots[a].parent)
    if (a == child.index) return false;
  uint32_t old_parent = ctx->slots[child.index].parent;
  if (old_parent) rd__mark_layout_dirty(ctx, old_parent);
  unlink_node(ctx, child.index);
  link_node(ctx, parent.index, child.index, ref.index);
  rd__mark_layout_dirty(ctx, parent.index);
  rd__mark_style_dirty(ctx, child.index);
  return true;
}

void rd_node_detach(rd_ctx* ctx, rd_node node) {
  rd_slot* s = rd__slot(ctx, node);
  if (!s || !s->parent) return;
  rd__mark_layout_dirty(ctx, s->parent);
  unlink_node(ctx, node.index);
  rd__mark_style_dirty(ctx, node.index);
}

#define RD_GETTER(field)                                  \
  rd_node rd_node_##field(const rd_ctx* ctx, rd_node n) { \
    rd_slot* s = rd__slot(ctx, n);                        \
    return s ? rd__handle(ctx, s->field) : (rd_node){0};  \
  }

RD_GETTER(parent)
RD_GETTER(first_child)
RD_GETTER(last_child)
RD_GETTER(next_sibling)
RD_GETTER(prev_sibling)

uint32_t rd_node_child_count(const rd_ctx* ctx, rd_node node) {
  rd_slot* s = rd__slot(ctx, node);
  return s ? s->child_count : 0;
}

bool rd_node_set_userdata(rd_ctx* ctx, rd_node node, void* data, rd_destroy_fn destroy) {
  rd_slot* s = rd__slot(ctx, node);
  if (!s) return false;
  void*         old    = s->userdata;
  rd_destroy_fn old_fn = s->destroy;
  s->userdata          = data;
  s->destroy           = destroy;
  if (old_fn && old != data) old_fn(old);
  return true;
}

void* rd_node_userdata(const rd_ctx* ctx, rd_node node) {
  rd_slot* s = rd__slot(ctx, node);
  return s ? s->userdata : NULL;
}

bool rd_ctx_set_extension(rd_ctx* ctx, const void* key, void* data, rd_destroy_fn destroy) {
  if (!ctx || !key) return false;
  for (uint32_t i = 0; i < ctx->extension_count; i++) {
    rd__extension* e = &ctx->extensions[i];
    if (e->key != key) continue;
    rd__extension old = *e;
    *e                = (rd__extension){key, data, destroy};
    if (old.destroy && old.data != data) old.destroy(old.data);
    return true;
  }
  rd__extension* p = realloc(ctx->extensions, (ctx->extension_count + 1) * sizeof *p);
  if (!p) return false;
  ctx->extensions                         = p;
  ctx->extensions[ctx->extension_count++] = (rd__extension){key, data, destroy};
  return true;
}

void* rd_ctx_extension(const rd_ctx* ctx, const void* key) {
  for (uint32_t i = 0; ctx && i < ctx->extension_count; i++)
    if (ctx->extensions[i].key == key) return ctx->extensions[i].data;
  return NULL;
}
