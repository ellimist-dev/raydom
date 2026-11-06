#include <math.h>
#include <raydom/raydom.h>
#include <stdio.h>

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
  CHECK(count_kind(cmds, n, RD_CMD_RECT) == 4);  // three visible rows and the scrollbar thumb
  const rd__cmd* c = &cmds[0];
  CHECK(c->kind == RD_CMD_CLIP && near(c->rect.x, 5) && near(c->rect.y, 5) && near(c->rect.width, 90) &&
        near(c->rect.height, 90));
  CHECK(cmds[n - 2].kind == RD_CMD_CLIP && near(cmds[n - 2].rect.width, 400));
  // The thumb: inside the padding box, 2px from the edge, 8px wide, at least 24px long.
  const rd__cmd* thumb = &cmds[n - 1];
  CHECK(thumb->kind == RD_CMD_RECT && near(thumb->rect.x, 85) && near(thumb->rect.y, 7) && near(thumb->rect.width, 8) &&
        near(thumb->rect.height, 24) && near(thumb->radii[0], 4));

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

static void test_pixel_ratio(void) {
  rd_ctx* ctx = rd_ctx_create();
  CHECK(rd_pixel_ratio(ctx) == 1);
  rd_set_pixel_ratio(ctx, 2);
  CHECK(rd_pixel_ratio(ctx) == 2);
  rd_set_pixel_ratio(ctx, 0);  // ignored, as are negatives and NaN
  rd_set_pixel_ratio(ctx, -1);
  rd_set_pixel_ratio(ctx, NAN);
  CHECK(rd_pixel_ratio(ctx) == 2);
  CHECK(rd_pixel_ratio(NULL) == 1);
  rd_paint_restore(&(rd_paint_info){.ratio = 1});  // no-op outside rd_draw
  rd_ctx_destroy(ctx);
}

// A long text in a scroller: its command carries the clip, and a selection over all of it fills only
// the lines in view.
static void test_text_cull(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_font_load_file(ctx, RAYDOM_TEST_FONT);
  rd_node     view = add(ctx, rd_ctx_root(ctx),
                         (rd_style){.width = rd_px(200), .height = rd_px(100), .overflow_y = RD_OVERFLOW_SCROLL, .font_size = rd_px(10)});
  static char text[2000 * 6];
  size_t      o = 0;
  for (int j = 0; j < 2000; j++) o += (size_t)snprintf(text + o, sizeof text - o, "l%04d ", j);  // wraps: 300+ lines
  rd_node t = rd_node_create_text(ctx, text, o);
  rd_node_set_style(ctx, t, &(rd_style){.flex_shrink = rd_num(0)});
  rd_node_append(ctx, view, t);
  rd_ctx_layout(ctx, 400, 300);
  rd_node_set_scroll(ctx, view, 0, 2000);
  CHECK(rd_ctx_select(ctx, t, 0, t, o));

  uint32_t       n;
  const rd__cmd* cmds = rd__display_list(ctx, &n);
  const rd__cmd* c    = find(cmds, n, RD_CMD_TEXT, t);
  CHECK(c && near(c->clip.y, 0) && near(c->clip.height, 100) && near(c->rect.y, -2000));
  uint32_t fills = count_kind(cmds, n, RD_CMD_FILL);
  float    lh    = 100.0f / 8;  // generous: lines are at least this tall at 10px
  CHECK(fills > 0 && fills <= (uint32_t)(100 / lh) + 2);
  for (uint32_t j = 0; j < n; j++)  // each fill meets the view
    if (cmds[j].kind == RD_CMD_FILL) CHECK(cmds[j].rect.y < 100 && cmds[j].rect.y + cmds[j].rect.height > 0);
  rd_ctx_destroy(ctx);
}


// A column of 100px rows in a 100px tall scroller at the top left of the root.
static rd_node scroller_of(rd_ctx* ctx, rd_node parent, rd_node rows[5]) {
  rd_node s = add(ctx, parent, (rd_style){.width = rd_px(200), .height = rd_px(100), .overflow_y = RD_OVERFLOW_SCROLL, .flex_shrink = rd_num(0)});
  for (int j = 0; j < 5; j++) rows[j] = add(ctx, s, (rd_style){.height = rd_px(100), .flex_shrink = rd_num(0)});
  return s;
}

