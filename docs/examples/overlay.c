// Runs headless as a test for the "Overlay and anchoring" chapter.
#include <assert.h>
#include <raydom/raydom.h>

int main(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node root = rd_ctx_root(ctx);
  rd_node button = rd_node_create_button(ctx, NULL, 0);
  rd_node_set_style(ctx, button, &(rd_style){.width = rd_px(100), .height = rd_px(30), .margin.left = rd_px(40)});
  rd_node_append(ctx, root, button);

  // ANCHOR: popup
  rd_node menu = rd_node_create(ctx);
  rd_node_set_style(ctx, menu, &(rd_style){.position = RD_POSITION_ABSOLUTE, .width = rd_px(160), .height = rd_px(90)});
  rd_node_append(ctx, rd_ctx_overlay(ctx), menu);
  rd_node_set_anchor(ctx, menu, &(rd_anchor){.node = button, .offset = 4});  // below the button
  // ANCHOR_END: popup

  rd_ctx_layout(ctx, 800, 600);
  rd_rect b = rd_node_bounds(ctx, menu);
  assert(b.x == 40 && b.y == 34);

  // ANCHOR: options
  rd_node_set_anchor(ctx, menu, &(rd_anchor){.node = button, .side = RD_SIDE_RIGHT, .align = RD_ALIGN_CENTER});
  rd_node_set_anchor(ctx, menu, &(rd_anchor){.x = 300, .y = 200});  // at a point: a context menu
  rd_node_set_anchor(ctx, menu, &(rd_anchor){.node = button, .match_width = true});  // a dropdown
  // ANCHOR_END: options
  rd_ctx_layout(ctx, 800, 600);
  assert(rd_node_bounds(ctx, menu).width == 100);

  // ANCHOR: flip
  rd_node_merge_style(ctx, button, &(rd_style){.margin.top = rd_px(560)});
  rd_ctx_layout(ctx, 800, 600);
  rd_side side = rd_node_anchor_side(ctx, menu);  // RD_SIDE_TOP: no room below, so it flipped
  // ANCHOR_END: flip
  assert(side == RD_SIDE_TOP && rd_node_bounds(ctx, menu).y == 560 - 90);

  // ANCHOR: dismiss
  rd_node_set_style(ctx, menu, &(rd_style){.position = RD_POSITION_ABSOLUTE, .display = RD_DISPLAY_NONE});  // close
  rd_node_destroy(ctx, menu);                                                                           // or remove
  // ANCHOR_END: dismiss
  rd_ctx_destroy(ctx);
  return 0;
}
