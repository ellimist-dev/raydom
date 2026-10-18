// Runs headless as a test for the "Layout" chapter.
#include <assert.h>
#include <raydom/raydom.h>

static rd_node add(rd_ctx* ctx, rd_node parent, rd_style style) {
  rd_node n = rd_node_create(ctx);
  rd_node_set_style(ctx, n, &style);
  rd_node_append(ctx, parent, n);
  return n;
}

int main(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node root = rd_ctx_root(ctx);

  // ANCHOR: flex
  // A toolbar: fixed-width side items and a flexible middle.
  rd_node bar = add(ctx, root,
                    (rd_style){.flex_direction = RD_FLEX_ROW,
                               .align_items    = RD_ALIGN_CENTER,
                               .column_gap     = rd_px(8),
                               .padding        = rd_edges_all(rd_px(8)),
                               .height         = rd_px(48)});
  rd_node left   = add(ctx, bar, (rd_style){.width = rd_px(80), .height = rd_px(32)});
  rd_node middle = add(ctx, bar, (rd_style){.flex_grow = rd_num(1), .flex_basis = rd_px(0), .height = rd_px(32)});
  rd_node right  = add(ctx, bar, (rd_style){.width = rd_px(80), .height = rd_px(32)});
  // ANCHOR_END: flex

  // ANCHOR: grid
  rd_track  cols[3] = {rd_track_of(rd_px(200)), rd_track_of(rd_fr(1)), rd_track_of(rd_fr(2))};
  rd_node   grid    = add(ctx, root,
                          (rd_style){.display               = RD_DISPLAY_GRID,
                                     .grid_template_columns = rd_tracks_create(ctx, cols, 3),
                                     .grid_auto_rows        = rd_track_of(rd_px(40)),
                                     .column_gap            = rd_px(10),
                                     .row_gap               = rd_px(10)});
  rd_node header = add(ctx, grid, (rd_style){.grid_column_start = rd_line(1), .grid_column_end = rd_line(-1)});
  for (int i = 0; i < 6; i++) add(ctx, grid, (rd_style){0});  // auto-placed, row by row
  // ANCHOR_END: grid

  // ANCHOR: run
  rd_ctx_layout(ctx, 800, 600);  // rd_update does this for you in a raylib app
  rd_rect m = rd_node_rect(ctx, middle);    // relative to the parent's border box
  rd_rect h = rd_node_bounds(ctx, header);  // relative to the root, after scrolling
  // ANCHOR_END: run
  assert(m.x == 96 && m.width == 800 - 16 - 160 - 16);
  assert(h.width == 800 && rd_node_rect(ctx, left).width == 80 && rd_node_rect(ctx, right).x == 712);

  // ANCHOR: absolute
  rd_node badge = add(ctx, bar,
                      (rd_style){.position = RD_POSITION_ABSOLUTE,
                                 .inset    = {.top = rd_px(-6), .right = rd_px(-6)},
                                 .width    = rd_px(12),
                                 .height   = rd_px(12)});
  // ANCHOR_END: absolute
  rd_ctx_layout(ctx, 800, 600);
  assert(rd_node_rect(ctx, badge).x == 800 - 12 + 6);

  // ANCHOR: scroll
  rd_node list = add(ctx, root, (rd_style){.height = rd_px(200), .overflow_y = RD_OVERFLOW_SCROLL});
  for (int i = 0; i < 50; i++) add(ctx, list, (rd_style){.height = rd_px(24)});
  rd_ctx_layout(ctx, 800, 600);
  rd_node_set_scroll(ctx, list, 0, 300);  // clamped to the content: at most 50 * 24 - 200
  // ANCHOR_END: scroll
  assert(rd_node_scroll(ctx, list).y == 300 && rd_node_content_size(ctx, list).height == 1200);

  rd_ctx_destroy(ctx);
  return 0;
}