static void test_scroll_into_view(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node rows[5];
  rd_node s = scroller_of(ctx, rd_ctx_root(ctx), rows);
  rd_ctx_layout(ctx, 800, 600);

  // Laid out: it happens now, by the least distance, in both directions.
  CHECK(rd_node_scroll_into_view(ctx, rows[2], 0));
  CHECK(near(rd_node_scroll(ctx, s).y, 200));
  CHECK(rd_node_scroll_into_view(ctx, rows[2], 0));
  CHECK(near(rd_node_scroll(ctx, s).y, 200));  // already showing
  rd_node_set_scroll(ctx, s, 0, 150);
  rd_node_scroll_into_view(ctx, rows[0], 0);
  CHECK(near(rd_node_scroll(ctx, s).y, 0));

  // The margin shows beyond the node, as far as the view has room: none for a row as tall as the view.
  rd_node small = add(ctx, rows[3], (rd_style){.height = rd_px(20), .margin.top = rd_px(40)});  // 340 to 360
  rd_ctx_layout(ctx, 800, 600);
  rd_node_scroll_into_view(ctx, small, 10);
  CHECK(near(rd_node_scroll(ctx, s).y, 370 - 100));
  rd_node_scroll_into_view(ctx, small, 10);
  CHECK(near(rd_node_scroll(ctx, s).y, 270));
  rd_node_set_scroll(ctx, s, 0, 345);
  rd_node_scroll_into_view(ctx, small, 10);
  CHECK(near(rd_node_scroll(ctx, s).y, 330));
  rd_node_set_scroll(ctx, s, 0, 0);
  rd_node_scroll_into_view(ctx, small, 1000);  // shrinks to center it at most
  CHECK(near(rd_node_scroll(ctx, s).y, 360 - 60));
  rd_node_scroll_into_view(ctx, rows[4], 50);
  CHECK(near(rd_node_scroll(ctx, s).y, 400));
  rd_node_set_scroll(ctx, s, 0, 0);
  rd_node_scroll_into_view(ctx, small, -5);  // a negative margin is none
  CHECK(near(rd_node_scroll(ctx, s).y, 260));

  // A node not laid out yet waits for the layout; the scroll range then includes it.
  rd_node late = add(ctx, s, (rd_style){.height = rd_px(100), .flex_shrink = rd_num(0)});
  rd_node_set_scroll(ctx, s, 0, 0);
  CHECK(rd_node_scroll_into_view(ctx, late, 0));
  CHECK(near(rd_node_scroll(ctx, s).y, 0));
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_scroll(ctx, s).y, 500));
  rd_ctx_layout(ctx, 800, 600);  // once
  rd_node_set_scroll(ctx, s, 0, 0);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_scroll(ctx, s).y, 0));

  // Waiting requests skip nodes that went away or don't show by then, and a dead one fails at once.
  rd_node doomed = add(ctx, s, (rd_style){.height = rd_px(100), .flex_shrink = rd_num(0)});
  rd_node hidden = add(ctx, s, (rd_style){.height = rd_px(100), .flex_shrink = rd_num(0)});
  rd_node_scroll_into_view(ctx, doomed, 0);
  rd_node_scroll_into_view(ctx, hidden, 0);
  rd_node_destroy(ctx, doomed);
  rd_node_set_style(ctx, hidden, &(rd_style){.display = RD_DISPLAY_NONE});
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_scroll(ctx, s).y, 0));
  CHECK(!rd_node_scroll_into_view(ctx, doomed, 0));
  rd_node detached = rd_node_create(ctx);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(rd_node_scroll_into_view(ctx, detached, 0));  // its turn comes at the next layout: not in the tree, skipped
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_scroll(ctx, s).y, 0));
  rd_ctx_destroy(ctx);
}

