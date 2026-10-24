#include <math.h>
#include <raydom/raydom.h>

#include "check.h"
#include "internal.h"

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

typedef struct words {
  int   count;
  float word_w, line_h;
} words;

// Equal-width words that wrap at word boundaries.
static rd_size measure_words(void* data, float w, rd_measure_mode wm, float h, rd_measure_mode hm) {
  (void)h;
  (void)hm;
  const words* t = data;
  if (wm == RD_MEASURE_MIN_CONTENT) return (rd_size){t->word_w, (float)t->count * t->line_h};
  if (wm == RD_MEASURE_UNDEFINED) return (rd_size){(float)t->count * t->word_w, t->line_h};
  int per   = (int)fmaxf(1, floorf(w / t->word_w));
  int lines = (t->count + per - 1) / per;
  int cols  = per < t->count ? per : t->count;
  return (rd_size){wm == RD_MEASURE_EXACTLY ? w : (float)cols * t->word_w, (float)lines * t->line_h};
}

static rd_node add_text(rd_ctx* ctx, rd_node parent, rd_style style, words* w) {
  rd_node n = add(ctx, parent, style);
  rd_node_set_measure(ctx, n, measure_words, w, NULL);
  return n;
}

#define TRACKS(ctx, ...) \
  rd_tracks_create(ctx, (rd_track[]){__VA_ARGS__}, sizeof((rd_track[]){__VA_ARGS__}) / sizeof(rd_track))

static void test_fixed_and_fr(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node g   = add(ctx, rd_ctx_root(ctx),
                    (rd_style){
                        .display               = RD_DISPLAY_GRID,
                        .width                 = rd_px(400),
                        .column_gap            = rd_px(10),
                        .grid_template_columns = TRACKS(ctx, rd_track_of(rd_px(100)), rd_track_of(rd_fr(1)),
                                                        rd_track_of(rd_fr(2))),
                  });
  rd_node items[6];
  for (int j = 0; j < 6; j++) items[j] = add(ctx, g, (rd_style){.height = rd_px(20)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, g, 0, 0, 400, 40);
  CHECK_RECT(ctx, items[0], 0, 0, 100, 20);
  CHECK_RECT(ctx, items[1], 110, 0, 93.33f, 20);
  CHECK_RECT(ctx, items[2], 213.33f, 0, 186.67f, 20);
  CHECK_RECT(ctx, items[3], 0, 20, 100, 20);
  CHECK_RECT(ctx, items[5], 213.33f, 20, 186.67f, 20);
  rd_ctx_destroy(ctx);
}

static void test_placement(void) {
  rd_ctx*   ctx = rd_ctx_create();
  rd_tracks t   = TRACKS(ctx, rd_track_of(rd_px(100)), rd_track_of(rd_px(100)), rd_track_of(rd_px(100)));
  rd_tracks r   = TRACKS(ctx, rd_track_of(rd_px(50)), rd_track_of(rd_px(50)), rd_track_of(rd_px(50)));
  rd_node   g   = add(ctx, rd_ctx_root(ctx),
                      (rd_style){.display = RD_DISPLAY_GRID, .grid_template_columns = t, .grid_template_rows = r});
  rd_node   a   = add(ctx, g,
                      (rd_style){.grid_column_start = rd_line(2), .grid_column_end = rd_span(2), .grid_row_start = rd_line(1)});
  rd_node   b   = add(ctx, g, (rd_style){.grid_column_start = rd_line(-2), .grid_row_start = rd_line(3)});
  rd_node   c   = add(ctx, g, (rd_style){0});
  rd_node   d   = add(ctx, g, (rd_style){0});
  rd_node   e   = add(ctx, g, (rd_style){.grid_column_start = rd_span(2)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 100, 0, 200, 50);
  CHECK_RECT(ctx, b, 200, 100, 100, 50);
  CHECK_RECT(ctx, c, 0, 0, 100, 50);
  CHECK_RECT(ctx, d, 0, 50, 100, 50);
  CHECK_RECT(ctx, e, 100, 50, 200, 50);
  rd_ctx_destroy(ctx);
}

static void test_auto_track_content(void) {
  rd_ctx* ctx = rd_ctx_create();
  words   w   = {3, 30, 20};
  rd_node g   = add(ctx, rd_ctx_root(ctx),
                    (rd_style){
                        .display               = RD_DISPLAY_GRID,
                        .width                 = rd_px(300),
                        .grid_template_columns = TRACKS(ctx, rd_track_of(rd_auto()), rd_track_of(rd_fr(1))),
                  });
  rd_node a   = add_text(ctx, g, (rd_style){0}, &w);
  rd_node b   = add(ctx, g, (rd_style){0});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 0, 0, 90, 20);
  CHECK_RECT(ctx, b, 90, 0, 210, 20);
  rd_ctx_destroy(ctx);
}

static void test_fr_min_content_floor(void) {
  rd_ctx* ctx = rd_ctx_create();
  words   w   = {1, 80, 20};
  rd_node g   = add(ctx, rd_ctx_root(ctx),
                    (rd_style){
                        .display               = RD_DISPLAY_GRID,
                        .width                 = rd_px(100),
                        .grid_template_columns = TRACKS(ctx, rd_track_of(rd_fr(1)), rd_track_of(rd_fr(1))),
                  });
  rd_node a   = add_text(ctx, g, (rd_style){0}, &w);
  rd_node b   = add(ctx, g, (rd_style){0});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 0, 0, 80, 20);
  CHECK_RECT(ctx, b, 80, 0, 20, 20);

  rd_node_merge_style(ctx, g, &(rd_style){.grid_template_columns = TRACKS(ctx, rd_track_minmax(rd_px(0), rd_fr(1)), rd_track_minmax(rd_px(0), rd_fr(1)))});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 0, 0, 50, 20);
  CHECK_RECT(ctx, b, 50, 0, 50, 20);
  rd_ctx_destroy(ctx);
}

