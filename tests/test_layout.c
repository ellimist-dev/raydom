#include <math.h>
#include <raydom/raydom.h>

#include "check.h"

static bool near(float a, float b) { return fabsf(a - b) < 0.01f; }

#define CHECK_RECT(ctx, n, X, Y, W, H) check_rect(ctx, n, X, Y, W, H, __LINE__)

static void check_rect(rd_ctx* ctx, rd_node n, float x, float y, float w, float h, int line) {
  rd_rect r = rd_node_rect(ctx, n);
  if (near(r.x, x) && near(r.y, y) && near(r.width, w) && near(r.height, h)) return;
  fprintf(stderr, "%s:%d: rect {%g, %g, %g, %g}, expected {%g, %g, %g, %g}\n", __FILE__, line, r.x, r.y, r.width,
          r.height, x, y, w, h);
  rd_test_failures++;
}

static rd_node add(rd_ctx* ctx, rd_node parent, rd_style style) {
  rd_node n = rd_node_create(ctx);
  rd_node_set_style(ctx, n, &style);
  rd_node_append(ctx, parent, n);
  return n;
}

typedef struct fake_text {
  float text_w, line_h;
  int   calls;
} fake_text;

// Wraps a fixed-width run of text at the available width.
static rd_size measure_text(void* data, float w, rd_measure_mode wm, float h, rd_measure_mode hm) {
  (void)h;
  (void)hm;
  fake_text* t = data;
  t->calls++;
  float width = t->text_w;
  if (wm != RD_MEASURE_UNDEFINED && w < width) width = w;
  float lines = width > 0 ? ceilf(t->text_w / width) : 1;
  return (rd_size){wm == RD_MEASURE_EXACTLY ? w : width, lines * t->line_h};
}

static void test_column_stretch(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node root = rd_ctx_root(ctx);
  rd_node a    = add(ctx, root, (rd_style){.height = rd_px(100)});
  rd_node b    = add(ctx, root, (rd_style){.height = rd_px(50), .margin = rd_edges_all(rd_px(10))});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, root, 0, 0, 800, 600);
  CHECK_RECT(ctx, a, 0, 0, 800, 100);
  CHECK_RECT(ctx, b, 10, 110, 780, 50);
  rd_ctx_destroy(ctx);
}

static void test_justify(void) {
  rd_ctx*  ctx  = rd_ctx_create();
  rd_node  root = rd_ctx_root(ctx);
  rd_node  c    = add(ctx, root,
                      (rd_style){.flex_direction  = RD_FLEX_ROW,
                                 .width           = rd_px(400),
                                 .height          = rd_px(100),
                                 .justify_content = RD_ALIGN_SPACE_BETWEEN});
  rd_style item = {.width = rd_px(50), .height = rd_px(50)};
  rd_node  a = add(ctx, c, item), b = add(ctx, c, item), d = add(ctx, c, item);
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, c, 0, 0, 400, 100);
  CHECK_RECT(ctx, a, 0, 0, 50, 50);
  CHECK_RECT(ctx, b, 175, 0, 50, 50);
  CHECK_RECT(ctx, d, 350, 0, 50, 50);

  rd_node_merge_style(ctx, c, &(rd_style){.justify_content = RD_ALIGN_CENTER});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 125, 0, 50, 50);
  CHECK_RECT(ctx, d, 225, 0, 50, 50);

  rd_node_merge_style(ctx, c, &(rd_style){.justify_content = RD_ALIGN_SPACE_EVENLY});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 62.5f, 0, 50, 50);
  CHECK_RECT(ctx, b, 175, 0, 50, 50);
  rd_ctx_destroy(ctx);
}

