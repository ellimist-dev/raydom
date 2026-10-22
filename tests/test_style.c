#include <raydom/raydom.h>

#include "check.h"
#include "internal.h"

static bool color_eq(rd_color a, rd_color b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static void clear_layout_dirty(rd_ctx* ctx) {
  for (uint32_t i = 1; i < ctx->len; i++) ctx->slots[i].flags &= ~RD_FLAG_LAYOUT_DIRTY;
}

static bool layout_dirty(rd_ctx* ctx, rd_node n) { return ctx->slots[n.index].flags & RD_FLAG_LAYOUT_DIRTY; }

static void test_defaults(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node a   = rd_node_create(ctx);
  rd_node_append(ctx, rd_ctx_root(ctx), a);
  const rd_style* s = rd_node_computed_style(ctx, a);
  CHECK(s->display == RD_DISPLAY_FLEX);
  CHECK(s->flex_direction == RD_FLEX_COLUMN);
  CHECK(s->align_items == RD_ALIGN_STRETCH);
  CHECK(s->width.unit == RD_UNIT_AUTO);
  CHECK(s->flex_shrink.unit == RD_UNIT_NUM && s->flex_shrink.value == 0);
  CHECK(s->padding.left.unit == RD_UNIT_PX && s->padding.left.value == 0);
  CHECK(color_eq(s->color, rd_rgb(0, 0, 0)));
  CHECK(s->font_size.value == 16);
  rd_ctx_destroy(ctx);
}

static void test_inheritance(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node root = rd_ctx_root(ctx);
  rd_node p = rd_node_create(ctx), c = rd_node_create(ctx);
  rd_node_append(ctx, root, p);
  rd_node_append(ctx, p, c);
  rd_node_set_style(ctx, p, &(rd_style){
                                .color      = rd_hex(0xff0000),
                                .font_size  = rd_px(20),
                                .background = rd_hex(0x00ff00),
                                .padding    = rd_edges_all(rd_px(4)),
                            });
  const rd_style* cs = rd_node_computed_style(ctx, c);
  CHECK(color_eq(cs->color, rd_hex(0xff0000)));
  CHECK(cs->font_size.value == 20);
  CHECK(cs->background.a == 0);
  CHECK(cs->padding.top.value == 0);

  rd_node_merge_style(ctx, p, &(rd_style){.color = rd_hex(0x0000ff)});
  cs = rd_node_computed_style(ctx, c);
  CHECK(color_eq(cs->color, rd_hex(0x0000ff)));
  CHECK(cs->font_size.value == 20);

  rd_node_detach(ctx, c);
  cs = rd_node_computed_style(ctx, c);
  CHECK(color_eq(cs->color, rd_rgb(0, 0, 0)));
  CHECK(cs->font_size.value == 16);
  rd_ctx_destroy(ctx);
}

static void test_class_order_and_inline(void) {
  rd_ctx*  ctx  = rd_ctx_create();
  rd_node  n    = rd_node_create(ctx);
  rd_class card = rd_class_create(ctx, &(rd_style){.padding = rd_edges_all(rd_px(8)), .background = rd_hex(0x111111)});
  rd_class wide = rd_class_create(ctx, &(rd_style){.padding = rd_edges_vh(rd_px(8), rd_px(24))});
  rd_node_append(ctx, rd_ctx_root(ctx), n);
  rd_node_add_class(ctx, n, card);
  rd_node_add_class(ctx, n, wide);
  CHECK(rd_node_has_class(ctx, n, wide));

  const rd_style* s = rd_node_computed_style(ctx, n);
  CHECK(s->padding.left.value == 24 && s->padding.top.value == 8);
  CHECK(color_eq(s->background, rd_hex(0x111111)));

  rd_node_merge_style(ctx, n, &(rd_style){.padding.left = rd_px(2)});
  s = rd_node_computed_style(ctx, n);
  CHECK(s->padding.left.value == 2 && s->padding.right.value == 24);

  rd_class_set(ctx, card, &(rd_style){.background = rd_hex(0x222222)});
  s = rd_node_computed_style(ctx, n);
  CHECK(color_eq(s->background, rd_hex(0x222222)));

  rd_node_remove_class(ctx, n, wide);
  rd_node_set_style(ctx, n, NULL);
  s = rd_node_computed_style(ctx, n);
  CHECK(s->padding.left.value == 0);
  CHECK(!rd_node_has_class(ctx, n, wide));
  rd_ctx_destroy(ctx);
}

static void test_states(void) {
  rd_ctx*  ctx     = rd_ctx_create();
  rd_node  n       = rd_node_create(ctx);
  rd_class button  = rd_class_create(ctx, &(rd_style){.background = rd_hex(0x333333)});
  rd_class primary = rd_class_create(ctx, &(rd_style){.background = rd_hex(0x0055ff)});
  rd_class_set_state(ctx, button, RD_STATE_HOVER, &(rd_style){.background = rd_hex(0x444444)});
  rd_class_set_state(ctx, button, RD_STATE_DISABLED, &(rd_style){.opacity = rd_num(0.5f)});
  CHECK(!rd_class_set_state(ctx, button, RD_STATE_HOVER | RD_STATE_FOCUS, NULL));  // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
  rd_node_append(ctx, rd_ctx_root(ctx), n);
  rd_node_add_class(ctx, n, button);
  rd_node_add_class(ctx, n, primary);

  CHECK(color_eq(rd_node_computed_style(ctx, n)->background, rd_hex(0x0055ff)));
  rd_node_set_states(ctx, n, RD_STATE_HOVER, true);
  CHECK(color_eq(rd_node_computed_style(ctx, n)->background, rd_hex(0x444444)));
  rd_node_set_states(ctx, n, RD_STATE_DISABLED, true);
  CHECK(rd_node_computed_style(ctx, n)->opacity.value == 0.5f);
  rd_node_set_states(ctx, n, RD_STATE_HOVER | RD_STATE_DISABLED, false);
  CHECK(rd_node_states(ctx, n) == 0);
  CHECK(color_eq(rd_node_computed_style(ctx, n)->background, rd_hex(0x0055ff)));
  CHECK(rd_node_computed_style(ctx, n)->opacity.value == 1);
  rd_ctx_destroy(ctx);
}

static void test_dirty_tracking(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node root = rd_ctx_root(ctx);
  rd_node a = rd_node_create(ctx), b = rd_node_create(ctx), sib = rd_node_create(ctx);
  rd_node_append(ctx, root, a);
  rd_node_append(ctx, a, b);
  rd_node_append(ctx, root, sib);
  rd_ctx_compute_styles(ctx);
  clear_layout_dirty(ctx);
  ctx->paint_dirty = false;

  rd_node_merge_style(ctx, b, &(rd_style){.background = rd_hex(0xabcdef)});
  rd_ctx_compute_styles(ctx);
  CHECK(ctx->paint_dirty);
  CHECK(!layout_dirty(ctx, b) && !layout_dirty(ctx, a) && !layout_dirty(ctx, root));

  rd_node_merge_style(ctx, b, &(rd_style){.width = rd_px(100)});
  rd_ctx_compute_styles(ctx);
  CHECK(layout_dirty(ctx, b) && layout_dirty(ctx, a) && layout_dirty(ctx, root));
  CHECK(!layout_dirty(ctx, sib));

  clear_layout_dirty(ctx);
  ctx->paint_dirty = false;
  rd_node_merge_style(ctx, b, &(rd_style){.width = rd_px(100)});
  rd_ctx_compute_styles(ctx);
  CHECK(!ctx->paint_dirty && !layout_dirty(ctx, b));

  rd_node_merge_style(ctx, a, &(rd_style){.font_size = rd_px(30)});
  rd_ctx_compute_styles(ctx);
  CHECK(layout_dirty(ctx, b));
  CHECK(rd_node_computed_style(ctx, b)->font_size.value == 30);
  rd_ctx_destroy(ctx);
}

static void test_shared_states(void) {
  rd_ctx*  ctx  = rd_ctx_create();
  rd_node  row  = rd_node_create(ctx);  // no classes: its own style never reacts to states
  rd_node  box  = rd_node_create(ctx);
  rd_node  mark = rd_node_create(ctx);
  rd_class look = rd_class_create(ctx, &(rd_style){.opacity = rd_num(0.5f)});
  rd_class_set_state(ctx, look, RD_STATE_HOVER, &(rd_style){.opacity = rd_num(0.8f)});
  rd_class_set_state(ctx, look, RD_STATE_CHECKED, &(rd_style){.opacity = rd_num(1)});
  rd_node_append(ctx, rd_ctx_root(ctx), row);
  rd_node_append(ctx, row, box);
  rd_node_append(ctx, box, mark);
  rd_node_add_class(ctx, box, look);
  rd_node_add_class(ctx, mark, look);
  rd_node_set_share_states(ctx, box, true);
  rd_node_set_share_states(ctx, mark, true);

  rd_node_set_states(ctx, row, RD_STATE_HOVER, true);
  CHECK(rd_node_computed_style(ctx, box)->opacity.value == 0.8f);
  CHECK(rd_node_computed_style(ctx, mark)->opacity.value == 0.8f);  // through the box
  rd_node_set_states(ctx, row, RD_STATE_CHECKED, true);
  CHECK(rd_node_computed_style(ctx, mark)->opacity.value == 1);
  CHECK(rd_node_states(ctx, box) == 0);  // own states unchanged
  rd_node_set_states(ctx, row, RD_STATE_HOVER | RD_STATE_CHECKED, false);
  CHECK(rd_node_computed_style(ctx, mark)->opacity.value == 0.5f);

  rd_node_set_share_states(ctx, box, false);  // the chain breaks at the box
  rd_node_set_states(ctx, row, RD_STATE_CHECKED, true);
  CHECK(rd_node_computed_style(ctx, box)->opacity.value == 0.5f && rd_node_computed_style(ctx, mark)->opacity.value == 0.5f);
  rd_ctx_destroy(ctx);
}

static void test_class_merge(void) {
  rd_ctx*  ctx = rd_ctx_create();
  rd_class c   = rd_class_create(ctx, &(rd_style){.width = rd_px(40), .background = rd_hex(0x111111)});
  rd_node  n   = rd_node_create(ctx);
  rd_node_append(ctx, rd_ctx_root(ctx), n);
  rd_node_add_class(ctx, n, c);
  rd_ctx_compute_styles(ctx);
  CHECK(rd_class_merge(ctx, c, &(rd_style){.background = rd_hex(0x222222)}));
  rd_ctx_compute_styles(ctx);
  const rd_style* st = rd_node_computed_style(ctx, n);
  CHECK(st->width.value == 40 && st->background.r == 0x22);  // the width stays

  CHECK(rd_class_merge_state(ctx, c, RD_STATE_HOVER, &(rd_style){.background = rd_hex(0x333333)}));
  rd_node_set_states(ctx, n, RD_STATE_HOVER, true);
  rd_ctx_compute_styles(ctx);
  CHECK(rd_node_computed_style(ctx, n)->background.r == 0x33);

  rd_style got;
  CHECK(rd_class_get(ctx, c, &got) && got.width.value == 40 && got.background.r == 0x22);
  CHECK(rd_class_get_state(ctx, c, RD_STATE_HOVER, &got) && got.background.r == 0x33 && got.width.unit == RD_UNIT_UNSET);
  CHECK(!rd_class_get_state(ctx, c, RD_STATE_ACTIVE, &got) && !got.background.set);  // no variant
  CHECK(!rd_class_get(ctx, (rd_class){99}, &got) && !rd_class_merge(ctx, (rd_class){99}, &got));
  rd_ctx_destroy(ctx);
}

static void test_style_merge(void) {
  rd_style dst = {.width = rd_px(10), .background = rd_hex(0x111111), .padding = rd_edges_all(rd_px(4))};
  rd_style_merge(&dst, &(rd_style){.background = rd_hex(0x222222), .padding = {.left = rd_px(9)}});
  CHECK(dst.width.value == 10 && dst.background.r == 0x22);           // unset in src leaves dst alone
  CHECK(dst.padding.left.value == 9 && dst.padding.top.value == 4);  // per side
  rd_style_merge(&dst, &(rd_style){0});
  CHECK(dst.width.value == 10 && dst.background.set);
}

int main(void) {
  test_style_merge();
  test_class_merge();
  test_defaults();
  test_shared_states();
  test_inheritance();
  test_class_order_and_inline();
  test_states();
  test_dirty_tracking();
  return TEST_RESULT();
}