static void test_row_height_from_wrapped_text(void) {
  rd_ctx* ctx = rd_ctx_create();
  words   w   = {5, 30, 20};
  rd_node g   = add(ctx, rd_ctx_root(ctx),
                    (rd_style){.display = RD_DISPLAY_GRID, .grid_template_columns = TRACKS(ctx, rd_track_of(rd_px(100)))});
  rd_node a   = add_text(ctx, g, (rd_style){0}, &w);
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, g, 0, 0, 800, 40);
  CHECK_RECT(ctx, a, 0, 0, 100, 40);
  rd_ctx_destroy(ctx);
}

static void test_alignment(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node g   = add(ctx, rd_ctx_root(ctx),
                    (rd_style){
                        .display               = RD_DISPLAY_GRID,
                        .width                 = rd_px(300),
                        .justify_items         = RD_ALIGN_CENTER,
                        .justify_content       = RD_ALIGN_CENTER,
                        .grid_template_columns = TRACKS(ctx, rd_track_of(rd_px(100))),
                        .grid_template_rows    = TRACKS(ctx, rd_track_of(rd_px(50))),
                  });
  rd_node a   = add(ctx, g, (rd_style){.width = rd_px(20), .height = rd_px(10), .align_self = RD_ALIGN_END});
  rd_node b   = add(ctx, g, (rd_style){.justify_self = RD_ALIGN_STRETCH, .margin.left = rd_px(5)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 140, 40, 20, 10);
  CHECK_RECT(ctx, b, 105, 50, 95, 0);
  rd_ctx_destroy(ctx);
}

static void test_column_flow_and_auto_rows(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node g   = add(ctx, rd_ctx_root(ctx),
                    (rd_style){
                        .display            = RD_DISPLAY_GRID,
                        .width              = rd_px(200),
                        .grid_auto_flow     = RD_GRID_FLOW_COLUMN,
                        .grid_template_rows = TRACKS(ctx, rd_track_of(rd_px(50)), rd_track_of(rd_px(50))),
                  });
  rd_node a   = add(ctx, g, (rd_style){0});
  rd_node b   = add(ctx, g, (rd_style){0});
  rd_node c   = add(ctx, g, (rd_style){0});
  rd_node h   = add(ctx, rd_ctx_root(ctx),
                    (rd_style){
                        .display               = RD_DISPLAY_GRID,
                        .grid_template_columns = TRACKS(ctx, rd_track_of(rd_fr(1))),
                        .grid_auto_rows        = rd_track_of(rd_px(30)),
                  });
  rd_node rows[3];
  for (int j = 0; j < 3; j++) rows[j] = add(ctx, h, (rd_style){0});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 0, 0, 100, 50);
  CHECK_RECT(ctx, b, 0, 50, 100, 50);
  CHECK_RECT(ctx, c, 100, 0, 100, 50);
  CHECK_RECT(ctx, h, 0, 100, 800, 90);
  CHECK_RECT(ctx, rows[2], 0, 60, 800, 30);
  rd_ctx_destroy(ctx);
}

