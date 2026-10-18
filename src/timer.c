#include <math.h>
#include <stdlib.h>

#include "internal.h"

static rd__timer* get(const rd_ctx* ctx, rd_timer t) {
  if (!ctx || t.index == 0 || t.index > ctx->timer_count) return NULL;
  rd__timer* tm = &ctx->timers[t.index - 1];
  return tm->active && tm->gen == t.gen ? tm : NULL;
}

rd_timer rd_ctx_set_timer(rd_ctx* ctx, double delay, uint32_t flags, rd_timer_fn fn, void* data, rd_destroy_fn destroy) {
  if (!ctx || !fn) return (rd_timer){0};
  if (!(delay >= 0)) delay = 0;  // also NaN
  bool repeat = flags & RD_TIMER_REPEAT;
  if (repeat && delay < 0.001) delay = 0.001;
  uint32_t i = 0;
  while (i < ctx->timer_count && ctx->timers[i].active) i++;
  if (i == ctx->timer_count) {
    if (ctx->timer_count == ctx->timer_cap) {
      uint32_t   cap = ctx->timer_cap ? ctx->timer_cap * 2 : 8;
      rd__timer* p   = realloc(ctx->timers, cap * sizeof *p);
      if (!p) return (rd_timer){0};
      ctx->timers    = p;
      ctx->timer_cap = cap;
    }
    ctx->timers[ctx->timer_count++] = (rd__timer){0};
  }
  rd__timer* tm = &ctx->timers[i];
  *tm           = (rd__timer){.due     = ctx->now + delay,
                              .period  = repeat ? delay : 0,
                              .fn      = fn,
                              .data    = data,
                              .destroy = destroy,
                              .gen     = tm->gen + 1,
                              .epoch   = ctx->timer_epoch,
                              .active  = true};
  return (rd_timer){i + 1, tm->gen};
}

static void finish(rd__timer* tm) {
  rd_destroy_fn destroy = tm->destroy;
  void*         data    = tm->data;
  tm->active            = false;
  tm->destroy           = NULL;
  tm->data              = NULL;
  if (destroy) destroy(data);
}

bool rd_ctx_clear_timer(rd_ctx* ctx, rd_timer t) {
  rd__timer* tm = get(ctx, t);
  if (!tm) return false;
  if (tm->firing) {
    tm->cleared = true;  // destroyed once its callback returns
    return true;
  }
  finish(tm);
  return true;
}

double rd_ctx_timer_wait(const rd_ctx* ctx) {
  double next = INFINITY;
  if (!ctx) return next;
  for (uint32_t i = 0; i < ctx->timer_count; i++)
    if (ctx->timers[i].active && !ctx->timers[i].cleared) next = fmin(next, ctx->timers[i].due);
  return isinf(next) ? next : fmax(0, next - ctx->now);
}

void rd__timers_run(rd_ctx* ctx) {
  uint32_t epoch = ++ctx->timer_epoch;
  for (;;) {
    // The earliest due timer that existed before this run.
    uint32_t best = UINT32_MAX;
    for (uint32_t i = 0; i < ctx->timer_count; i++) {
      const rd__timer* tm = &ctx->timers[i];
      if (tm->active && !tm->cleared && tm->epoch < epoch && tm->due <= ctx->now && tm->fired != epoch &&
          (best == UINT32_MAX || tm->due < ctx->timers[best].due))
        best = i;
    }
    if (best == UINT32_MAX) return;
    rd__timer* tm = &ctx->timers[best];
    tm->firing    = true;
    tm->fired     = epoch;  // a repeating timer fires at most once per run
    tm->fn(ctx, tm->data);
    tm = &ctx->timers[best];  // the array may have grown
    tm->firing = false;
    if (tm->cleared || tm->period == 0) {
      tm->cleared = false;
      finish(tm);
    } else {
      tm->due += tm->period;
      if (tm->due <= ctx->now) tm->due = ctx->now + tm->period;  // fell behind: skip the missed calls
    }
  }
}

void rd__timers_free(rd_ctx* ctx) {
  for (uint32_t i = 0; i < ctx->timer_count; i++)
    if (ctx->timers[i].active) finish(&ctx->timers[i]);
  free(ctx->timers);
  ctx->timers = NULL;
}