static void test_scroll_into_view_containers(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node root = rd_ctx_root(ctx);
  // Nested: the inner container moves first, then the outer brings it into view.
  rd_node outer = add(ctx, root, (rd_style){.width = rd_px(300), .height = rd_px(150), .overflow_y = RD_OVERFLOW_SCROLL});
  add(ctx, outer, (rd_style){.height = rd_px(400), .flex_shrink = rd_num(0)});
  rd_node rows[5];
  rd_node inner = scroller_of(ctx, outer, rows);
  rd_ctx_layout(ctx, 800, 600);
  rd_node_scroll_into_view(ctx, rows[4], 0);
  CHECK(near(rd_node_scroll(ctx, inner).y, 400));
  CHECK(near(rd_node_scroll(ctx, outer).y, 500 - 150));
  rd_rect b = rd_node_bounds(ctx, rows[4]);
  CHECK(near(b.y, 50));

  // Only axes that scroll move: this container overflows sideways but hides it.
  rd_node wide = add(ctx, root, (rd_style){.position = RD_POSITION_ABSOLUTE, .inset = {.top = rd_px(300), .left = rd_px(0)},
                                           .width = rd_px(100), .height = rd_px(50), .flex_direction = RD_FLEX_ROW,
                                           .overflow_x = RD_OVERFLOW_HIDDEN, .overflow_y = RD_OVERFLOW_SCROLL});
  rd_node far  = add(ctx, wide, (rd_style){.width = rd_px(50), .height = rd_px(200), .margin.left = rd_px(300), .flex_shrink = rd_num(0)});
  rd_ctx_layout(ctx, 800, 600);
  rd_node_scroll_into_view(ctx, far, 0);
  CHECK(near(rd_node_scroll(ctx, wide).x, 0));

  // A node larger than the view shows its start, and stays put once it fills the view.
  rd_node_set_scroll(ctx, wide, 0, 0);
  rd_node tall   = add(ctx, root, (rd_style){.position = RD_POSITION_ABSOLUTE, .inset = {.top = rd_px(400), .left = rd_px(0)},
                                             .width = rd_px(100), .height = rd_px(50), .overflow_y = RD_OVERFLOW_SCROLL});
  rd_node before = add(ctx, tall, (rd_style){.height = rd_px(30), .flex_shrink = rd_num(0)});
  rd_node big    = add(ctx, tall, (rd_style){.height = rd_px(200), .flex_shrink = rd_num(0)});
  (void)before;
  rd_ctx_layout(ctx, 800, 600);
  rd_node_scroll_into_view(ctx, big, 0);
  CHECK(near(rd_node_scroll(ctx, tall).y, 30));
  rd_node_set_scroll(ctx, tall, 0, 100);
  rd_node_scroll_into_view(ctx, big, 0);
  CHECK(near(rd_node_scroll(ctx, tall).y, 100));

  // Hooks see the scrolled position; one that asks with the tree dirty gets it in the second pass. An anchor
  // in the scroller follows in the same layout.
  rd_node_set_scroll(ctx, tall, 0, 0);
  rd_node popup = add(ctx, rd_ctx_overlay(ctx), (rd_style){.position = RD_POSITION_ABSOLUTE, .width = rd_px(10), .height = rd_px(10)});
  rd_node_set_anchor(ctx, popup, &(rd_anchor){.node = big});
  rd_node_scroll_into_view(ctx, add(ctx, tall, (rd_style){.height = rd_px(10), .flex_shrink = rd_num(0)}), 0);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_scroll(ctx, tall).y, 240 - 50));
  CHECK(near(rd_node_bounds(ctx, popup).y, 400 + 230 - 190));
  rd_ctx_destroy(ctx);
}

