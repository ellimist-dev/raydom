#include <math.h>
#include <raydom/raydom.h>

#include "check.h"

static bool near(float a, float b) { return fabsf(a - b) < 0.6f; }

static rd_node add(rd_ctx* ctx, rd_node parent, rd_style style) {
  rd_node n = rd_node_create(ctx);
  rd_node_set_style(ctx, n, &style);
  rd_node_append(ctx, parent, n);
  return n;
}

static void at(rd_ctx* ctx, double t) {
  rd_ctx_set_time(ctx, t);
  rd_ctx_layout(ctx, 800, 600);
}

static const rd_style* cs(rd_ctx* ctx, rd_node n) { return rd_node_computed_style(ctx, n); }

static void paint(void) {
  rd_ctx*  ctx  = rd_ctx_create();
  rd_class look = rd_class_create(ctx, &(rd_style){.height              = rd_px(10),
                                                   .background          = rd_rgb(0, 0, 0),
                                                   .transition_duration = rd_s(1),
                                                   .transition_easing   = RD_EASING_LINEAR});
  rd_class_set_state(ctx, look, RD_STATE_HOVER, &(rd_style){.background = rd_rgb(200, 100, 0)});
  rd_node n = add(ctx, rd_ctx_root(ctx), (rd_style){0});
  rd_node_add_class(ctx, n, look);
  at(ctx, 0);
  CHECK(!rd_ctx_animating(ctx) && cs(ctx, n)->background.r == 0);  // a first style doesn't animate

  rd_node_set_states(ctx, n, RD_STATE_HOVER, true);
  at(ctx, 0);
  CHECK(rd_ctx_animating(ctx) && cs(ctx, n)->background.r == 0);
  at(ctx, 0.5);
  CHECK(cs(ctx, n)->background.r == 100 && cs(ctx, n)->background.g == 50);
  at(ctx, 1);
  CHECK(cs(ctx, n)->background.r == 200 && !rd_ctx_animating(ctx));

  // Fading from transparent keeps the color (premultiplied), rather than passing through black.
  rd_class_set(ctx, look, &(rd_style){.height = rd_px(10), .background = rd_rgba(200, 100, 0, 0),
                                      .transition_duration = rd_s(1), .transition_easing = RD_EASING_LINEAR});
  rd_node_set_states(ctx, n, RD_STATE_HOVER, false);
  at(ctx, 1);
  at(ctx, 2);
  rd_node_set_states(ctx, n, RD_STATE_HOVER, true);
  at(ctx, 2);
  at(ctx, 2.5);
  rd_color c = cs(ctx, n)->background;
  CHECK(c.r == 200 && c.g == 100 && (c.a == 127 || c.a == 128));
  rd_ctx_destroy(ctx);
}

static void layout(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node row = add(ctx, rd_ctx_root(ctx), (rd_style){.flex_direction = RD_FLEX_ROW});
  rd_node box = add(ctx, row, (rd_style){.width = rd_px(100), .height = rd_px(20), .transition_duration = rd_s(1),
                                         .transition_easing = RD_EASING_LINEAR});
  rd_node next = add(ctx, row, (rd_style){.width = rd_px(10)});
  at(ctx, 0);

  rd_node_merge_style(ctx, box, &(rd_style){.width = rd_px(200)});
  at(ctx, 0);
  at(ctx, 0.5);
  CHECK(near(rd_node_rect(ctx, box).width, 150) && near(rd_node_rect(ctx, next).x, 150));  // siblings follow

  // Retargeting mid-way starts from where it is.
  rd_node_merge_style(ctx, box, &(rd_style){.width = rd_px(100)});
  at(ctx, 0.5);
  at(ctx, 1.0);
  CHECK(near(rd_node_rect(ctx, box).width, 125));
  at(ctx, 1.5);
  CHECK(near(rd_node_rect(ctx, box).width, 100) && !rd_ctx_animating(ctx));

  // Delay, and easing.
  rd_node_merge_style(ctx, box, &(rd_style){.width = rd_px(300), .transition_delay = rd_s(0.5f),
                                            .transition_easing = RD_EASING_EASE_IN});
  at(ctx, 2);
  at(ctx, 2.25);
  CHECK(near(rd_node_rect(ctx, box).width, 100));
  at(ctx, 3.0);  // halfway through: ease-in is behind linear
  CHECK(rd_node_rect(ctx, box).width > 110 && rd_node_rect(ctx, box).width < 200);
  at(ctx, 3.5);
  CHECK(near(rd_node_rect(ctx, box).width, 300));

  // The property mask: color animates, size jumps. Different units jump.
  rd_node_merge_style(ctx, box, &(rd_style){.transition_delay = rd_s(0), .transition_properties = RD_TRANSITION_COLOR,
                                            .width = rd_px(50), .background = rd_rgb(100, 0, 0)});
  at(ctx, 4);
  CHECK(near(rd_node_rect(ctx, box).width, 50));
  rd_node_merge_style(ctx, box, &(rd_style){.transition_properties = RD_TRANSITION_ALL, .width = rd_pct(50)});
  at(ctx, 5);
  CHECK(near(rd_node_rect(ctx, box).width, 400));

  // display: none doesn't animate.
  rd_node_merge_style(ctx, box, &(rd_style){.display = RD_DISPLAY_NONE, .width = rd_px(10)});
  at(ctx, 6);
  CHECK(!rd_ctx_animating(ctx));
  rd_ctx_destroy(ctx);
}

