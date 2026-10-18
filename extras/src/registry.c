#include <stdlib.h>

#include "extras_internal.h"

typedef struct registry {
  rd_node* nodes;
  void**   objs;
  uint32_t len, cap;
} registry;

static void registry_free(void* p) {
  registry* r = p;
  free(r->nodes);
  free((void*)r->objs);
  free(r);
}

static void prune(const rd_ctx* ctx, registry* r) {
  uint32_t kept = 0;
  for (uint32_t j = 0; j < r->len; j++) {
    if (!rd_node_alive(ctx, r->nodes[j])) continue;
    r->nodes[kept]  = r->nodes[j];
    r->objs[kept++] = r->objs[j];
  }
  r->len = kept;
}

bool rdx__track(rd_ctx* ctx, const void* key, rd_node node, void* obj) {
  registry* r = rd_ctx_extension(ctx, key);
  if (!r) {
    r = calloc(1, sizeof *r);
    if (!r || !rd_ctx_set_extension(ctx, key, r, registry_free)) {
      free(r);
      return false;
    }
  }
  prune(ctx, r);
  if (r->len == r->cap) {
    uint32_t cap = r->cap ? r->cap * 2 : 8;
    rd_node* n   = realloc(r->nodes, cap * sizeof *n);
    if (!n) return false;
    r->nodes = n;
    void** o = (void**)realloc((void*)r->objs, cap * sizeof *o);
    if (!o) return false;
    r->objs = o;
    r->cap  = cap;
  }
  r->nodes[r->len]  = node;
  r->objs[r->len++] = obj;
  return true;
}

void* rdx__lookup(const rd_ctx* ctx, const void* key, rd_node node) {
  const registry* r = rd_ctx_extension(ctx, key);
  if (!r || !rd_node_alive(ctx, node)) return NULL;
  for (uint32_t j = 0; j < r->len; j++)
    if (rd_node_eq(r->nodes[j], node)) return r->objs[j];
  return NULL;
}

int rdx__all(rd_ctx* ctx, const void* key, void*** out) {
  registry* r = rd_ctx_extension(ctx, key);
  if (!r) return 0;
  prune(ctx, r);
  *out = r->objs;
  return (int)r->len;
}
