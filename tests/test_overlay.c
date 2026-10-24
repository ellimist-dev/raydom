#include <math.h>
#include <raydom/raydom.h>

#include "check.h"

static bool near(float a, float b) { return fabsf(a - b) < 0.01f; }

static rd_node add(rd_ctx* ctx, rd_node parent, rd_style style) {
  rd_node n = rd_node_create(ctx);
  rd_node_set_style(ctx, n, &style);
  rd_node_append(ctx, parent, n);
  return n;
}

static rd_node popup(rd_ctx* ctx, float w, float h) {
  return add(ctx, rd_ctx_overlay(ctx), (rd_style){.position = RD_POSITION_ABSOLUTE, .width = rd_px(w), .height = rd_px(h)});
}

static void layering(void) {
  rd_ctx* ctx     = rd_ctx_create();
  rd_node root    = rd_ctx_root(ctx);
  rd_node overlay = rd_ctx_overlay(ctx);
  CHECK(rd_node_eq(rd_ctx_overlay(ctx), overlay));
  rd_node page = add(ctx, root, (rd_style){.height = rd_px(600)});  // appended after the overlay exists
  CHECK(rd_node_eq(rd_node_last_child(ctx, root), overlay));
  rd_node_insert_before(ctx, root, add(ctx, page, (rd_style){0}), (rd_node){0});
  CHECK(rd_node_eq(rd_node_last_child(ctx, root), overlay));

  rd_node menu = popup(ctx, 100, 100);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_rect(ctx, overlay).width, 800) && near(rd_node_rect(ctx, overlay).height, 600));
  CHECK(rd_node_eq(rd_node_at(ctx, 50, 50), menu));    // above the page
  CHECK(rd_node_eq(rd_node_at(ctx, 300, 300), page));  // the overlay itself doesn't block

  // Its geometry can't be styled away.
  rd_node_set_style(ctx, overlay, &(rd_style){.position = RD_POSITION_RELATIVE, .width = rd_px(10), .margin.left = rd_px(50)});
  rd_ctx_layout(ctx, 800, 600);
  rd_rect o = rd_node_rect(ctx, overlay);
  CHECK(near(o.x, 0) && near(o.y, 0) && near(o.width, 800) && near(o.height, 600));
  CHECK(rd_node_eq(rd_node_at(ctx, 50, 50), menu));

  // Clipping ancestors don't clip overlay content.
  rd_node clip = add(ctx, page, (rd_style){.height = rd_px(10), .overflow_y = RD_OVERFLOW_HIDDEN});
  rd_node_set_anchor(ctx, menu, &(rd_anchor){.node = clip});
  rd_ctx_layout(ctx, 800, 600);
  CHECK(rd_node_eq(rd_node_at(ctx, 50, 50), menu));

  // Destroying the overlay destroys its content; the next call makes a new one.
  rd_node_destroy(ctx, overlay);
  CHECK(!rd_node_alive(ctx, menu));
  rd_node again = rd_ctx_overlay(ctx);
  CHECK(rd_node_alive(ctx, again) && !rd_node_eq(again, overlay));
  rd_ctx_layout(ctx, 800, 600);
  rd_ctx_destroy(ctx);
}

