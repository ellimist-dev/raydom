#include <math.h>
#include <raydom/raydom.h>
#include <string.h>

#include "check.h"
#include "render_internal.h"

static bool near(float a, float b) { return fabsf(a - b) < 0.01f; }

#define CHECK_SIZE(ctx, n, W, H) check_size(ctx, n, W, H, __LINE__)

static void check_size(rd_ctx* ctx, rd_node n, float w, float h, int line) {
  rd_rect r = rd_node_rect(ctx, n);
  if (near(r.width, w) && near(r.height, h)) return;
  fprintf(stderr, "%s:%d: size {%g, %g}, expected {%g, %g}\n", __FILE__, line, r.width, r.height, w, h);
  rd_test_failures++;
}

static rd_node add(rd_ctx* ctx, rd_node parent, rd_style style) {
  rd_node n = rd_node_create(ctx);
  rd_node_set_style(ctx, n, &style);
  rd_node_append(ctx, parent, n);
  return n;
}

static rd_node add_text(rd_ctx* ctx, rd_node parent, rd_style style, const char* text) {
  rd_node n = add(ctx, parent, style);
  rd_node_set_text(ctx, n, text, RD_TEXT_NUL);
  return n;
}

static const float LH = 16 * 1.2f;
static float       cw;

static rd_ctx* setup(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_font f   = rd_font_load_file(ctx, RAYDOM_TEST_FONT);
  CHECK(f.id != 0);
  cw = rd_font_measure(ctx, f, 16, "a", 1);
  return ctx;
}

static void test_font(void) {
  rd_ctx*         ctx = setup();
  rd_font_metrics m   = rd_font_get_metrics(ctx, (rd_font){0, true}, 16);
  CHECK(m.ascent > 0 && m.descent > 0 && m.ascent < 32);
  CHECK(cw > 0);
  CHECK(near(rd_font_measure(ctx, (rd_font){0, true}, 16, "abc", RD_TEXT_NUL), 3 * cw));
  CHECK(near(rd_font_measure(ctx, (rd_font){0, true}, 32, "a", 1), 2 * cw));
  CHECK(rd_font_load(ctx, "not a font", 10, true).id == 0);
  rd_ctx_destroy(ctx);
}

static void test_max_content_and_wrap(void) {
  rd_ctx* ctx = setup();
  rd_node row = add(ctx, rd_ctx_root(ctx), (rd_style){.flex_direction = RD_FLEX_ROW});
  rd_node a   = add_text(ctx, row, (rd_style){0}, "aaa bbb ccc");
  rd_node col = add(ctx, rd_ctx_root(ctx), (rd_style){.width = rd_px(7.5f * cw)});
  rd_node b   = add_text(ctx, col, (rd_style){0}, "aaa bbb ccc");
  rd_node fit = add(ctx, rd_ctx_root(ctx), (rd_style){.width = rd_px(7.5f * cw), .align_items = RD_ALIGN_START});
  rd_node c   = add_text(ctx, fit, (rd_style){0}, "aaa bbb ccc");
  rd_ctx_layout(ctx, 800, 600);
  CHECK_SIZE(ctx, a, 11 * cw, LH);
  CHECK_SIZE(ctx, b, 7.5f * cw, 2 * LH);
  CHECK_SIZE(ctx, c, 7 * cw, 2 * LH);
  rd_ctx_destroy(ctx);
}

static void test_min_content_in_grid(void) {
  rd_ctx*  ctx  = setup();
  rd_track t[2] = {rd_track_of(rd_fr(1)), rd_track_of(rd_fr(1))};
  rd_node  g    = add(ctx, rd_ctx_root(ctx),
                      (rd_style){.display               = RD_DISPLAY_GRID,
                                 .width                 = rd_px(4 * cw),
                                 .grid_template_columns = rd_tracks_create(ctx, t, 2)});
  rd_node  a    = add_text(ctx, g, (rd_style){0}, "aaa bbb ccc");
  rd_node  b    = add(ctx, g, (rd_style){0});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_SIZE(ctx, a, 3 * cw, 3 * LH);
  CHECK(near(rd_node_rect(ctx, b).x, 3 * cw) && near(rd_node_rect(ctx, b).width, cw));
  rd_ctx_destroy(ctx);
}