static void auto_height(void) {
  rd_ctx* ctx   = rd_ctx_create();
  rd_node panel = add(ctx, rd_ctx_root(ctx), (rd_style){.overflow_y = RD_OVERFLOW_HIDDEN, .padding = rd_edges_all(rd_px(5)),
                                                        .transition_duration = rd_s(1),
                                                        .transition_easing   = RD_EASING_LINEAR});
  add(ctx, panel, (rd_style){.height = rd_px(80)});
  at(ctx, 0);
  CHECK(near(rd_node_rect(ctx, panel).height, 90));

  // Collapsing from auto starts at the laid-out height.
  rd_node_merge_style(ctx, panel, &(rd_style){.height = rd_px(10)});
  at(ctx, 0);
  at(ctx, 0.5);
  CHECK(near(rd_node_rect(ctx, panel).height, 50));
  at(ctx, 1);
  CHECK(near(rd_node_rect(ctx, panel).height, 10));

  // Expanding to auto ends at the measured height, and ends as auto.
  rd_node_merge_style(ctx, panel, &(rd_style){.height = rd_auto()});
  at(ctx, 1);
  at(ctx, 1.5);
  CHECK(near(rd_node_rect(ctx, panel).height, 50));
  at(ctx, 2);
  CHECK(near(rd_node_rect(ctx, panel).height, 90) && cs(ctx, panel)->height.unit == RD_UNIT_AUTO);
  add(ctx, panel, (rd_style){.height = rd_px(20)});  // auto keeps following the content
  at(ctx, 2.1);
  CHECK(near(rd_node_rect(ctx, panel).height, 110));
  rd_ctx_destroy(ctx);
}

static int done_calls = 0;
static void done(rd_ctx* ctx, void* data) {
  (void)ctx;
  done_calls++;
  rd_node_destroy(ctx, *(rd_node*)data);
}

static void explicit_and_inherited(void) {
  rd_ctx* ctx = rd_ctx_create();
  at(ctx, 0);
  // A new node fading and sliding in, then out and removed.
  static rd_node toast;
  toast = add(ctx, rd_ctx_root(ctx), (rd_style){.height = rd_px(10)});
  CHECK(rd_node_animate(ctx, toast, &(rd_style){.opacity = rd_num(0), .margin.top = rd_px(40)},
                        &(rd_style){.opacity = rd_num(1), .margin.top = rd_px(0)}, 1, RD_EASING_LINEAR, NULL, NULL));
  at(ctx, 0);
  CHECK(near(cs(ctx, toast)->opacity.value * 100, 0) && near(rd_node_rect(ctx, toast).y, 40));
  at(ctx, 0.5);
  CHECK(near(cs(ctx, toast)->opacity.value * 100, 50) && near(rd_node_rect(ctx, toast).y, 20));
  at(ctx, 1);
  CHECK(near(cs(ctx, toast)->opacity.value * 100, 100) && near(rd_node_rect(ctx, toast).y, 0));
  rd_node_animate(ctx, toast, NULL, &(rd_style){.opacity = rd_num(0)}, 0.5, RD_EASING_LINEAR, done, &toast);
  at(ctx, 1.25);
  CHECK(near(cs(ctx, toast)->opacity.value * 100, 50) && done_calls == 0);
  at(ctx, 1.5);
  CHECK(done_calls == 1 && !rd_node_alive(ctx, toast) && !rd_ctx_animating(ctx));

  // An inherited property animating on a parent shows in its children.
  rd_node parent = add(ctx, rd_ctx_root(ctx), (rd_style){.color = rd_rgb(0, 0, 0), .font_size = rd_px(10),
                                                         .transition_duration = rd_s(1), .transition_easing = RD_EASING_LINEAR});
  rd_node child = add(ctx, parent, (rd_style){0});
  at(ctx, 2);
  rd_node_merge_style(ctx, parent, &(rd_style){.color = rd_rgb(200, 0, 0), .font_size = rd_px(20)});
  at(ctx, 2);
  at(ctx, 2.5);
  CHECK(cs(ctx, child)->color.r == 100 && near(cs(ctx, child)->font_size.value, 15));

  // Destroyed mid-animation.
  rd_node_destroy(ctx, parent);
  at(ctx, 2.6);
  CHECK(!rd_ctx_animating(ctx));
  CHECK(!rd_node_animate(ctx, parent, NULL, &(rd_style){.opacity = rd_num(1)}, 1, 0, NULL, NULL));
  rd_ctx_destroy(ctx);
}

int main(void) {
  paint();
  layout();
  auto_height();
  explicit_and_inherited();
  return TEST_RESULT();
}
