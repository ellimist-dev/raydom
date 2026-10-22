#include <stdlib.h>
#include <string.h>

#include "internal.h"

// Scratch memory for one layout or dispatch at a time, released in reverse order: a chain of chunks kept
// between uses, so steady-state frames allocate nothing. A mark is the chunk in use and how much of it.
struct rd__chunk {
  struct rd__chunk* next;
  size_t            cap, used;
  _Alignas(max_align_t) unsigned char data[];
};

enum { SCRATCH_MIN = 64 * 1024 };

rd__scratch_mark rd__scratch_save(const rd_ctx* ctx) {
  return (rd__scratch_mark){ctx->scratch_cur, ctx->scratch_cur ? ctx->scratch_cur->used : 0};
}

void rd__scratch_restore(rd_ctx* ctx, rd__scratch_mark m) {
  ctx->scratch_cur = m.chunk;
  if (m.chunk) m.chunk->used = m.used;
}

void* rd__scratch_alloc(rd_ctx* ctx, size_t n) {
  if (!n) return NULL;
  size_t     a = _Alignof(max_align_t);
  rd__chunk* c = ctx->scratch_cur;
  n            = (n + a - 1) / a * a;
  if (c && c->cap - c->used >= n) {
    void* p = c->data + c->used;
    c->used += n;
    return memset(p, 0, n);
  }
  // The next chunk, reused if it's big enough; otherwise a new one goes before it.
  rd__chunk* next = c ? c->next : ctx->scratch_first;
  if (!next || next->cap < n) {
    size_t     cap   = n > SCRATCH_MIN ? n : SCRATCH_MIN;
    rd__chunk* fresh = malloc(sizeof *fresh + cap);
    if (!fresh) return NULL;
    fresh->cap  = cap;
    fresh->next = next;
    if (c)
      c->next = fresh;
    else
      ctx->scratch_first = fresh;
    next = fresh;
  }
  next->used       = n;
  ctx->scratch_cur = next;
  return memset(next->data, 0, n);
}

void rd__scratch_free(rd_ctx* ctx) {
  for (rd__chunk* c = ctx->scratch_first; c;) {
    rd__chunk* next = c->next;
    free(c);
    c = next;
  }
  ctx->scratch_first = ctx->scratch_cur = NULL;
}
