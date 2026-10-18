#include <math.h>
#include <raydom/raydom.h>
#include <raylib.h>
#include <stdlib.h>

#include "check.h"
#include "internal.h"
#include "render_internal.h"

static bool near(float a, float b) { return fabsf(a - b) < 0.01f; }

static rd_node add(rd_ctx* ctx, rd_node parent, rd_style style) {
  rd_node n = rd_node_create(ctx);
  rd_node_set_style(ctx, n, &style);
  rd_node_append(ctx, parent, n);
  return n;
}

static const rd__cmd* image_cmd(rd_ctx* ctx) {
  uint32_t       n;
  const rd__cmd* cmds = rd__display_list(ctx, &n);
  for (uint32_t j = 0; j < n; j++)
    if (cmds[j].kind == RD_CMD_IMAGE) return &cmds[j];
  return NULL;
}

static bool rect_is(rd_rect r, float x, float y, float w, float h) {
  return near(r.x, x) && near(r.y, y) && near(r.width, w) && near(r.height, h);
}

static void loading(void) {
  rd_ctx* ctx = rd_ctx_create();
  // A 40x20 PNG, encoded by raylib.
  Image          src = GenImageColor(40, 20, RED);
  int            size = 0;
  unsigned char* png  = ExportImageToMemory(src, ".png", &size);
  UnloadImage(src);
  rd_image img = rd_image_load(ctx, png, (size_t)size);
  MemFree(png);
  CHECK(img.id && near(rd_image_size(ctx, img).width, 40) && near(rd_image_size(ctx, img).height, 20));

  unsigned char garbage[16] = {1, 2, 3};
  CHECK(rd_image_load(ctx, garbage, sizeof garbage).id == 0);
  CHECK(rd_image_load_file(ctx, "/nonexistent.png").id == 0);
  CHECK(rd_image_from_pixels(ctx, NULL, 4, 4).id == 0 && rd_image_from_pixels(ctx, garbage, 0, 4).id == 0);

  rd_image_unload(ctx, img);
  rd_image_unload(ctx, img);
  CHECK(near(rd_image_size(ctx, img).width, 0));
  rd_ctx_destroy(ctx);
}

static void sizing(void) {
  rd_ctx*       ctx = rd_ctx_create();
  unsigned char px[200 * 100 * 4] = {0};
  rd_image      img = rd_image_from_pixels(ctx, px, 200, 100);
  rd_node       row = add(ctx, rd_ctx_root(ctx), (rd_style){.flex_direction = RD_FLEX_ROW, .align_items = RD_ALIGN_START});

  rd_node natural = add(ctx, row, (rd_style){0});
  rd_node wide    = add(ctx, row, (rd_style){.width = rd_px(50)});
  rd_node tall    = add(ctx, row, (rd_style){.height = rd_px(50)});
  rd_node both    = add(ctx, row, (rd_style){.width = rd_px(30), .height = rd_px(30)});
  rd_node col     = add(ctx, rd_ctx_root(ctx), (rd_style){.width = rd_px(400)});
  rd_node stretch = add(ctx, col, (rd_style){0});  // column + stretch: full width, height from the ratio
  rd_node all[] = {natural, wide, tall, both, stretch};
  for (size_t j = 0; j < sizeof all / sizeof all[0]; j++) rd_node_set_image(ctx, all[j], img);
  rd_ctx_layout(ctx, 800, 600);

  CHECK(near(rd_node_rect(ctx, natural).width, 200) && near(rd_node_rect(ctx, natural).height, 100));
  CHECK(near(rd_node_rect(ctx, wide).height, 25));
  CHECK(near(rd_node_rect(ctx, tall).width, 100));
  CHECK(near(rd_node_rect(ctx, both).width, 30) && near(rd_node_rect(ctx, both).height, 30));
  CHECK(near(rd_node_rect(ctx, stretch).width, 400) && near(rd_node_rect(ctx, stretch).height, 200));
  CHECK(rd_node_image(ctx, natural).id == img.id);

  // A new size relayouts the nodes showing it.
  unsigned char px2[100 * 100 * 4] = {0};
  CHECK(rd_image_update(ctx, img, px2, 100, 100));
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_rect(ctx, natural).width, 100) && near(rd_node_rect(ctx, wide).height, 50));

  // Unloaded: the nodes collapse and draw nothing.
  rd_image_unload(ctx, img);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_rect(ctx, natural).width, 0) && !image_cmd(ctx));
  CHECK(!rd_image_update(ctx, img, px2, 100, 100));

  // Clearing, and text replacing an image.
  rd_image other = rd_image_from_pixels(ctx, px2, 100, 100);
  rd_node_set_image(ctx, natural, other);
  rd_node_set_image(ctx, natural, (rd_image){0});
  CHECK(rd_node_image(ctx, natural).id == 0);
  rd_node_set_image(ctx, wide, other);
  rd_node_set_text(ctx, wide, "text", RD_TEXT_NUL);
  CHECK(rd_node_image(ctx, wide).id == 0);
  rd_ctx_destroy(ctx);
}

