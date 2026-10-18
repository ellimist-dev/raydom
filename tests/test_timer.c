#include <math.h>
#include <raydom/raydom.h>
#include <string.h>

#include "check.h"

typedef struct journal {
  char  order[32];
  int   len;
  int   destroyed;
  rd_timer self;
  rd_ctx*  ctx;
} journal;

typedef struct tag {
  journal* log;
  char c;
} tag;

static void note(rd_ctx* ctx, void* data) {
  (void)ctx;
  tag* t = data;
  if (t->log->len < 31) t->log->order[t->log->len++] = t->c;
}

static void destroyed(void* data) { ((tag*)data)->log->destroyed++; }

static void clear_self(rd_ctx* ctx, void* data) {
  tag* t = data;
  note(ctx, data);
  rd_ctx_clear_timer(ctx, t->log->self);
  note(ctx, data);  // data is still valid until the callback returns
}

static void chain(rd_ctx* ctx, void* data) {
  note(ctx, data);
  static tag next;
  next = (tag){((tag*)data)->log, 'n'};
  rd_ctx_set_timer(ctx, 0, 0, note, &next, NULL);
}

int main(void) {
  rd_ctx* ctx = rd_ctx_create();
  journal lg  = {0};
  tag     a = {&lg, 'a'}, b = {&lg, 'b'}, r = {&lg, 'r'}, x = {&lg, 'x'};
  rd_ctx_set_time(ctx, 10);
  CHECK(isinf(rd_ctx_timer_wait(ctx)));

  // One-shots fire once, in due order, and are destroyed after firing.
  rd_ctx_set_timer(ctx, 2, 0, note, &b, destroyed);
  rd_ctx_set_timer(ctx, 1, 0, note, &a, destroyed);
  CHECK(fabs(rd_ctx_timer_wait(ctx) - 1) < 1e-9);
  rd_ctx_set_time(ctx, 10.5);
  CHECK(lg.len == 0);
  rd_ctx_set_time(ctx, 12);
  CHECK(lg.len == 2 && memcmp(lg.order, "ab", 2) == 0 && lg.destroyed == 2);
  rd_ctx_set_time(ctx, 20);
  CHECK(lg.len == 2);

  // Repeating: once per period, skipping missed calls after a stall.
  lg = (journal){0};
  rd_timer rep = rd_ctx_set_timer(ctx, 1, RD_TIMER_REPEAT, note, &r, destroyed);
  rd_ctx_set_time(ctx, 21);
  rd_ctx_set_time(ctx, 22);
  rd_ctx_set_time(ctx, 30);  // eight periods late: one call
  CHECK(lg.len == 3);
  CHECK(fabs(rd_ctx_timer_wait(ctx) - 1) < 1e-9);
  CHECK(rd_ctx_clear_timer(ctx, rep) && lg.destroyed == 1 && !rd_ctx_clear_timer(ctx, rep));
  rd_ctx_set_time(ctx, 40);
  CHECK(lg.len == 3);

  // Cleared before firing: never fires.
  lg = (journal){0};
  rd_timer gone = rd_ctx_set_timer(ctx, 1, 0, note, &x, destroyed);
  rd_ctx_clear_timer(ctx, gone);
  rd_ctx_set_time(ctx, 50);
  CHECK(lg.len == 0 && lg.destroyed == 1);

  // Cleared from its own callback: destroyed after the callback returns, and stops repeating.
  lg      = (journal){0};
  lg.self = rd_ctx_set_timer(ctx, 1, RD_TIMER_REPEAT, clear_self, &x, destroyed);
  rd_ctx_set_time(ctx, 51);
  CHECK(lg.len == 2 && lg.destroyed == 1);
  rd_ctx_set_time(ctx, 60);
  CHECK(lg.len == 2);

  // A timer set from a callback, even with no delay, waits for the next clock update.
  lg = (journal){0};
  tag c = {&lg, 'c'};
  rd_ctx_set_timer(ctx, 0, 0, chain, &c, NULL);
  rd_ctx_set_time(ctx, 60);
  CHECK(lg.len == 1 && lg.order[0] == 'c' && rd_ctx_timer_wait(ctx) == 0);
  rd_ctx_set_time(ctx, 60);
  CHECK(lg.len == 2 && lg.order[1] == 'n');

  // Handles: stale after reuse; null and bad input.
  CHECK(!rd_ctx_clear_timer(ctx, gone) && !rd_ctx_clear_timer(ctx, (rd_timer){0}));
  CHECK(rd_ctx_set_timer(ctx, 1, 0, NULL, NULL, NULL).index == 0);

  // Pending timers are destroyed with the context.
  lg = (journal){0};
  rd_ctx_set_timer(ctx, 100, 0, note, &a, destroyed);
  rd_ctx_destroy(ctx);
  CHECK(lg.destroyed == 1 && lg.len == 0);
  return TEST_RESULT();
}
