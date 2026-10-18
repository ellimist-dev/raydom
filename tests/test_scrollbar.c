#include <math.h>
#include <raydom/raydom.h>

#include "check.h"
#include "internal.h"

static bool near(float a, float b) { return fabsf(a - b) < 0.01f; }

static rd_node add(rd_ctx* ctx, rd_node parent, rd_style style) {
  rd_node n = rd_node_create(ctx);
  rd_node_set_style(ctx, n, &style);
  rd_node_append(ctx, parent, n);
  return n;
}

static void count(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ctx;
  (void)ev;
  ++*(int*)data;
}

static void press(rd_ctx* ctx, float x, float y) { rd_ctx_pointer(ctx, x, y, RD_BUTTON_PRIMARY, 0, 0); }
static void release(rd_ctx* ctx, float x, float y) { rd_ctx_pointer(ctx, x, y, 0, 0, 0); }

static void vertical(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node list = add(ctx, rd_ctx_root(ctx), (rd_style){.width = rd_px(200), .height = rd_px(100), .overflow_y = RD_OVERFLOW_SCROLL});
  rd_node body = add(ctx, list, (rd_style){.height = rd_px(1000)});
  int     downs = 0, clicks = 0;
  rd_node_on(ctx, body, RD_EVENT_POINTER_DOWN, count, &downs, NULL, 0);
  rd_node_on(ctx, body, RD_EVENT_CLICK, count, &clicks, NULL, 0);
  rd_ctx_layout(ctx, 800, 600);

  // Thumb: 2px in from the right edge and top, 8px wide, max(24, 96 * 100 / 1000) = 24px long.
  rd_rect track, thumb;
  CHECK(rd__scrollbar(ctx, list.index, rd_node_bounds(ctx, list), 1, &track, &thumb));
  CHECK(near(thumb.x, 190) && near(thumb.y, 2) && near(thumb.width, 8) && near(thumb.height, 24));
  CHECK(!rd__scrollbar(ctx, list.index, rd_node_bounds(ctx, list), 0, &track, &thumb));  // x doesn't scroll

  // Hovering the bar: default cursor, not the content's.
  rd_ctx_pointer(ctx, 194, 10, 0, 0, 0);
  CHECK(rd_ctx_cursor(ctx) == RD_CURSOR_DEFAULT && rd_node_eq(ctx->sb_hot.node, list));

  // Dragging the thumb 36px of its 72px travel scrolls halfway; the content sees nothing.
  press(ctx, 194, 10);
  rd_ctx_pointer(ctx, 194, 46, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(ctx, 400, 46, RD_BUTTON_PRIMARY, 0, 0);  // keeps dragging off the bar
  CHECK(near(rd_node_scroll(ctx, list).y, 450));
  release(ctx, 400, 46);
  CHECK(downs == 0 && clicks == 0);

  // A track press pages by 90% of the view toward the pointer, once per press.
  rd_ctx_layout(ctx, 800, 600);
  press(ctx, 194, 95);
  press(ctx, 194, 95);  // held across frames
  release(ctx, 194, 95);
  CHECK(near(rd_node_scroll(ctx, list).y, 540) && downs == 0);
  press(ctx, 194, 4);
  release(ctx, 194, 4);
  CHECK(near(rd_node_scroll(ctx, list).y, 450));

  // Content clicks still work.
  press(ctx, 50, 50);
  release(ctx, 50, 50);
  CHECK(downs == 1 && clicks == 1 && rd_node_is_null(ctx->sb_hot.node));

  // No bar without overflow, or with zero width.
  rd_node_merge_style(ctx, list, &(rd_style){.scrollbar_width = rd_px(0)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK(!rd__scrollbar(ctx, list.index, rd_node_bounds(ctx, list), 1, &track, &thumb));
  press(ctx, 194, 50);
  release(ctx, 194, 50);
  CHECK(downs == 2);
  rd_node_merge_style(ctx, list, &(rd_style){.scrollbar_width = rd_px(8)});
  rd_node_merge_style(ctx, body, &(rd_style){.height = rd_px(50)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK(!rd__scrollbar(ctx, list.index, rd_node_bounds(ctx, list), 1, &track, &thumb));
  rd_ctx_destroy(ctx);
}

static void both(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node grid = add(ctx, rd_ctx_root(ctx),
                     (rd_style){.width = rd_px(200), .height = rd_px(100), .overflow_x = RD_OVERFLOW_SCROLL,
                                .overflow_y = RD_OVERFLOW_SCROLL, .scrollbar_width = rd_px(10)});
  add(ctx, grid, (rd_style){.width = rd_px(800), .height = rd_px(400), .flex_shrink = rd_num(0)});
  rd_ctx_layout(ctx, 800, 600);
  rd_rect r = rd_node_bounds(ctx, grid), track, thumb;
  // Each track stops short of the corner the other bar occupies.
  CHECK(rd__scrollbar(ctx, grid.index, r, 1, &track, &thumb) && near(track.height, 100 - 4 - 12));
  CHECK(rd__scrollbar(ctx, grid.index, r, 0, &track, &thumb) && near(track.width, 200 - 4 - 12) && near(track.y, 88));

  // Dragging the horizontal thumb to the end scrolls to the end.
  press(ctx, thumb.x + 2, thumb.y + 2);
  rd_ctx_pointer(ctx, thumb.x + 500, thumb.y + 2, RD_BUTTON_PRIMARY, 0, 0);
  release(ctx, thumb.x + 500, thumb.y + 2);
  CHECK(near(rd_node_scroll(ctx, grid).x, 600) && near(rd_node_scroll(ctx, grid).y, 0));
  rd_ctx_destroy(ctx);
}

int main(void) {
  vertical();
  both();
  return TEST_RESULT();
}
