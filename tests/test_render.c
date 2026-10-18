#include <math.h>
#include <raydom/raydom.h>

#include "check.h"
#include "render_internal.h"

static bool near(float a, float b) { return fabsf(a - b) < 0.01f; }

static rd_node add(rd_ctx* ctx, rd_node parent, rd_style style) {
  rd_node n = rd_node_create(ctx);
  rd_node_set_style(ctx, n, &style);
  rd_node_append(ctx, parent, n);
  return n;
}

static uint32_t count_kind(const rd__cmd* cmds, uint32_t n, uint8_t kind) {
  uint32_t c = 0;
  for (uint32_t j = 0; j < n; j++) c += cmds[j].kind == kind;
  return c;
}

static const rd__cmd* find(const rd__cmd* cmds, uint32_t n, uint8_t kind, rd_node node) {
  for (uint32_t j = 0; j < n; j++)
    if (cmds[j].kind == kind && cmds[j].node == node.index) return &cmds[j];
  return NULL;
}

static void test_display_list(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node box  = add(ctx, rd_ctx_root(ctx),
                     (rd_style){.width         = rd_px(100),
                                .height        = rd_px(50),
                                .margin        = rd_edges_all(rd_px(10)),
                                .background    = rd_hex(0x336699),
                                .border_width  = rd_edges_all(rd_px(2)),
                                .border_color  = rd_hex(0xffffff),
                                .border_radius = rd_corners_all(rd_px(80)),
                                .box_shadow    = rd_shadow_of(0, 4, 8, rd_rgba(0, 0, 0, 128)),
                                .opacity       = rd_num(0.5f)});
  rd_node bare = add(ctx, rd_ctx_root(ctx), (rd_style){.height = rd_px(10)});
  rd_node none = add(ctx, rd_ctx_root(ctx), (rd_style){.height = rd_px(10), .background = rd_hex(0xff0000), .display = RD_DISPLAY_NONE});
  rd_ctx_layout(ctx, 400, 300);

  uint32_t       n;
  const rd__cmd* cmds = rd__display_list(ctx, &n);
  CHECK(n == 2);
  const rd__cmd* sh = find(cmds, n, RD_CMD_SHADOW, box);
  const rd__cmd* r  = find(cmds, n, RD_CMD_RECT, box);
  CHECK(sh && r && sh < r);
  CHECK(r && near(r->rect.x, 10) && near(r->rect.y, 10) && near(r->rect.width, 100));
  CHECK(r && r->fill.a == 128 && r->stroke.a == 128 && r->border[0] == 2);
  CHECK(r && near(r->radii[0], 25) && near(r->radii[2], 25));
  CHECK(sh && near(sh->rect.y, 14) && sh->blur == 8 && sh->fill.a == 64);
  CHECK(!find(cmds, n, RD_CMD_RECT, bare) && !find(cmds, n, RD_CMD_RECT, none));
  rd_ctx_destroy(ctx);
}

static void test_clip_and_cull(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node clip = add(ctx, rd_ctx_root(ctx),
                     (rd_style){.width        = rd_px(100),
                                .height       = rd_px(100),
                                .border_width = rd_edges_all(rd_px(5)),
                                .overflow_y   = RD_OVERFLOW_SCROLL});
  rd_node rows[10];
  for (int j = 0; j < 10; j++)
    rows[j] = add(ctx, clip, (rd_style){.height = rd_px(40), .background = rd_hex(0x202020)});
  rd_ctx_layout(ctx, 400, 300);

  uint32_t       n;
  const rd__cmd* cmds = rd__display_list(ctx, &n);
  CHECK(count_kind(cmds, n, RD_CMD_CLIP) == 2);
  CHECK(count_kind(cmds, n, RD_CMD_RECT) == 3);
  const rd__cmd* c = &cmds[0];
  CHECK(c->kind == RD_CMD_CLIP && near(c->rect.x, 5) && near(c->rect.y, 5) && near(c->rect.width, 90) &&
        near(c->rect.height, 90));
  CHECK(cmds[n - 1].kind == RD_CMD_CLIP && near(cmds[n - 1].rect.width, 400));

  rd_size content = rd_node_content_size(ctx, clip);
  CHECK(near(content.height, 400));
  rd_node_set_scroll(ctx, clip, 0, 1000);
  CHECK(near(rd_node_scroll(ctx, clip).y, 310));
  cmds = rd__display_list(ctx, &n);
  CHECK(find(cmds, n, RD_CMD_RECT, rows[9]) && !find(cmds, n, RD_CMD_RECT, rows[0]));
  CHECK(near(find(cmds, n, RD_CMD_RECT, rows[9])->rect.y, 5 + 360 - 310));
  rd_ctx_destroy(ctx);
}