static void anchoring(void) {
  rd_ctx* ctx    = rd_ctx_create();
  rd_node root   = rd_ctx_root(ctx);
  rd_node anchor = add(ctx, root, (rd_style){.position = RD_POSITION_ABSOLUTE,
                                             .inset    = {.top = rd_px(100), .left = rd_px(200)},
                                             .width    = rd_px(80),
                                             .height   = rd_px(20)});
  rd_node menu   = popup(ctx, 120, 50);

  rd_node_set_anchor(ctx, menu, &(rd_anchor){.node = anchor, .offset = 4});
  rd_ctx_layout(ctx, 800, 600);
  rd_rect b = rd_node_bounds(ctx, menu);
  CHECK(near(b.x, 200) && near(b.y, 124) && rd_node_anchor_side(ctx, menu) == RD_SIDE_BOTTOM);

  rd_node_set_anchor(ctx, menu, &(rd_anchor){.node = anchor, .align = RD_ALIGN_END});
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_bounds(ctx, menu).x, 280 - 120));

  rd_node_set_anchor(ctx, menu, &(rd_anchor){.node = anchor, .side = RD_SIDE_RIGHT, .align = RD_ALIGN_CENTER});
  rd_ctx_layout(ctx, 800, 600);
  b = rd_node_bounds(ctx, menu);
  CHECK(near(b.x, 280) && near(b.y, 110 - 25));

  // Flip: no room below near the bottom edge.
  rd_node_merge_style(ctx, anchor, &(rd_style){.inset.top = rd_px(570)});
  rd_node_set_anchor(ctx, menu, &(rd_anchor){.node = anchor});
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_bounds(ctx, menu).y, 570 - 50) && rd_node_anchor_side(ctx, menu) == RD_SIDE_TOP);
  rd_node_set_anchor(ctx, menu, &(rd_anchor){.node = anchor, .no_flip = true, .no_shift = true});
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_bounds(ctx, menu).y, 590) && rd_node_anchor_side(ctx, menu) == RD_SIDE_BOTTOM);

  // Shift: kept inside the right edge.
  rd_node_merge_style(ctx, anchor, &(rd_style){.inset.top = rd_px(100), .inset.left = rd_px(760)});
  rd_node_set_anchor(ctx, menu, &(rd_anchor){.node = anchor});
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_bounds(ctx, menu).x, 800 - 120));

  // A point, for context menus.
  rd_node_set_anchor(ctx, menu, &(rd_anchor){.x = 30, .y = 40});
  rd_ctx_layout(ctx, 800, 600);
  b = rd_node_bounds(ctx, menu);
  CHECK(near(b.x, 30) && near(b.y, 40));

  // Match width: the node is laid out at the anchor's width.
  rd_node list = add(ctx, rd_ctx_overlay(ctx), (rd_style){.position = RD_POSITION_ABSOLUTE});
  add(ctx, list, (rd_style){.height = rd_px(30)});
  rd_node_set_anchor(ctx, list, &(rd_anchor){.node = anchor, .match_width = true});
  rd_ctx_layout(ctx, 800, 600);
  b = rd_node_bounds(ctx, list);
  CHECK(near(b.width, 80) && near(b.height, 30));
  rd_ctx_layout(ctx, 800, 600);  // stable across layouts
  CHECK(near(rd_node_bounds(ctx, list).width, 80));
  rd_node fixed = add(ctx, rd_ctx_overlay(ctx), (rd_style){.position = RD_POSITION_ABSOLUTE, .height = rd_px(90),
                                                           .max_width = rd_px(60)});
  rd_node_set_anchor(ctx, fixed, &(rd_anchor){.node = anchor, .match_width = true});
  rd_ctx_layout(ctx, 800, 600);
  b = rd_node_bounds(ctx, fixed);
  CHECK(near(b.width, 60) && near(b.height, 90));  // its own height kept, max_width respected

  // A submenu anchored to a node inside an anchored menu.
  rd_node item = add(ctx, menu, (rd_style){.height = rd_px(20)});
  rd_node sub  = popup(ctx, 60, 60);
  rd_node_set_anchor(ctx, sub, &(rd_anchor){.node = item, .side = RD_SIDE_RIGHT});
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_bounds(ctx, sub).x, 30 + 120) && near(rd_node_bounds(ctx, sub).y, 40));

  // Anchors inside a scrolled container follow the scroll.
  rd_node scroller = add(ctx, root, (rd_style){.position = RD_POSITION_ABSOLUTE, .inset = {.top = rd_px(300)},
                                               .width = rd_px(200), .height = rd_px(100), .overflow_y = RD_OVERFLOW_SCROLL});
  add(ctx, scroller, (rd_style){.height = rd_px(50)});
  rd_node row = add(ctx, scroller, (rd_style){.height = rd_px(20)});
  add(ctx, scroller, (rd_style){.height = rd_px(500)});
  rd_node_set_anchor(ctx, menu, &(rd_anchor){.node = row});
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_bounds(ctx, menu).y, 370));
  rd_node_set_scroll(ctx, scroller, 0, 30);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_bounds(ctx, menu).y, 340));

  // Clearing keeps the last position; a destroyed anchor leaves the node where it was.
  rd_node_destroy(ctx, row);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_bounds(ctx, menu).y, 340));
  rd_node_set_anchor(ctx, sub, NULL);
  rd_node_set_anchor(ctx, sub, &(rd_anchor){.x = 5, .y = 5});  // cleared and set again before a layout
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_bounds(ctx, sub).x, 5));
  rd_node_destroy(ctx, menu);
  rd_ctx_layout(ctx, 800, 600);
  rd_ctx_destroy(ctx);
}

typedef struct follow {
  rd_node list;
  int     calls;
} follow;

// Keeps a list scrolled to its end, the way a chat does.
static void stick(rd_ctx* ctx, void* data) {
  follow* f = data;
  f->calls++;
  rd_node_set_scroll(ctx, f->list, 0, 1e9f);
}

static void grow(rd_ctx* ctx, void* data) {
  follow* f = data;
  if (rd_node_child_count(ctx, f->list) < 12) {
    rd_node n = rd_node_create(ctx);
    rd_node_set_style(ctx, n, &(rd_style){.height = rd_px(10)});
    rd_node_append(ctx, f->list, n);
  }
}

typedef struct owned {
  rd_node node;
  int     runs, destroyed;
  bool    self_destruct;
} owned;

