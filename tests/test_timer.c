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

static bool clear_result;
static void clear_self_once(rd_ctx* ctx, void* data) {
  tag* t = data;
  note(ctx, data);
  clear_result = rd_ctx_clear_timer(ctx, t->log->self);
}

// Updates the clock from inside a callback, as a callback that calls rd_update would.
static void tick_inside(rd_ctx* ctx, void* data) {
  note(ctx, data);
  rd_ctx_set_time(ctx, rd_ctx_time(ctx) + 1);
}

int main(void) {
  rd_ctx* ctx = rd_ctx_create();
  journal lg  = {0};
  tag     a = {&lg, 'a'}, b = {&lg, 'b'}, r = {&lg, 'r'}, x = {&lg, 'x'};
  CHECK(rd_ctx_time(ctx) == 0 && rd_ctx_time(NULL) == 0);
  rd_ctx_set_time(ctx, 10);
  CHECK(rd_ctx_time(ctx) == 10);
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

  // A one-shot clearing itself from its callback: it has fired, so false, and it's destroyed once.
  lg      = (journal){0};
  lg.self = rd_ctx_set_timer(ctx, 0, 0, clear_self_once, &x, destroyed);
  rd_ctx_set_time(ctx, 60);
  CHECK(lg.len == 1 && !clear_result && lg.destroyed == 1);
  // A repeating one clearing itself: true, and it stops.
  lg      = (journal){0};
  lg.self = rd_ctx_set_timer(ctx, 1, RD_TIMER_REPEAT, clear_self_once, &x, destroyed);
  rd_ctx_set_time(ctx, 61);
  CHECK(lg.len == 1 && clear_result && lg.destroyed == 1);
  rd_ctx_set_time(ctx, 70);
  CHECK(lg.len == 1);

  // A callback updating the clock doesn't fire itself again; timers due by then fire after it returns.
  lg = (journal){0};
  rd_ctx_set_timer(ctx, 0, 0, tick_inside, &a, destroyed);
  rd_ctx_set_timer(ctx, 0.5, 0, note, &b, destroyed);
  rd_ctx_set_time(ctx, 70);
  CHECK(lg.len == 2 && memcmp(lg.order, "ab", 2) == 0 && lg.destroyed == 2 && rd_ctx_time(ctx) == 71);
  lg = (journal){0};
  rd_timer rt = rd_ctx_set_timer(ctx, 1, RD_TIMER_REPEAT, tick_inside, &r, destroyed);
  rd_ctx_set_time(ctx, 72);
  CHECK(lg.len == 1 && rd_ctx_time(ctx) == 73);
  CHECK(rd_ctx_clear_timer(ctx, rt) && lg.destroyed == 1);
  rd_ctx_set_time(ctx, 73);

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