static void test_percent_tracks_and_span(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node g   = add(ctx, rd_ctx_root(ctx),
                    (rd_style){
                        .display               = RD_DISPLAY_GRID,
                        .width                 = rd_px(400),
                        .grid_template_columns = TRACKS(ctx, rd_track_of(rd_pct(25)), rd_track_of(rd_pct(75))),
                  });
  rd_node a   = add(ctx, g, (rd_style){.height = rd_px(10)});
  rd_node b   = add(ctx, g, (rd_style){.height = rd_px(10)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, a, 0, 0, 100, 10);
  CHECK_RECT(ctx, b, 100, 0, 300, 10);

  words   w     = {8, 25, 20};
  rd_node s     = add(ctx, rd_ctx_root(ctx),
                      (rd_style){
                          .display               = RD_DISPLAY_GRID,
                          .width                 = rd_px(300),
                          .grid_template_columns = TRACKS(ctx, rd_track_of(rd_auto()), rd_track_of(rd_auto())),
                  });
  rd_node wide  = add_text(ctx, s, (rd_style){.grid_column_start = rd_span(2)}, &w);
  rd_node probe = add(ctx, s, (rd_style){.grid_column_start = rd_line(2), .grid_row_start = rd_line(2)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, wide, 0, 0, 300, 20);
  CHECK_RECT(ctx, probe, 150, 20, 150, 0);
  rd_ctx_destroy(ctx);
}

static void test_grid_in_flex_row(void) {
  rd_ctx* ctx = rd_ctx_create();
  words   w   = {4, 30, 20};
  rd_node row = add(ctx, rd_ctx_root(ctx), (rd_style){.flex_direction = RD_FLEX_ROW});
  rd_node g   = add(ctx, row,
                    (rd_style){
                        .display               = RD_DISPLAY_GRID,
                        .column_gap            = rd_px(10),
                        .grid_template_columns = TRACKS(ctx, rd_track_of(rd_auto()), rd_track_of(rd_px(50))),
                  });
  rd_node a   = add_text(ctx, g, (rd_style){0}, &w);
  rd_ctx_layout(ctx, 800, 600);
  CHECK_RECT(ctx, g, 0, 0, 180, 20);
  CHECK_RECT(ctx, a, 0, 0, 120, 20);
  rd_ctx_destroy(ctx);
}

static rd_node grid_with(rd_ctx* ctx, rd_node parent, rd_tracks cols, rd_style extra, int items) {
  rd_node g = rd_node_create(ctx);
  extra.display               = RD_DISPLAY_GRID;
  extra.grid_template_columns = cols;
  extra.grid_auto_rows        = rd_track_of(rd_px(20));
  rd_node_set_style(ctx, g, &extra);
  rd_node_append(ctx, parent, g);
  for (int j = 0; j < items; j++) rd_node_append(ctx, g, rd_node_create(ctx));
  return g;
}

static float child_w(rd_ctx* ctx, rd_node g, int k) {
  rd_node c = rd_node_first_child(ctx, g);
  for (int j = 0; j < k; j++) c = rd_node_next_sibling(ctx, c);
  return rd_node_rect(ctx, c).width;
}

static float child_x(rd_ctx* ctx, rd_node g, int k) {
  rd_node c = rd_node_first_child(ctx, g);
  for (int j = 0; j < k; j++) c = rd_node_next_sibling(ctx, c);
  return rd_node_rect(ctx, c).x;
}

static void auto_repeat(void) {
  rd_ctx*   ctx   = rd_ctx_create();
  rd_node   root  = rd_ctx_root(ctx);
  rd_track  card  = rd_track_minmax(rd_px(100), rd_fr(1));
  rd_tracks fill  = rd_tracks_create_auto(ctx, RD_REPEAT_AUTO_FILL, &card, 1);
  rd_tracks fit   = rd_tracks_create_auto(ctx, RD_REPEAT_AUTO_FIT, &card, 1);
  rd_style  sized = {.width = rd_px(430), .column_gap = rd_px(10)};
  CHECK(fill.id && fit.id && fill.id != fit.id && !rd_tracks_create_auto(ctx, RD_REPEAT_NONE, &card, 1).id);

  // 430px: (430 + 10) / (100 + 10) = 4 columns of 100. auto-fill keeps the empty fourth.
  rd_node a = grid_with(ctx, root, fill, sized, 3);
  // auto-fit drops the two empty columns: two items share the width.
  rd_node b = grid_with(ctx, root, fit, sized, 2);
  // Fixed tracks: 350 / 100 = 3 columns, the fourth item wraps.
  rd_track  hundred = rd_track_of(rd_px(100));
  rd_node   c       = grid_with(ctx, root, rd_tracks_create_auto(ctx, RD_REPEAT_AUTO_FILL, &hundred, 1),
                                (rd_style){.width = rd_px(350)}, 4);
  // No definite width: the max width decides, in a row where the grid sizes to content.
  rd_node row = rd_node_create(ctx);
  rd_node_set_style(ctx, row, &(rd_style){.flex_direction = RD_FLEX_ROW, .align_items = RD_ALIGN_START});
  rd_node_append(ctx, root, row);
  rd_tracks fixed = rd_tracks_create_auto(ctx, RD_REPEAT_AUTO_FILL, &hundred, 1);
  rd_node   d     = grid_with(ctx, row, fixed, (rd_style){.max_width = rd_px(250)}, 1);
  // ...or the min width: the fewest repetitions that reach it.
  rd_node   e     = grid_with(ctx, row, fixed, (rd_style){.min_width = rd_px(330)}, 1);
  // A header spanning every repeated column with line -1.
  rd_node   f     = grid_with(ctx, root, fill, sized, 0);
  rd_node   head  = rd_node_create(ctx);
  rd_node_set_style(ctx, head, &(rd_style){.grid_column_start = rd_line(1), .grid_column_end = rd_line(-1)});
  rd_node_append(ctx, f, head);
  rd_ctx_layout(ctx, 800, 2000);

  CHECK(near(child_w(ctx, a, 0), 100) && near(child_x(ctx, a, 2), 220));
  CHECK(near(child_w(ctx, b, 0), 210) && near(child_x(ctx, b, 1), 220));
  CHECK(near(child_w(ctx, c, 0), 100) && near(child_x(ctx, c, 3), 0) && near(rd_node_rect(ctx, c).height, 40));
  CHECK(near(rd_node_rect(ctx, d).width, 200));
  CHECK(near(rd_node_rect(ctx, e).width, 400));
  CHECK(near(rd_node_rect(ctx, head).width, 430));

  // Resizing changes the count without new track lists.
  uint32_t lists = ctx->track_list_count;
  rd_node_merge_style(ctx, a, &(rd_style){.width = rd_px(700)});  // (700 + 10) / 110: six columns
  rd_ctx_layout(ctx, 800, 2000);
  CHECK(near(child_w(ctx, a, 0), (700 - 50) / 6.0f) && ctx->track_list_count == lists);
  rd_ctx_destroy(ctx);
}

static void interning(void) {
  rd_ctx*  ctx  = rd_ctx_create();
  rd_track a[2] = {rd_track_of(rd_px(100)), rd_track_of(rd_fr(1))};
  rd_track b[2] = {rd_track_of(rd_px(100)), rd_track_of(rd_fr(2))};
  rd_tracks x = rd_tracks_create(ctx, a, 2), y = rd_tracks_create(ctx, b, 2), z = rd_tracks_create(ctx, a, 2);
  CHECK(x.id && y.id && x.id != y.id && z.id == x.id);  // identical lists share a handle
  CHECK(rd_tracks_create(ctx, a, 1).id != x.id && rd_tracks_create(ctx, NULL, 0).id == rd_tracks_create(ctx, NULL, 0).id);
  rd_ctx_destroy(ctx);
}

// CSS Grid §8.5 step 2: items locked to a row take the first free column past the ones this step
// already put in that row, adding implicit columns as needed.
static void test_row_locked_placement(void) {
  rd_ctx*   ctx  = rd_ctx_create();
  rd_tracks cols = TRACKS(ctx, rd_track_of(rd_px(50)), rd_track_of(rd_px(50)));
  rd_style  gs   = {.display               = RD_DISPLAY_GRID,
                    .grid_template_columns = cols,
                    .grid_auto_columns     = rd_track_of(rd_px(50)),
                    .grid_auto_rows        = rd_track_of(rd_px(20)),
                    .justify_items         = RD_ALIGN_START,
                    .width                 = rd_px(300)};
  rd_node   g    = add(ctx, rd_ctx_root(ctx), gs);
  rd_node   it[3];
  for (int j = 0; j < 3; j++) it[j] = add(ctx, g, (rd_style){.grid_row_start = rd_line(1), .width = rd_px(10)});
  gs.justify_items = RD_ALIGN_STRETCH;
  rd_node h        = add(ctx, rd_ctx_root(ctx), gs);
  add(ctx, h, (rd_style){.grid_column_start = rd_line(2), .grid_row_start = rd_line(1)});
  rd_node x = add(ctx, h, (rd_style){.grid_row_start = rd_line(1), .grid_column_start = rd_span(2)});
  rd_node y = add(ctx, h, (rd_style){.grid_row_start = rd_line(1)});
  rd_node z = add(ctx, h, (rd_style){0});
  rd_ctx_layout(ctx, 400, 300);
  CHECK_RECT(ctx, it[0], 0, 0, 10, 20);
  CHECK_RECT(ctx, it[1], 50, 0, 10, 20);
  CHECK_RECT(ctx, it[2], 100, 0, 10, 20);
  CHECK_RECT(ctx, x, 100, 0, 100, 20);
  CHECK_RECT(ctx, y, 200, 0, 50, 20);  // past x, not back in the free first column
  CHECK_RECT(ctx, z, 0, 0, 50, 20);    // auto-placed items still fill from the start
  rd_ctx_destroy(ctx);
}

static int counted_calls;

static rd_size measure_counted(void* data, float w, rd_measure_mode wm, float h, rd_measure_mode hm) {
  counted_calls++;
  return measure_words(data, w, wm, h, hm);
}

// A grid item that isn't stretched is measured under several keys per layout; they all stay cached.
static void test_unstretched_items_cached(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node g   = add(ctx, rd_ctx_root(ctx),
                    (rd_style){.display               = RD_DISPLAY_GRID,
                               .grid_template_columns = TRACKS(ctx, rd_track_of(rd_fr(1)), rd_track_of(rd_auto())),
                               .justify_items         = RD_ALIGN_START,
                               .align_items           = RD_ALIGN_START});
  words   w[16];
  rd_node leaf[16];
  for (int j = 0; j < 16; j++) {
    w[j]    = (words){3 + j, 10, 10};
    leaf[j] = add(ctx, g, (rd_style){0});
    rd_node_set_measure(ctx, leaf[j], j ? measure_counted : measure_words, &w[j], NULL);
  }
  rd_ctx_layout(ctx, 400, 3000);
  counted_calls = 0;
  w[0].count    = 4;  // one item changes, as a text edit would
  rd_node_mark_layout_dirty(ctx, leaf[0]);
  rd_ctx_layout(ctx, 400, 3000);
  CHECK(counted_calls == 0);
  rd_ctx_destroy(ctx);
}

int main(void) {
  interning();
  auto_repeat();
  test_fixed_and_fr();
  test_placement();
  test_auto_track_content();
  test_fr_min_content_floor();
  test_row_height_from_wrapped_text();
  test_alignment();
  test_column_flow_and_auto_rows();
  test_percent_tracks_and_span();
  test_grid_in_flex_row();
  test_row_locked_placement();
  test_unstretched_items_cached();
  return TEST_RESULT();
}
