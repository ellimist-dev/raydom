// Runs headless as a test for the timers section of "The frame loop": the clock is set by hand.
#include <assert.h>
#include <raydom/raydom.h>

typedef struct toast {
  rd_node  node;
  rd_timer hide;
} toast;

// ANCHOR: oneshot
static void hide_toast(rd_ctx* ctx, void* data) {
  toast* t = data;
  rd_node_merge_style(ctx, t->node, &(rd_style){.display = RD_DISPLAY_NONE});
}

static void show_toast(rd_ctx* ctx, toast* t) {
  rd_node_merge_style(ctx, t->node, &(rd_style){.display = RD_DISPLAY_FLEX});
  rd_ctx_clear_timer(ctx, t->hide);  // showing again restarts the countdown
  t->hide = rd_ctx_set_timer(ctx, 2.0, 0, hide_toast, t, NULL);
}
// ANCHOR_END: oneshot

static void tick(rd_ctx* ctx, void* data) {
  (void)ctx;
  ++*(int*)data;
}

int main(void) {
  rd_ctx* ctx = rd_ctx_create();
  toast   t   = {rd_node_create(ctx), {0}};
  rd_node_append(ctx, rd_ctx_root(ctx), t.node);

  rd_ctx_set_time(ctx, 0);
  show_toast(ctx, &t);
  rd_ctx_set_time(ctx, 1.5);
  show_toast(ctx, &t);  // restarts: now due at 3.5
  rd_ctx_set_time(ctx, 3);
  assert(rd_node_computed_style(ctx, t.node)->display == RD_DISPLAY_FLEX);
  rd_ctx_set_time(ctx, 3.5);
  assert(rd_node_computed_style(ctx, t.node)->display == RD_DISPLAY_NONE);

  // ANCHOR: repeat
  int      seconds = 0;
  rd_timer clock   = rd_ctx_set_timer(ctx, 1.0, RD_TIMER_REPEAT, tick, &seconds, NULL);
  // ... later
  rd_ctx_clear_timer(ctx, clock);
  // ANCHOR_END: repeat
  assert(seconds == 0);

  rd_ctx_destroy(ctx);
  return 0;
}