static void test_hit_and_pointer(void) {
  rd_ctx*  ctx   = rd_ctx_create();
  rd_class hover = rd_class_create(ctx, NULL);
  rd_class_set_state(ctx, hover, RD_STATE_HOVER, &(rd_style){.background = rd_hex(0x00ff00)});
  rd_node panel = add(ctx, rd_ctx_root(ctx),
                      (rd_style){.width = rd_px(200), .height = rd_px(100), .overflow_y = RD_OVERFLOW_SCROLL});
  rd_node a     = add(ctx, panel, (rd_style){.height = rd_px(80)});
  rd_node b     = add(ctx, panel, (rd_style){.height = rd_px(80)});
  rd_node over  = add(ctx, rd_ctx_root(ctx),
                      (rd_style){.position = RD_POSITION_ABSOLUTE,
                                 .inset    = {.left = rd_px(150), .top = rd_px(0)},
                                 .width    = rd_px(100),
                                 .height   = rd_px(20)});
  rd_node_add_class(ctx, a, hover);
  rd_node_add_class(ctx, panel, hover);
  rd_ctx_layout(ctx, 400, 300);

  CHECK(rd_node_eq(rd_node_at(ctx, 10, 10), a));
  CHECK(rd_node_eq(rd_node_at(ctx, 10, 90), b));
  CHECK(rd_node_eq(rd_node_at(ctx, 160, 10), over));
  CHECK(rd_node_eq(rd_node_at(ctx, 10, 150), rd_ctx_root(ctx)));
  CHECK(rd_node_is_null(rd_node_at(ctx, 500, 10)));

  rd_ctx_pointer(ctx, 10, 10, false, 0, 0);
  CHECK(rd_node_eq(rd_ctx_hovered(ctx), a));
  CHECK((rd_node_states(ctx, a) & RD_STATE_HOVER) && (rd_node_states(ctx, panel) & RD_STATE_HOVER));
  CHECK(rd_node_computed_style(ctx, a)->background.g == 0xff);

  rd_ctx_pointer(ctx, 10, 10, true, 0, 0);
  CHECK(rd_node_states(ctx, a) & RD_STATE_ACTIVE);
  rd_ctx_pointer(ctx, 10, 10, false, 0, -1);
  CHECK(!(rd_node_states(ctx, a) & RD_STATE_ACTIVE));
  CHECK(near(rd_node_scroll(ctx, panel).y, 48));
  rd_ctx_pointer(ctx, 10, 10, false, 0, -1);
  CHECK(near(rd_node_scroll(ctx, panel).y, 60));

  rd_ctx_layout(ctx, 400, 300);
  CHECK(rd_node_eq(rd_node_at(ctx, 10, 10), a));
  CHECK(rd_node_eq(rd_node_at(ctx, 10, 30), b));
  rd_node_destroy(ctx, b);
  rd_ctx_layout(ctx, 400, 300);
  rd_ctx_pointer(ctx, 300, 200, false, 0, 0);
  CHECK(!(rd_node_states(ctx, panel) & RD_STATE_HOVER));
  CHECK(rd_node_states(ctx, rd_ctx_root(ctx)) & RD_STATE_HOVER);
  rd_ctx_destroy(ctx);
}

static int  painted_destroyed;
static void paint_noop(rd_ctx* ctx, const rd_paint_info* info, void* data) {
  (void)ctx;
  (void)info;
  (void)data;
}
static void paint_destroy(void* data) {
  (void)data;
  painted_destroyed++;
}

static void test_paint_commands(void) {
  rd_ctx* ctx    = rd_ctx_create();
  rd_node scroll = add(ctx, rd_ctx_root(ctx),
                       (rd_style){.width = rd_px(200), .height = rd_px(100), .overflow_y = RD_OVERFLOW_SCROLL});
  rd_node canvas = add(ctx, scroll,
                       (rd_style){.height = rd_px(80), .padding = rd_edges_all(rd_px(10)), .background = rd_hex(0x101010)});
  rd_node label  = add(ctx, canvas, (rd_style){.height = rd_px(10), .background = rd_hex(0xffffff)});
  add(ctx, scroll, (rd_style){.height = rd_px(40)});
  rd_node below = add(ctx, scroll, (rd_style){.height = rd_px(80)});
  CHECK(rd_node_set_paint(ctx, canvas, paint_noop, NULL, paint_destroy));
  CHECK(rd_node_set_paint(ctx, below, paint_noop, NULL, paint_destroy));
  rd_ctx_layout(ctx, 400, 300);

  uint32_t       n;
  const rd__cmd* cmds = rd__display_list(ctx, &n);
  const rd__cmd *bg = find(cmds, n, RD_CMD_RECT, canvas), *paint = find(cmds, n, RD_CMD_PAINT, canvas);
  const rd__cmd* child = find(cmds, n, RD_CMD_RECT, label);
  CHECK(bg && paint && child && bg < paint && paint < child);
  CHECK(paint && near(paint->rect.x, 10) && near(paint->rect.y, 10) && near(paint->rect.width, 180) &&
        near(paint->rect.height, 60));
  CHECK(paint && near(paint->clip.height, 100));
  CHECK(find(cmds, n, RD_CMD_PAINT, below) == NULL);

  rd_node_set_scroll(ctx, scroll, 0, 60);
  cmds = rd__display_list(ctx, &n);
  CHECK(find(cmds, n, RD_CMD_PAINT, below) != NULL);

  rd_node_set_paint(ctx, below, NULL, NULL, NULL);
  CHECK(painted_destroyed == 1);
  rd_node_destroy(ctx, canvas);
  CHECK(painted_destroyed == 2);
  rd_ctx_destroy(ctx);
}

int main(void) {
  test_display_list();
  test_clip_and_cull();
  test_hit_and_pointer();
  test_paint_commands();
  return TEST_RESULT();
}