static void test_white_space(void) {
  rd_ctx* ctx = setup();
  rd_node row = add(ctx, rd_ctx_root(ctx), (rd_style){.flex_direction = RD_FLEX_ROW, .align_items = RD_ALIGN_START});
  rd_node a   = add_text(ctx, row, (rd_style){0}, "  aaa   bbb  ");
  rd_node b   = add_text(ctx, row, (rd_style){0}, "aaa\nbbb");
  rd_node c   = add_text(ctx, row, (rd_style){.white_space = RD_WHITE_SPACE_PRE}, "aa\nbbbb");
  rd_node d   = add_text(ctx, row, (rd_style){.white_space = RD_WHITE_SPACE_PRE}, "a  b");
  rd_node e   = add_text(ctx, row, (rd_style){.white_space = RD_WHITE_SPACE_PRE}, "a\n");
  rd_node f   = add_text(ctx, row, (rd_style){.white_space = RD_WHITE_SPACE_PRE}, "a\tb");
  rd_node col = add(ctx, rd_ctx_root(ctx), (rd_style){.width = rd_px(2 * cw), .align_items = RD_ALIGN_START});
  rd_node g   = add_text(ctx, col, (rd_style){.white_space = RD_WHITE_SPACE_NOWRAP}, "aaa bbb ccc");
  rd_node h   = add_text(ctx, col, (rd_style){.white_space = RD_WHITE_SPACE_PRE_WRAP}, "a  b");
  rd_ctx_layout(ctx, 800, 600);
  CHECK_SIZE(ctx, a, 7 * cw, LH);
  CHECK_SIZE(ctx, b, 7 * cw, LH);
  CHECK_SIZE(ctx, c, 4 * cw, 2 * LH);
  CHECK_SIZE(ctx, d, 4 * cw, LH);
  CHECK_SIZE(ctx, e, cw, 2 * LH);
  CHECK_SIZE(ctx, f, 6 * cw, LH);
  CHECK_SIZE(ctx, g, 2 * cw, LH);
  CHECK_SIZE(ctx, h, cw, 2 * LH);
  rd_ctx_destroy(ctx);
}

static void test_inheritance_and_update(void) {
  rd_ctx* ctx = setup();
  rd_node p   = add(ctx, rd_ctx_root(ctx), (rd_style){.font_size = rd_px(32), .align_items = RD_ALIGN_START});
  rd_node a   = add_text(ctx, p, (rd_style){0}, "a");
  rd_node b   = add_text(ctx, p, (rd_style){.line_height = rd_px(10)}, "a");
  rd_ctx_layout(ctx, 800, 600);
  CHECK_SIZE(ctx, a, 2 * cw, 2 * LH);
  CHECK_SIZE(ctx, b, 2 * cw, 10);

  rd_node_set_text(ctx, a, "aaaa", RD_TEXT_NUL);
  rd_ctx_layout(ctx, 800, 600);
  CHECK_SIZE(ctx, a, 8 * cw, 2 * LH);
  size_t len;
  CHECK(strcmp(rd_node_text(ctx, a, &len), "aaaa") == 0 && len == 4);
  CHECK(rd_node_text(ctx, p, &len) == NULL && len == 0);

  rd_node_set_text(ctx, a, "", 0);
  rd_ctx_layout(ctx, 800, 600);
  CHECK_SIZE(ctx, a, 0, 0);
  rd_ctx_destroy(ctx);
}

static void test_baseline(void) {
  rd_ctx* ctx = setup();
  rd_node row = add(ctx, rd_ctx_root(ctx), (rd_style){.flex_direction = RD_FLEX_ROW, .align_items = RD_ALIGN_BASELINE});
  rd_node a   = add_text(ctx, row, (rd_style){0}, "a");
  rd_node b   = add_text(ctx, row, (rd_style){.font_size = rd_px(32)}, "b");
  rd_ctx_layout(ctx, 800, 600);
  float ba = rd_node_baseline(ctx, a), bb = rd_node_baseline(ctx, b);
  CHECK(!isnan(ba) && !isnan(bb) && bb > ba);
  CHECK(near(rd_node_rect(ctx, a).y, bb - ba));
  CHECK(near(rd_node_rect(ctx, b).y, 0));
  CHECK(near(rd_node_rect(ctx, row).height, bb + fmaxf(LH - ba, 2 * LH - bb)));
  CHECK(near(rd_node_baseline(ctx, row), bb));
  CHECK(near(rd_node_baseline(ctx, rd_ctx_root(ctx)), bb));
  rd_ctx_destroy(ctx);
}

static void test_utf8_and_cjk(void) {
  rd_ctx* ctx = setup();
  float   nd  = rd_font_measure(ctx, (rd_font){0, true}, 16, "\xe4\xb8\x80", 3);
  rd_node row = add(ctx, rd_ctx_root(ctx), (rd_style){.flex_direction = RD_FLEX_ROW, .align_items = RD_ALIGN_START});
  rd_node a   = add_text(ctx, row, (rd_style){0}, "h\xc3\xa9llo");
  rd_node b   = add_text(ctx, row, (rd_style){0},
                         "a\xff"
                           "b");
  rd_node col = add(ctx, rd_ctx_root(ctx), (rd_style){.width = rd_px(2.5f * nd), .align_items = RD_ALIGN_START});
  rd_node c   = add_text(ctx, col, (rd_style){0}, "\xe4\xb8\x80\xe4\xba\x8c\xe4\xb8\x89\xe5\x9b\x9b");
  rd_ctx_layout(ctx, 800, 600);
  CHECK_SIZE(ctx, a, 5 * cw, LH);
  CHECK(rd_node_rect(ctx, b).width > 2 * cw - 0.01f);
  CHECK_SIZE(ctx, c, 2 * nd, 2 * LH);
  rd_ctx_destroy(ctx);
}

