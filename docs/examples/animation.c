// Runs headless as a test for the "Animation" chapter: the clock is set by hand.
#include <assert.h>
#include <stdlib.h>
#include <raydom/raydom.h>

static void remove_node(rd_ctx* ctx, void* data) { rd_node_destroy(ctx, *(rd_node*)data); }

int main(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_ctx_set_time(ctx, 0);

  // ANCHOR: transition
  rd_class card = rd_class_create(ctx, &(rd_style){.width               = rd_px(200),
                                                   .background          = rd_hex(0x1a1f2b),
                                                   .transition_duration = rd_ms(200),
                                                   .transition_easing   = RD_EASING_EASE_OUT});
  rd_class_set_state(ctx, card, RD_STATE_HOVER,
                     &(rd_style){.width = rd_px(240), .background = rd_hex(0x252c3d)});  // both animate
  // ANCHOR_END: transition
  rd_node n = rd_node_create(ctx);
  rd_node_add_class(ctx, n, card);
  rd_node_append(ctx, rd_ctx_root(ctx), n);
  rd_ctx_layout(ctx, 800, 600);
  rd_node_set_states(ctx, n, RD_STATE_HOVER, true);
  rd_ctx_layout(ctx, 800, 600);
  rd_ctx_set_time(ctx, 0.1);
  rd_ctx_layout(ctx, 800, 600);
  float w = rd_node_rect(ctx, n).width;
  assert(w > 200 && w < 240);

  // ANCHOR: accordion
  rd_node panel = rd_node_create(ctx);
  rd_node_set_style(ctx, panel, &(rd_style){.height              = rd_px(0),  // closed
                                            .overflow_y          = RD_OVERFLOW_HIDDEN,
                                            .transition_duration = rd_ms(250)});
  // open: animates to the content's height, then stays auto as the content changes
  rd_node_merge_style(ctx, panel, &(rd_style){.height = rd_auto()});
  // ANCHOR_END: accordion
  rd_node_append(ctx, rd_ctx_root(ctx), panel);

  // ANCHOR: explicit
  static rd_node toast;
  toast = rd_node_create(ctx);
  rd_node_append(ctx, rd_ctx_root(ctx), toast);
  // In: from transparent and 24px lower, to its place.
  rd_node_animate(ctx, toast, &(rd_style){.opacity = rd_num(0), .margin.top = rd_px(24)},
                  &(rd_style){.opacity = rd_num(1), .margin.top = rd_px(0)}, 0.2, RD_EASING_EASE_OUT);
  // ... later, out: fade, then remove it when the animation ends, with a timer for the same time.
  rd_node_animate(ctx, toast, NULL, &(rd_style){.opacity = rd_num(0)}, 0.15, RD_EASING_EASE_IN);
  rd_node* doomed = malloc(sizeof *doomed);
  *doomed         = toast;
  rd_ctx_set_timer(ctx, 0.15, 0, remove_node, doomed, free);  // freed when it fires, or if cleared
  // ANCHOR_END: explicit
  rd_ctx_set_time(ctx, 1);
  rd_ctx_layout(ctx, 800, 600);
  assert(!rd_node_alive(ctx, toast) && !rd_ctx_animating(ctx));
  rd_ctx_destroy(ctx);
  return 0;
}