static void fitting(void) {
  rd_ctx*       ctx = rd_ctx_create();
  unsigned char px[200 * 100 * 4] = {0};
  rd_image      img = rd_image_from_pixels(ctx, px, 200, 100);  // 2:1 into a 100x100 box
  rd_node       box = add(ctx, rd_ctx_root(ctx),
                          (rd_style){.position = RD_POSITION_ABSOLUTE, .width = rd_px(100), .height = rd_px(100)});
  rd_node_set_image(ctx, box, img);
  rd_ctx_layout(ctx, 800, 600);

  const rd__cmd* c = image_cmd(ctx);  // fill: stretched
  CHECK(c && rect_is(c->dest, 0, 0, 100, 100) && rect_is(c->src, 0, 0, 200, 100));

  rd_node_merge_style(ctx, box, &(rd_style){.object_fit = RD_OBJECT_FIT_CONTAIN});
  rd_ctx_layout(ctx, 800, 600);
  c = image_cmd(ctx);
  CHECK(c && rect_is(c->dest, 0, 25, 100, 50) && rect_is(c->src, 0, 0, 200, 100));

  rd_node_merge_style(ctx, box, &(rd_style){.object_fit = RD_OBJECT_FIT_COVER});
  rd_ctx_layout(ctx, 800, 600);
  c = image_cmd(ctx);
  CHECK(c && rect_is(c->dest, 0, 0, 100, 100) && rect_is(c->src, 50, 0, 100, 100));

  rd_node_merge_style(ctx, box, &(rd_style){.object_fit = RD_OBJECT_FIT_NONE});
  rd_ctx_layout(ctx, 800, 600);
  c = image_cmd(ctx);
  CHECK(c && rect_is(c->dest, 0, 0, 100, 100) && rect_is(c->src, 50, 0, 100, 100));  // natural size, center cropped

  rd_node_merge_style(ctx, box, &(rd_style){.object_fit = RD_OBJECT_FIT_SCALE_DOWN});
  rd_ctx_layout(ctx, 800, 600);
  c = image_cmd(ctx);
  CHECK(c && rect_is(c->dest, 0, 25, 100, 50));

  // Corners: a 50px radius less 4px of padding rounds the content box by 46px.
  rd_node_merge_style(ctx, box, &(rd_style){.padding = rd_edges_all(rd_px(4)), .border_radius = rd_corners_all(rd_px(50))});
  rd_ctx_layout(ctx, 800, 600);
  c = image_cmd(ctx);
  CHECK(c && rect_is(c->rect, 4, 4, 92, 92) && near(c->radii[0], 46) && near(c->radii[2], 46));
  rd_ctx_destroy(ctx);
}

static const char ICON[] =
    "<?xml version=\"1.0\"?>\n<!-- an icon -->\n"
    "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 12\" width=\"24\" height=\"12\">"
    "<rect x=\"0\" y=\"0\" width=\"24\" height=\"12\" fill=\"currentColor\"/></svg>";

static const char LOGO[] =
    "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 50\">"
    "<circle cx=\"25\" cy=\"25\" r=\"20\" fill=\"#ff0000\"/></svg>";

static void svg(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_image icon = rd_image_load(ctx, ICON, sizeof ICON - 1);
  rd_image logo = rd_image_load(ctx, LOGO, sizeof LOGO - 1);
  CHECK(icon.id && near(rd_image_size(ctx, icon).width, 24) && near(rd_image_size(ctx, icon).height, 12));
  CHECK(logo.id && near(rd_image_size(ctx, logo).width, 100) && near(rd_image_size(ctx, logo).height, 50));  // from viewBox
  CHECK(rd_image_load(ctx, "<svg></svg>", 11).id == 0);  // no size
  CHECK(rd_image_load(ctx, "<html><body></body></html>", 26).id == 0);

  // Lays out from its natural size, like any image; currentColor icons draw tinted by the node's color.
  rd_node n = add(ctx, rd_ctx_root(ctx), (rd_style){.position = RD_POSITION_ABSOLUTE, .height = rd_px(24),
                                                    .color = rd_hex(0x00ff00)});
  rd_node_set_image(ctx, n, icon);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_rect(ctx, n).width, 48));
  const rd__cmd* c = image_cmd(ctx);
  CHECK(c && c->fill.g == 255 && c->fill.r == 0 && rect_is(c->src, 0, 0, 24, 12));
  rd_node_set_image(ctx, n, logo);
  rd_ctx_layout(ctx, 800, 600);
  c = image_cmd(ctx);
  CHECK(c && c->fill.r == 255 && c->fill.g == 255);  // no currentColor: untinted

  // Rasterizing: the sentinel became white; explicit colors are kept; scale sets the raster size.
  rd__image* im = rd__image_get(ctx, icon.id);
  CHECK(im->svg && im->current_color && !im->pixels && rd__image_rasterize(im, 2));
  CHECK(im->w == 48 && im->h == 24 && near(im->raster_scale, 2));
  unsigned char* p = &im->pixels[(12 * 48 + 24) * 4];
  CHECK(p[0] == 255 && p[1] == 255 && p[2] == 255 && p[3] == 255);
  im = rd__image_get(ctx, logo.id);
  CHECK(rd__image_rasterize(im, 1) && !im->current_color);
  p = &im->pixels[(25 * 100 + 25) * 4];
  CHECK(p[0] == 255 && p[1] == 0 && p[3] == 255);
  CHECK(rd__image_rasterize(im, 1000) && im->w == RD__MAX_RASTER);  // capped

  // Pixels replace an SVG.
  unsigned char px[4 * 4 * 4] = {0};
  CHECK(rd_image_update(ctx, icon, px, 4, 4) && !rd__image_get(ctx, icon.id)->svg);
  CHECK(near(rd_image_size(ctx, icon).width, 4));
  rd_ctx_destroy(ctx);
}

int main(void) {
  loading();
  svg();
  sizing();
  fitting();
  return TEST_RESULT();
}