static void owned_hook(rd_ctx* ctx, void* data) {
  owned* o = data;
  o->runs++;
  if (o->self_destruct) rd_node_destroy(ctx, o->node);  // its data must survive until the hook returns
}

static void owned_free(void* data) { ((owned*)data)->destroyed++; }

static void node_hooks(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node view = add(ctx, rd_ctx_root(ctx), (rd_style){0});
  owned   o    = {view, 0, 0, false};
  CHECK(rd_node_on_layout(ctx, view, owned_hook, &o, owned_free));
  rd_ctx_layout(ctx, 800, 600);
  CHECK(o.runs == 1);

  rd_node_detach(ctx, view);  // detached: skipped
  rd_ctx_layout(ctx, 800, 600);
  CHECK(o.runs == 1);
  rd_node_append(ctx, rd_ctx_root(ctx), view);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(o.runs == 2);

  rd_node_destroy(ctx, view);  // removed with its node, and its data destroyed
  CHECK(o.destroyed == 1);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(o.runs == 2 && o.destroyed == 1);

  // A hook that destroys its own node: destroyed after the run, once.
  rd_node v2 = add(ctx, rd_ctx_root(ctx), (rd_style){0});
  owned   q  = {v2, 0, 0, true};
  rd_node_on_layout(ctx, v2, owned_hook, &q, owned_free);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(q.runs == 1 && q.destroyed == 1 && !rd_node_alive(ctx, v2));
  rd_ctx_layout(ctx, 800, 600);
  CHECK(q.runs == 1);

  // Still alive at context destruction: destroyed then.
  rd_node v3 = add(ctx, rd_ctx_root(ctx), (rd_style){0});
  owned   r  = {v3, 0, 0, false};
  rd_node_on_layout(ctx, v3, owned_hook, &r, owned_free);
  CHECK(!rd_node_on_layout(ctx, v2, owned_hook, &r, owned_free));  // dead node
  rd_ctx_destroy(ctx);
  CHECK(r.destroyed == 1);
}

static void hooks(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node list = add(ctx, rd_ctx_root(ctx), (rd_style){.height = rd_px(100), .overflow_y = RD_OVERFLOW_SCROLL});
  for (int j = 0; j < 30; j++) add(ctx, list, (rd_style){.height = rd_px(10)});
  follow f = {list, 0};
  CHECK(rd_ctx_on_layout(ctx, stick, &f, NULL));
  rd_ctx_layout(ctx, 800, 600);
  CHECK(f.calls == 1 && near(rd_node_scroll(ctx, list).y, 200));  // geometry from this layout
  add(ctx, list, (rd_style){.height = rd_px(10)});
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_scroll(ctx, list).y, 210));

  // A hook that changes the tree gets one more layout pass, not another hook call.
  rd_ctx* c2 = rd_ctx_create();
  rd_node l2 = add(c2, rd_ctx_root(c2), (rd_style){0});
  follow  g  = {l2, 0};
  rd_ctx_on_layout(c2, grow, &g, NULL);
  rd_ctx_layout(c2, 800, 600);
  CHECK(rd_node_child_count(c2, l2) == 1 && near(rd_node_rect(c2, l2).height, 10));
  rd_ctx_destroy(c2);
  rd_ctx_destroy(ctx);
}

// A submenu whose anchor was set before its menu's is still placed after the menu: tree order.
static void anchor_order(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node btn  = add(ctx, rd_ctx_root(ctx), (rd_style){.position = RD_POSITION_ABSOLUTE,
                                                       .inset    = {.top = rd_px(100), .left = rd_px(200)},
                                                       .width    = rd_px(80),
                                                       .height   = rd_px(20)});
  rd_node menu = popup(ctx, 100, 100);
  rd_node item = add(ctx, menu, (rd_style){.height = rd_px(20)});
  rd_node sub  = popup(ctx, 60, 60);
  rd_node_set_anchor(ctx, sub, &(rd_anchor){.node = item, .side = RD_SIDE_RIGHT});
  rd_node_set_anchor(ctx, menu, &(rd_anchor){.node = btn});
  rd_ctx_layout(ctx, 800, 600);
  rd_rect m = rd_node_bounds(ctx, menu), s = rd_node_bounds(ctx, sub);
  CHECK(near(m.x, 200) && near(m.y, 120) && near(s.x, 300) && near(s.y, 120));
  rd_node_merge_style(ctx, btn, &(rd_style){.inset = {.top = rd_px(300), .left = rd_px(400)}});
  rd_ctx_layout(ctx, 800, 600);
  m = rd_node_bounds(ctx, menu), s = rd_node_bounds(ctx, sub);
  CHECK(near(m.x, 400) && near(m.y, 320) && near(s.x, 500) && near(s.y, 320));
  rd_ctx_destroy(ctx);
}

int main(void) {
  layering();
  hooks();
  node_hooks();
  anchoring();
  anchor_order();
  return TEST_RESULT();
}
