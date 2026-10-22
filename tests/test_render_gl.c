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

  // The box is at (20, 20), 100x60: fill inside, border at its edge, background in its rounded corner.
  Image img   = frame(ctx);
  float scale = (float)img.width / (float)GetScreenWidth();
  CHECK(close_to(at(img, 70, 50, scale), 0xc8, 0x32, 0x32));  // fill
  CHECK(close_to(at(img, 70, 22, scale), 0x32, 0xc8, 0x32));  // top border
  CHECK(close_to(at(img, 21, 21, scale), 0x10, 0x20, 0x30));  // outside the rounded corner
  CHECK(close_to(at(img, 5, 5, scale), 0x10, 0x20, 0x30));    // background
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