static int count_fills(rd_ctx* ctx, rd_node n, float* first_x, float* first_y) {
  uint32_t       len;
  const rd__cmd* cmds = rd__display_list(ctx, &len);
  int            c    = 0;
  for (uint32_t j = 0; j < len; j++) {
    if (cmds[j].kind != RD_CMD_FILL || cmds[j].node != n.index) continue;
    if (!c && first_x) *first_x = cmds[j].rect.x, *first_y = cmds[j].rect.y;
    c++;
  }
  return c;
}

static void test_clamp_and_highlights(void) {
  rd_ctx* ctx  = setup();
  rd_node box  = add(ctx, rd_ctx_root(ctx), (rd_style){.width = rd_px(7.5f * cw), .align_items = RD_ALIGN_START});
  rd_node para = add_text(ctx, box, (rd_style){.line_clamp = rd_num(2), .text_overflow = RD_TEXT_OVERFLOW_ELLIPSIS},
                          "aaa bbb ccc ddd eee fff");
  rd_ctx_layout(ctx, 800, 600);
  CHECK_SIZE(ctx, para, 7 * cw, 2 * LH);

  rd_highlight h[2] = {
      {4, 7, rd_rgb(255, 220, 0), rd_rgb(0, 0, 0)},
      {6, 13, rd_rgb(255, 0, 0), {0}},
  };
  CHECK(rd_node_set_highlights(ctx, para, h, 2));
  float x = 0, y = 0;
  CHECK(count_fills(ctx, para, &x, &y) == 3);
  CHECK(near(x, 4 * cw) && near(y, 0));

  rd_highlight hidden = {16, 19, rd_rgb(255, 0, 0), {0}};
  rd_node_set_highlights(ctx, para, &hidden, 1);
  CHECK(count_fills(ctx, para, NULL, NULL) == 0);
  rd_highlight past = {20, 99, rd_rgb(255, 0, 0), {0}};
  rd_node_set_highlights(ctx, para, &past, 1);
  CHECK(count_fills(ctx, para, NULL, NULL) == 0);
  rd_node_set_highlights(ctx, para, NULL, 0);
  CHECK(count_fills(ctx, para, NULL, NULL) == 0);

  rd_node_merge_style(ctx, para, &(rd_style){.line_clamp = rd_num(0)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_SIZE(ctx, para, 7 * cw, 3 * LH);
  CHECK(!rd_node_set_highlights(ctx, box, h, 2));
  rd_ctx_destroy(ctx);
}

static void test_shift_wheel_scrolls_horizontally(void) {
  rd_ctx* ctx = setup();
  rd_node box = add(ctx, rd_ctx_root(ctx),
                    (rd_style){.width = rd_px(100), .flex_direction = RD_FLEX_ROW, .overflow_x = RD_OVERFLOW_SCROLL});
  add_text(ctx, box, (rd_style){.white_space = RD_WHITE_SPACE_NOWRAP}, "a long line that does not fit in the box");
  rd_ctx_layout(ctx, 800, 600);
  CHECK(rd_node_content_size(ctx, box).width > 100);
  rd_ctx_key(ctx, RD_KEY_UNKNOWN, true, false, RD_MOD_SHIFT);
  rd_ctx_pointer(ctx, 10, 5, 0, 0, -1);
  CHECK(near(rd_node_scroll(ctx, box).x, 48) && near(rd_node_scroll(ctx, box).y, 0));
  rd_ctx_destroy(ctx);
}

static void test_no_font(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node a   = add_text(ctx, rd_ctx_root(ctx), (rd_style){.align_self = RD_ALIGN_START}, "hello");
  rd_ctx_layout(ctx, 800, 600);
  CHECK_SIZE(ctx, a, 0, 0);
  CHECK(isnan(rd_node_baseline(ctx, a)));
  rd_ctx_destroy(ctx);
}

int main(void) {
  test_font();
  test_max_content_and_wrap();
  test_min_content_in_grid();
  test_white_space();
  test_inheritance_and_update();
  test_baseline();
  test_utf8_and_cjk();
  test_clamp_and_highlights();
  test_shift_wheel_scrolls_horizontally();
  test_no_font();
  return TEST_RESULT();
}