static void test_grow_and_gap(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node c   = add(ctx, rd_ctx_root(ctx), (rd_style){.flex_direction = RD_FLEX_ROW, .width = rd_px(400)});
  rd_node a   = add(ctx, c, (rd_style){.flex_grow = rd_num(1), .flex_basis = rd_px(0), .height = rd_px(10)});
  rd_node b   = add(ctx, c, (rd_style){.flex_grow = rd_num(2), .flex_basis = rd_px(0), .height = rd_px(10)});
  rd_node d   = add(ctx, c, (rd_style){.flex_grow = rd_num(1), .flex_basis = rd_px(0), .height = rd_px(10)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 0, 0, 100, 10);
  CHECK_RECT(ctx, b, 100, 0, 200, 10);
  CHECK_RECT(ctx, d, 300, 0, 100, 10);
  CHECK_RECT(ctx, c, 0, 0, 400, 10);

  rd_node_merge_style(ctx, c, &(rd_style){.column_gap = rd_px(10)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 0, 0, 95, 10);
  CHECK_RECT(ctx, b, 105, 0, 190, 10);
  CHECK_RECT(ctx, d, 305, 0, 95, 10);
  rd_ctx_destroy(ctx);
}

static void test_shrink(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node c   = add(ctx, rd_ctx_root(ctx), (rd_style){.flex_direction = RD_FLEX_ROW, .width = rd_px(200)});
  rd_node a   = add(ctx, c, (rd_style){.width = rd_px(200), .flex_shrink = rd_num(1)});
  rd_node b   = add(ctx, c, (rd_style){.width = rd_px(100), .flex_shrink = rd_num(1)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 0, 0, 133.33f, 0);
  CHECK_RECT(ctx, b, 133.33f, 0, 66.67f, 0);

  rd_node_merge_style(ctx, b, &(rd_style){.flex_shrink = rd_num(0)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 0, 0, 100, 0);
  CHECK_RECT(ctx, b, 100, 0, 100, 0);

  rd_node_merge_style(ctx, b, &(rd_style){.flex_shrink = rd_num(1), .width = rd_px(200)});
  rd_node_merge_style(ctx, a, &(rd_style){.min_width = rd_px(150)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 0, 0, 150, 0);
  CHECK_RECT(ctx, b, 150, 0, 50, 0);

  // Shrinking is weighted by the content-box base size (CSS Flexbox §9.7): a's is 60, b's 100, so of
  // the 100px overflow a takes 37.5 and b 62.5.
  rd_node row = add(ctx, rd_ctx_root(ctx), (rd_style){.flex_direction = RD_FLEX_ROW, .width = rd_px(100)});
  rd_node p   = add(ctx, row,
                    (rd_style){.flex_basis  = rd_px(100),
                               .flex_shrink = rd_num(1),
                               .padding     = rd_edges_vh(rd_px(0), rd_px(20))});
  rd_node q   = add(ctx, row, (rd_style){.flex_basis = rd_px(100), .flex_shrink = rd_num(1)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, p, 0, 0, 62.5f, 0);
  CHECK_RECT(ctx, q, 62.5f, 0, 37.5f, 0);
  rd_ctx_destroy(ctx);
}

// Factors and lengths too large to add up still lay out, and finish.
static void test_non_finite(void) {
  rd_style cases[] = {
      {.height = rd_px(10), .flex_grow = rd_num(INFINITY)},
      {.height = rd_px(10), .flex_grow = rd_num(3e38f)},
      {.height = rd_px(10), .width = rd_px(INFINITY), .flex_shrink = rd_num(1)},
      {.height = rd_px(10), .width = rd_px(1e38f), .flex_shrink = rd_num(INFINITY)},
      {.height = rd_px(10), .flex_basis = rd_px(-INFINITY), .flex_grow = rd_num(1), .margin = {.left = rd_px(1e38f)}},
  };
  for (size_t c = 0; c < sizeof cases / sizeof *cases; c++) {
    rd_ctx* ctx  = rd_ctx_create();
    rd_node root = rd_ctx_root(ctx);
    rd_node_set_style(ctx, root, &(rd_style){.flex_direction = RD_FLEX_ROW});
    rd_node a = add(ctx, root, cases[c]);
    rd_node b = add(ctx, root, cases[c]);
    rd_ctx_layout(ctx, 300, 200);
    for (int j = 0; j < 2; j++) {
      rd_rect r = rd_node_rect(ctx, j ? b : a);
      CHECK(isfinite(r.x) && isfinite(r.y) && isfinite(r.width) && isfinite(r.height));
    }
    if (c < 2) {
      CHECK_RECT(ctx, a, 0, 0, 150, 10);
      CHECK_RECT(ctx, b, 150, 0, 150, 10);
    }
    rd_ctx_destroy(ctx);
  }
}

static void test_max_clamp(void) {
  rd_ctx*  ctx     = rd_ctx_create();
  rd_node  c       = add(ctx, rd_ctx_root(ctx), (rd_style){.flex_direction = RD_FLEX_ROW, .width = rd_px(300)});
  rd_style grow    = {.flex_grow = rd_num(1), .flex_basis = rd_px(0)};
  rd_style capped  = grow;
  capped.max_width = rd_px(50);
  rd_node a = add(ctx, c, capped), b = add(ctx, c, grow), d = add(ctx, c, grow);
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 0, 0, 50, 0);
  CHECK_RECT(ctx, b, 50, 0, 125, 0);
  CHECK_RECT(ctx, d, 175, 0, 125, 0);
  rd_ctx_destroy(ctx);
}

static void test_box_model(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node c   = add(ctx, rd_ctx_root(ctx),
                    (rd_style){.width        = rd_px(200),
                               .height       = rd_px(200),
                               .padding      = rd_edges_all(rd_px(10)),
                               .border_width = rd_edges_all(rd_px(5))});
  rd_node a   = add(ctx, c, (rd_style){.height = rd_px(20), .margin = rd_edges_all(rd_px(5))});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 20, 20, 160, 20);
  rd_rect bounds = rd_node_bounds(ctx, a);
  CHECK(near(bounds.x, 20) && near(bounds.y, 20));
  rd_box box = rd_node_box(ctx, c);
  CHECK(box.padding.left == 10 && box.border.top == 5);
  CHECK(rd_node_box(ctx, a).margin.right == 5);
  rd_ctx_destroy(ctx);
}

static void test_percentages(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node c   = add(ctx, rd_ctx_root(ctx),
                    (rd_style){.flex_direction = RD_FLEX_ROW, .width = rd_px(400), .height = rd_px(100)});
  rd_node a   = add(ctx, c,
                    (rd_style){.width = rd_pct(25), .height = rd_pct(50), .padding = rd_edges_all(rd_pct(5))});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 0, 0, 100, 50);
  rd_box box = rd_node_box(ctx, a);
  CHECK(near(box.padding.left, 20) && near(box.padding.top, 20));
  rd_ctx_destroy(ctx);
}

static void test_wrap(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node c   = add(ctx, rd_ctx_root(ctx),
                    (rd_style){.flex_direction = RD_FLEX_ROW,
                               .flex_wrap      = RD_FLEX_WRAP,
                               .width          = rd_px(350),
                               .column_gap     = rd_px(10),
                               .row_gap        = rd_px(5)});
  rd_node items[5];
  for (int j = 0; j < 5; j++) items[j] = add(ctx, c, (rd_style){.width = rd_px(100), .height = rd_px(40)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, c, 0, 0, 350, 85);
  CHECK_RECT(ctx, items[2], 220, 0, 100, 40);
  CHECK_RECT(ctx, items[3], 0, 45, 100, 40);
  CHECK_RECT(ctx, items[4], 110, 45, 100, 40);

  rd_node_merge_style(ctx, c, &(rd_style){.height = rd_px(200)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, items[3], 0, 102.5f, 100, 40);

  rd_node_merge_style(ctx, c, &(rd_style){.align_content = RD_ALIGN_START});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, items[3], 0, 45, 100, 40);

  rd_node_merge_style(ctx, c, &(rd_style){.flex_wrap = RD_FLEX_WRAP_REVERSE});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, items[0], 0, 160, 100, 40);
  CHECK_RECT(ctx, items[3], 0, 115, 100, 40);
  rd_ctx_destroy(ctx);
}

static void test_align(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node c   = add(ctx, rd_ctx_root(ctx),
                    (rd_style){.flex_direction = RD_FLEX_ROW,
                               .width          = rd_px(300),
                               .height         = rd_px(100),
                               .align_items    = RD_ALIGN_CENTER});
  rd_node a   = add(ctx, c, (rd_style){.width = rd_px(50), .height = rd_px(20)});
  rd_node b   = add(ctx, c, (rd_style){.width = rd_px(50), .height = rd_px(20), .align_self = RD_ALIGN_END});
  rd_node d   = add(ctx, c, (rd_style){.width = rd_px(50), .align_self = RD_ALIGN_STRETCH});
  rd_node e   = add(ctx, c, (rd_style){.width = rd_px(50), .align_self = RD_ALIGN_STRETCH, .max_height = rd_px(60)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 0, 40, 50, 20);
  CHECK_RECT(ctx, b, 50, 80, 50, 20);
  CHECK_RECT(ctx, d, 100, 0, 50, 100);
  CHECK_RECT(ctx, e, 150, 0, 50, 60);
  rd_ctx_destroy(ctx);
}

static void test_auto_margins(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node c   = add(ctx, rd_ctx_root(ctx),
                    (rd_style){.flex_direction = RD_FLEX_ROW, .width = rd_px(300), .height = rd_px(50)});
  rd_node a   = add(ctx, c, (rd_style){.width = rd_px(50), .height = rd_px(10), .margin.right = rd_auto()});
  rd_node b   = add(ctx, c,
                    (rd_style){.width = rd_px(50), .height = rd_px(10), .margin.top = rd_auto(), .margin.bottom = rd_auto()});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 0, 0, 50, 10);
  CHECK_RECT(ctx, b, 250, 20, 50, 10);

  rd_node_merge_style(ctx, a, &(rd_style){.margin.left = rd_auto()});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 100, 0, 50, 10);
  CHECK_RECT(ctx, b, 250, 20, 50, 10);
  rd_ctx_destroy(ctx);
}

static void test_reverse(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node c   = add(ctx, rd_ctx_root(ctx), (rd_style){.flex_direction = RD_FLEX_ROW_REVERSE, .width = rd_px(300)});
  rd_node a   = add(ctx, c, (rd_style){.width = rd_px(50), .margin.left = rd_px(5)});
  rd_node b   = add(ctx, c, (rd_style){.width = rd_px(100)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 250, 0, 50, 0);
  CHECK_RECT(ctx, b, 145, 0, 100, 0);

  rd_node_merge_style(ctx, c, &(rd_style){.justify_content = RD_ALIGN_END});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 105, 0, 50, 0);
  CHECK_RECT(ctx, b, 0, 0, 100, 0);

  rd_node col = add(ctx, rd_ctx_root(ctx), (rd_style){.flex_direction = RD_FLEX_COLUMN_REVERSE});
  rd_node x   = add(ctx, col, (rd_style){.height = rd_px(10)});
  rd_node y   = add(ctx, col, (rd_style){.height = rd_px(30)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, col, 0, 0, 800, 40);
  CHECK_RECT(ctx, x, 0, 30, 800, 10);
  CHECK_RECT(ctx, y, 0, 0, 800, 30);
  rd_ctx_destroy(ctx);
}

static void test_measure(void) {
  rd_ctx*   ctx = rd_ctx_create();
  fake_text t1 = {120, 20, 0}, t2 = {120, 20, 0};
  rd_node   col = add(ctx, rd_ctx_root(ctx), (rd_style){.width = rd_px(50)});
  rd_node   a   = add(ctx, col, (rd_style){0});
  rd_node_set_measure(ctx, a, measure_text, &t1, NULL);
  rd_node row = add(ctx, rd_ctx_root(ctx), (rd_style){.flex_direction = RD_FLEX_ROW, .width = rd_px(300)});
  rd_node b   = add(ctx, row, (rd_style){.flex_shrink = rd_num(1), .padding = rd_edges_all(rd_px(2))});
  rd_node_set_measure(ctx, b, measure_text, &t2, NULL);
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 0, 0, 50, 60);
  CHECK_RECT(ctx, col, 0, 0, 50, 60);
  CHECK_RECT(ctx, b, 0, 0, 124, 24);

  rd_node_merge_style(ctx, row, &(rd_style){.width = rd_px(104)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, b, 0, 0, 104, 44);
  CHECK_RECT(ctx, row, 0, 60, 104, 44);
  rd_ctx_destroy(ctx);
}

static void test_absolute(void) {
  rd_ctx*   ctx  = rd_ctx_create();
  rd_node   c    = add(ctx, rd_ctx_root(ctx),
                       (rd_style){.width = rd_px(400), .height = rd_px(300), .padding = rd_edges_all(rd_px(10))});
  rd_node   flow = add(ctx, c, (rd_style){.width = rd_px(100), .height = rd_px(100)});
  rd_node   a    = add(ctx, c,
                       (rd_style){.position = RD_POSITION_ABSOLUTE,
                                  .inset    = {.left = rd_px(20), .top = rd_px(30)},
                                  .width    = rd_px(100),
                                  .height   = rd_px(50)});
  rd_node   b    = add(ctx, c,
                       (rd_style){.position = RD_POSITION_ABSOLUTE,
                                  .inset    = {.right = rd_px(10), .bottom = rd_px(10)},
                                  .width    = rd_px(50),
                                  .height   = rd_px(50)});
  rd_node   d    = add(ctx, c,
                       (rd_style){.position = RD_POSITION_ABSOLUTE,
                                  .inset    = {.left = rd_px(0), .right = rd_px(0), .top = rd_px(0)},
                                  .height   = rd_px(20)});
  rd_node   e    = add(ctx, c, (rd_style){.position = RD_POSITION_ABSOLUTE});
  fake_text t    = {80, 10, 0};
  rd_node_set_measure(ctx, e, measure_text, &t, NULL);
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, flow, 10, 10, 100, 100);
  CHECK_RECT(ctx, a, 20, 30, 100, 50);
  CHECK_RECT(ctx, b, 340, 240, 50, 50);
  CHECK_RECT(ctx, d, 0, 0, 400, 20);
  CHECK_RECT(ctx, e, 10, 10, 80, 10);
  rd_ctx_destroy(ctx);
}

// CSS 2 §10.3.7 and §10.6.4: with both insets and a size, auto margins take what's left.
static void test_absolute_auto_margins(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node root = rd_ctx_root(ctx);
  rd_node a    = add(ctx, root,
                     (rd_style){.position = RD_POSITION_ABSOLUTE,
                                .inset    = rd_edges_all(rd_px(0)),
                                .width    = rd_px(100),
                                .height   = rd_px(50),
                                .margin   = rd_edges_all(rd_auto())});
  rd_node b    = add(ctx, root,
                     (rd_style){.position = RD_POSITION_ABSOLUTE,
                                .inset    = rd_edges_all(rd_px(10)),
                                .width    = rd_px(100),
                                .height   = rd_px(50),
                                .margin   = {.left = rd_auto(), .top = rd_auto(), .bottom = rd_px(20)}});
  rd_node c    = add(ctx, root,
                     (rd_style){.position = RD_POSITION_ABSOLUTE,
                                .inset    = rd_edges_all(rd_px(0)),
                                .width    = rd_px(500),
                                .height   = rd_px(400),
                                .margin   = rd_edges_all(rd_auto())});
  rd_node d    = add(ctx, root,
                     (rd_style){.position = RD_POSITION_ABSOLUTE,
                                .inset    = {.left = rd_px(0), .top = rd_px(0)},
                                .width    = rd_px(100),
                                .height   = rd_px(50),
                                .margin   = rd_edges_all(rd_auto())});
  rd_node e    = add(ctx, root,
                     (rd_style){.position = RD_POSITION_ABSOLUTE,
                                .inset    = rd_edges_all(rd_px(0)),
                                .height   = rd_px(50),
                                .margin   = rd_edges_all(rd_auto())});
  rd_ctx_layout(ctx, 400, 300);
  CHECK_RECT(ctx, a, 150, 125, 100, 50);
  CHECK_RECT(ctx, b, 290, 220, 100, 50);
  CHECK_RECT(ctx, c, 0, -50, 500, 400);  // a negative split puts the left margin at 0, not the top
  CHECK_RECT(ctx, d, 0, 0, 100, 50);     // without both insets, auto margins are 0
  CHECK_RECT(ctx, e, 0, 125, 400, 50);   // an auto width fills the space, with 0 margins
  rd_ctx_destroy(ctx);
}

static void test_display_none_and_relative(void) {
  rd_ctx* ctx   = rd_ctx_create();
  rd_node c     = add(ctx, rd_ctx_root(ctx), (rd_style){.flex_direction = RD_FLEX_ROW});
  rd_node a     = add(ctx, c, (rd_style){.width = rd_px(50), .display = RD_DISPLAY_NONE});
  rd_node b     = add(ctx, c, (rd_style){.width = rd_px(50), .inset = {.left = rd_px(5), .top = rd_px(7)}});
  rd_node inner = add(ctx, a, (rd_style){.width = rd_px(10)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 0, 0, 0, 0);
  CHECK_RECT(ctx, b, 5, 7, 50, 0);

  rd_node_merge_style(ctx, inner, &(rd_style){.width = rd_px(20)});
  rd_ctx_layout(ctx, 800, 600);
  rd_node_merge_style(ctx, a, &(rd_style){.display = RD_DISPLAY_FLEX});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 0, 0, 50, 0);
  CHECK_RECT(ctx, inner, 0, 0, 20, 0);
  CHECK_RECT(ctx, b, 55, 7, 50, 0);
  rd_ctx_destroy(ctx);
}

static void test_auto_height_and_content(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node c   = add(ctx, rd_ctx_root(ctx), (rd_style){.padding = rd_edges_all(rd_px(10)), .row_gap = rd_px(5)});
  add(ctx, c, (rd_style){.height = rd_px(20)});
  add(ctx, c, (rd_style){.height = rd_px(30)});
  rd_node s    = add(ctx, rd_ctx_root(ctx),
                     (rd_style){.width = rd_px(100), .height = rd_px(100), .overflow_y = RD_OVERFLOW_SCROLL});
  rd_node tall = add(ctx, s, (rd_style){.height = rd_px(300)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, c, 0, 0, 800, 75);
  CHECK_RECT(ctx, tall, 0, 0, 100, 300);
  rd_size content = rd_node_content_size(ctx, s);
  CHECK(near(content.width, 100) && near(content.height, 300));
  rd_ctx_destroy(ctx);
}

static void test_caching(void) {
  rd_ctx*   ctx = rd_ctx_create();
  fake_text t   = {120, 20, 0};
  rd_node   col = add(ctx, rd_ctx_root(ctx), (rd_style){.width = rd_px(50)});
  rd_node   sib = add(ctx, col, (rd_style){.height = rd_px(10)});
  rd_node   a   = add(ctx, col, (rd_style){0});
  rd_node_set_measure(ctx, a, measure_text, &t, NULL);
  rd_ctx_layout(ctx, 800, 600);
  int calls = t.calls;
  CHECK(calls > 0);

  rd_ctx_layout(ctx, 800, 600);
  CHECK(t.calls == calls);

  rd_node_merge_style(ctx, a, &(rd_style){.background = rd_hex(0xff0000)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK(t.calls == calls);

  rd_node_merge_style(ctx, sib, &(rd_style){.height = rd_px(40)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK(t.calls == calls);
  CHECK_RECT(ctx, a, 0, 40, 50, 60);

  rd_ctx_layout(ctx, 1000, 600);
  CHECK(t.calls == calls);

  rd_node_mark_layout_dirty(ctx, a);
  rd_ctx_layout(ctx, 1000, 600);
  CHECK(t.calls > calls);
  rd_ctx_destroy(ctx);
}

static void baseline_tree(rd_ctx* ctx, rd_node out[2], fake_text* t) {
  rd_node root = rd_ctx_root(ctx);
  rd_node_set_style(ctx, root, &(rd_style){.align_items = RD_ALIGN_START});
  rd_node a = add(ctx, root, (rd_style){0});
  out[0]    = add(ctx, a,
                  (rd_style){.flex_direction = RD_FLEX_ROW, .align_items = RD_ALIGN_BASELINE, .width = rd_pct(50)});
  out[1]    = add(ctx, out[0], (rd_style){.flex_shrink = rd_num(1)});
  rd_node_set_measure(ctx, out[1], measure_text, t, NULL);
}

// Measuring a baseline row lays its items out; the row's own cached layout must not keep them so.
static void test_baseline_relayout(void) {
  fake_text t1 = {80, 10, 0}, t2 = {80, 10, 0};
  rd_ctx*   inc   = rd_ctx_create();
  rd_ctx*   fresh = rd_ctx_create();
  rd_node   n[2], m[2];
  baseline_tree(inc, n, &t1);
  baseline_tree(fresh, m, &t2);
  rd_ctx_layout(inc, 300, 200);
  rd_ctx_layout(inc, 400, 200);
  rd_ctx_layout(fresh, 400, 200);
  for (int j = 0; j < 2; j++) {
    rd_rect r = rd_node_rect(fresh, m[j]);
    check_rect(inc, n[j], r.x, r.y, r.width, r.height, __LINE__);
  }
  rd_ctx_destroy(inc);
  rd_ctx_destroy(fresh);
}

int main(void) {
  test_column_stretch();
  test_justify();
  test_grow_and_gap();
  test_shrink();
  test_non_finite();
  test_max_clamp();
  test_box_model();
  test_percentages();
  test_wrap();
  test_align();
  test_auto_margins();
  test_reverse();
  test_measure();
  test_absolute();
  test_absolute_auto_margins();
  test_display_none_and_relative();
  test_auto_height_and_content();
  test_caching();
  test_baseline_relayout();
  return TEST_RESULT();
}