static void test_background_commands(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node root = rd_ctx_root(ctx);
  uint8_t px[4 * 8 * 4] = {0};
  rd_image im = rd_image_from_pixels(ctx, px, 8, 4);  // 8 wide, 4 tall
  rd_node  n  = add(ctx, root, (rd_style){.width = rd_px(100), .height = rd_px(60), .background_image = rd_bg_image(im),
                                          .border_width = rd_edges_all(rd_px(2)), .padding = rd_edges_all(rd_px(5)),
                                          .border_radius = rd_corners_all(rd_px(10)), .opacity = rd_num(0.5f)});
  add(ctx, n, (rd_style){.height = rd_px(10)});
  rd_ctx_layout(ctx, 800, 600);

  uint32_t       count;
  const rd__cmd* cmds = rd__display_list(ctx, &count);
  const rd__cmd* c    = find(cmds, count, RD_CMD_TILES, n);
  CHECK(c != NULL);
  if (!c) return;
  CHECK(near(c->rect.x, 2) && near(c->rect.y, 2) && near(c->rect.width, 96) && near(c->rect.height, 56));  // the padding box
  CHECK(near(c->dest.x, 2) && near(c->dest.y, 2) && near(c->dest.width, 8) && near(c->dest.height, 4));  // natural size
  CHECK(c->repeat == RD_BACKGROUND_REPEAT);
  CHECK(near(c->radii[0], 8) && near(c->radii[3], 8));  // the border radius, less the border
  CHECK(c->fill.a == 128);

  // Sizes: px, pct of the padding box, auto keeping the aspect ratio.
  rd_node_merge_style(ctx, n, &(rd_style){.background_width = rd_px(16)});
  rd_ctx_layout(ctx, 800, 600);
  c = find(rd__display_list(ctx, &count), count, RD_CMD_TILES, n);
  CHECK(c && near(c->dest.width, 16) && near(c->dest.height, 8));
  rd_node_merge_style(ctx, n, &(rd_style){.background_width = rd_auto(), .background_height = rd_pct(50), .background_repeat = RD_BACKGROUND_REPEAT_X});
  rd_ctx_layout(ctx, 800, 600);
  c = find(rd__display_list(ctx, &count), count, RD_CMD_TILES, n);
  CHECK(c && near(c->dest.height, 28) && near(c->dest.width, 56) && c->repeat == RD_BACKGROUND_REPEAT_X);
  rd_node_merge_style(ctx, n, &(rd_style){.background_width = rd_px(0)});  // a tile of nothing
  rd_ctx_layout(ctx, 800, 600);
  CHECK(!find(rd__display_list(ctx, &count), count, RD_CMD_TILES, n));

  // Paint only, off-screen culled, and gone with the image or with none.
  rd_node_merge_style(ctx, n, &(rd_style){.background_width = rd_auto(), .background_height = rd_auto()});
  rd_ctx_layout(ctx, 800, 600);
  CHECK(find(rd__display_list(ctx, &count), count, RD_CMD_TILES, n));
  rd_node_merge_style(ctx, n, &(rd_style){.background_image = rd_bg_none()});
  rd_ctx_layout(ctx, 800, 600);
  CHECK(!find(rd__display_list(ctx, &count), count, RD_CMD_TILES, n));
  rd_node_merge_style(ctx, n, &(rd_style){.background_image = rd_bg_image(im)});
  rd_ctx_layout(ctx, 800, 600);
  rd_image_unload(ctx, im);
  CHECK(!find(rd__display_list(ctx, &count), count, RD_CMD_TILES, n));

  // Below the node's children and its own text, above its background color.
  im = rd_image_from_pixels(ctx, px, 8, 4);
  rd_node_merge_style(ctx, n, &(rd_style){.background_image = rd_bg_image(im), .background = rd_hex(0x101010)});
  rd_node child = add(ctx, n, (rd_style){.height = rd_px(10), .background = rd_hex(0x202020)});
  rd_ctx_layout(ctx, 800, 600);
  cmds = rd__display_list(ctx, &count);
  const rd__cmd* bg = find(cmds, count, RD_CMD_RECT, n), *tiles = find(cmds, count, RD_CMD_TILES, n), *kid = find(cmds, count, RD_CMD_RECT, child);
  CHECK(bg && tiles && kid && bg < tiles && tiles < kid);
  rd_ctx_destroy(ctx);
}

int main(void) {
  test_pixel_ratio();
  test_text_cull();
  test_display_list();
  test_clip_and_cull();
  test_hit_and_pointer();
  test_paint_commands();
  test_scroll_into_view();
  test_scroll_into_view_containers();
  test_background_commands();
  return TEST_RESULT();
}
