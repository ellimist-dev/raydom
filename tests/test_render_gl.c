// Draws through raylib and reads the pixels back: the one path the headless tests can't reach. Needs a
// display, so it's built only with RAYDOM_TEST_GRAPHICS (CI runs it, under xvfb on Linux).
#include <raydom/raydom.h>
#include <raylib.h>
#include <rlgl.h>
#include <stdlib.h>

#include "check.h"

static Color at(Image img, float x, float y, float scale) {
  return GetImageColor(img, (int)(x * scale), (int)(y * scale));
}

static bool close_to(Color c, int r, int g, int b) {
  return abs(c.r - r) <= 8 && abs(c.g - g) <= 8 && abs(c.b - b) <= 8;
}

// Pixels in r that are mostly red: text drawn in red.
static int reds(Image img, rd_rect r, float scale) {
  int n = 0;
  for (float y = r.y; y < r.y + r.height; y += 1)
    for (float x = r.x; x < r.x + r.width; x += 1) {
      Color c = at(img, x, y, scale);
      n += c.r > 150 && c.g < 90 && c.b < 90;
    }
  return n;
}

// Text from a paint callback, in red, on the canvas's left.
static void paint_label(rd_ctx* ctx, const rd_paint_info* info, void* data) {
  (void)data;
  rd_font_metrics m = rd_font_get_metrics(ctx, (rd_font){0}, 32);
  rd_paint_text(ctx, info, (rd_font){0}, 32, info->content.x + 4, info->content.y + 4 + m.ascent, "MW", RD_TEXT_NUL,
                rd_rgb(255, 0, 0));
}

// One frame of the tree, captured before the buffers swap.
static Image frame(rd_ctx* ctx) {
  rd_update(ctx);
  BeginDrawing();
  ClearBackground(BLACK);
  rd_draw(ctx);
  rlDrawRenderBatchActive();
  Image img = LoadImageFromScreen();
  EndDrawing();
  return img;
}

int main(void) {
  SetTraceLogLevel(LOG_WARNING);
  InitWindow(320, 240, "raydom gl test");
  if (!IsWindowReady()) {  // no display or no OpenGL, as on GitHub's macOS runners: skipped, not failed
    rd_ctx* ctx = rd_ctx_create();
    rd_update(ctx);  // and raydom stays out of raylib's way rather than crashing
    rd_draw(ctx);
    rd_ctx_destroy(ctx);
    return 77;
  }
  rd_ctx* ctx = rd_ctx_create();
  rd_font_load_file(ctx, RAYDOM_TEST_FONT);
  rd_node root = rd_ctx_root(ctx);
  rd_node_set_style(ctx, root, &(rd_style){.background = rd_hex(0x102030), .padding = rd_edges_all(rd_px(20)),
                                           .align_items = RD_ALIGN_START, .color = rd_hex(0xffffff)});
  rd_node box = rd_node_create(ctx);
  rd_node_set_style(ctx, box, &(rd_style){.width = rd_px(100), .height = rd_px(60), .background = rd_hex(0xc83232),
                                          .border_radius = rd_corners_all(rd_px(20)), .border_width = rd_edges_all(rd_px(4)),
                                          .border_color = rd_hex(0x32c832)});
  rd_node_append(ctx, root, box);
  rd_node_append(ctx, root, rd_node_create_text(ctx, "raydom", RD_TEXT_NUL));
  // A canvas drawing text, with a child that raydom draws after the callback.
  rd_node canvas = rd_node_create(ctx), after = rd_node_create(ctx);
  rd_node_set_style(ctx, canvas, &(rd_style){.width = rd_px(120), .height = rd_px(48), .flex_direction = RD_FLEX_ROW,
                                             .justify_content = RD_ALIGN_END});
  rd_node_set_style(ctx, after, &(rd_style){.width = rd_px(20), .height = rd_px(48), .background = rd_hex(0x32c832)});
  rd_node_set_paint(ctx, canvas, paint_label, NULL, NULL);
  rd_node_append(ctx, canvas, after);
  rd_node_append(ctx, root, canvas);
  // Translucent over opaque: the framebuffer stays opaque, so a browser doesn't show the page through.
  rd_node veil = rd_node_create(ctx);
  rd_node_set_style(ctx, veil, &(rd_style){.position = RD_POSITION_ABSOLUTE, .inset = {rd_px(200), rd_px(20), rd_px(10), rd_px(220)},
                                           .background = rd_rgba(255, 255, 255, 128)});
  rd_node_append(ctx, root, veil);

  // The box is at (20, 20), 100x60: fill inside, border at its edge, background in its rounded corner.
  Image img   = frame(ctx);
  float scale = (float)img.width / (float)GetScreenWidth();
  CHECK(close_to(at(img, 70, 50, scale), 0xc8, 0x32, 0x32));  // fill
  CHECK(close_to(at(img, 70, 22, scale), 0x32, 0xc8, 0x32));  // top border
  CHECK(close_to(at(img, 21, 21, scale), 0x10, 0x20, 0x30));  // outside the rounded corner
  CHECK(close_to(at(img, 5, 5, scale), 0x10, 0x20, 0x30));    // background
  rd_rect cb = rd_node_bounds(ctx, canvas), ab = rd_node_bounds(ctx, after);
  CHECK(reds(img, (rd_rect){cb.x, cb.y, 90, cb.height}, scale) > 50);                           // the canvas text
  CHECK(close_to(at(img, ab.x + ab.width / 2, ab.y + ab.height / 2, scale), 0x32, 0xc8, 0x32));  // drawn after it
  CHECK(reds(img, (rd_rect){130, 0, 190, cb.y}, scale) == 0);  // nowhere else: right of the red box
  Color v = at(img, 260, 215, scale);
  // Half white over 0x10 is about 0x88. The alpha check only bites where the framebuffer keeps alpha (a
  // browser's canvas); macOS reads back 255 regardless.
  CHECK(v.a == 255 && v.r > 120 && v.r < 150);
  UnloadImage(img);

  // At a pixel ratio of 2, everything is twice the size on screen.
  rd_set_pixel_ratio(ctx, 2);
  img = frame(ctx);
  CHECK(close_to(at(img, 140, 100, scale), 0xc8, 0x32, 0x32));   // the fill, at twice (70, 50)
  CHECK(close_to(at(img, 140, 44, scale), 0x32, 0xc8, 0x32));    // the border, at twice (70, 22)
  UnloadImage(img);

  rd_ctx_destroy(ctx);
  CloseWindow();
  return TEST_RESULT();
}
