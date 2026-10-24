// Draws through raylib and reads the pixels back: the one path the headless tests can't reach. Needs a
// display, so it's built only with RAYDOM_TEST_GRAPHICS (CI runs it, under xvfb on Linux).
#include <raydom/raydom.h>
#include <raylib.h>
#include <rlgl.h>
#include <stdlib.h>
#include <string.h>

#include "../src/internal.h"  // an SVG's raster scale
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

// A painter that replaces or destroys its own node's painter, then reads its data: still valid, as
// the destroy runs after the callback returns (a C++ lambda reading its captures). ASan catches it.
typedef struct painter {
  int  value, *sum;
  bool destroy_node;
} painter;
static int  painters_freed;
static void painter_free(void* data) {
  painters_freed++;
  free(data);
}
static void paint_nothing(rd_ctx* ctx, const rd_paint_info* info, void* data) {
  (void)ctx;
  (void)info;
  (void)data;
}
static void paint_self_replacing(rd_ctx* ctx, const rd_paint_info* info, void* data) {
  painter* p = data;
  if (p->destroy_node)
    rd_node_destroy(ctx, info->node);
  else
    rd_node_set_paint(ctx, info->node, paint_nothing, NULL, NULL);
  *p->sum += p->value;
}

// A shader active in the callback stays active across rd_paint_text, which draws through it.
static Shader blue;
static void   paint_shaded(rd_ctx* ctx, const rd_paint_info* info, void* data) {
  (void)data;
  BeginShaderMode(blue);
  rd_paint_text(ctx, info, (rd_font){0}, 12, info->content.x, info->content.y + 12, "a", RD_TEXT_NUL, rd_rgb(255, 0, 0));
  DrawRectangle((int)info->content.x + 30, (int)info->content.y, 30, 30, RED);
  EndShaderMode();
}

static Shader solid_blue(void) {
  int v = rlGetVersion();
  return LoadShaderFromMemory(NULL, v == RL_OPENGL_ES_20 ? "#version 100\nprecision mediump float;\n"
                                                           "void main() { gl_FragColor = vec4(0.0, 0.0, 1.0, 1.0); }\n"
                                    : v == RL_OPENGL_21  ? "#version 120\nvoid main() { gl_FragColor = vec4(0.0, 0.0, 1.0, 1.0); }\n"
                                                         : "#version 330\nout vec4 finalColor;\n"
                                                           "void main() { finalColor = vec4(0.0, 0.0, 1.0, 1.0); }\n");
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

// The app's blend mode and its custom factors survive rd_draw: additive before it stays additive after,
// and factors set for BLEND_CUSTOM_SEPARATE still apply.
static void test_blend_kept(rd_ctx* ctx) {
  rd_update(ctx);
  BeginDrawing();
  ClearBackground(BLACK);
  rlSetBlendFactorsSeparate(RL_ONE, RL_ONE, RL_ONE, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);
  BeginBlendMode(BLEND_ADDITIVE);
  rd_draw(ctx);
  DrawRectangle(2, 2, 6, 6, (Color){100, 0, 0, 255});
  BeginBlendMode(BLEND_CUSTOM_SEPARATE);
  DrawRectangle(10, 2, 6, 6, (Color){0, 100, 0, 255});
  EndBlendMode();
  rlDrawRenderBatchActive();
  Image img   = LoadImageFromScreen();
  float scale = (float)img.width / (float)GetScreenWidth();
  EndDrawing();
  CHECK(close_to(at(img, 5, 5, scale), 0x10 + 100, 0x20, 0x30));   // added to the background
  CHECK(close_to(at(img, 13, 5, scale), 0x10, 0x20 + 100, 0x30));  // with the app's factors
  UnloadImage(img);
}

// A context drawn from another's paint callback, at the outer's pixel ratio of 2, clips where it draws.
static rd_ctx* inner;
static void    paint_inner(rd_ctx* ctx, const rd_paint_info* info, void* data) {
  (void)ctx;
  (void)info;
  (void)data;
  rd_draw(inner);
}

static void test_nested(void) {
  rd_ctx* outer = rd_ctx_create();
  rd_node_set_style(outer, rd_ctx_root(outer), &(rd_style){.background = rd_hex(0x000000)});
  rd_node_set_paint(outer, rd_ctx_root(outer), paint_inner, NULL, NULL);
  rd_set_pixel_ratio(outer, 2);
  inner     = rd_ctx_create();
  rd_node r = rd_ctx_root(inner);
  rd_node_set_style(inner, r, &(rd_style){.align_items = RD_ALIGN_START, .padding = rd_edges_all(rd_px(10))});
  rd_node view = rd_node_create(inner), tall = rd_node_create(inner);
  rd_node_set_style(inner, view, &(rd_style){.width = rd_px(60), .height = rd_px(40), .overflow_y = RD_OVERFLOW_HIDDEN});
  rd_node_set_style(inner, tall, &(rd_style){.width = rd_px(60), .height = rd_px(100), .background = rd_hex(0xff0000), .flex_shrink = rd_num(0)});
  rd_node_append(inner, view, tall);
  rd_node_append(inner, r, view);
  rd_ctx_layout(inner, 150, 110);
  Image img   = frame(outer);
  float scale = (float)img.width / (float)GetScreenWidth();
  // The view is at (10, 10) to (70, 50) in the inner context, twice that on screen.
  CHECK(close_to(at(img, 130, 90, scale), 255, 0, 0));
  CHECK(close_to(at(img, 100, 110, scale), 0, 0, 0));
  UnloadImage(img);
  rd_ctx_destroy(inner);
  rd_ctx_destroy(outer);
}

// Drawn under the app's transform at a ratio of 1: clips follow it, and rd_paint_restore brings it
// back after a render-texture pass.
static RenderTexture2D pass;
static void            paint_after_pass(rd_ctx* ctx, const rd_paint_info* info, void* data) {
  (void)ctx;
  (void)data;
  BeginTextureMode(pass);
  ClearBackground(BLUE);
  EndTextureMode();
  rd_paint_restore(info);
  DrawRectangle((int)info->content.x, (int)info->content.y, 20, 20, GREEN);
}

static void test_translated(void) {
  pass        = LoadRenderTexture(8, 8);
  rd_ctx* ctx = rd_ctx_create();
  rd_node r   = rd_ctx_root(ctx);
  rd_node_set_style(ctx, r, &(rd_style){.flex_direction = RD_FLEX_ROW, .align_items = RD_ALIGN_START, .padding = rd_edges_all(rd_px(10))});
  rd_node view = rd_node_create(ctx), tall = rd_node_create(ctx), canvas = rd_node_create(ctx);
  rd_node_set_style(ctx, view, &(rd_style){.width = rd_px(60), .height = rd_px(40), .overflow_y = RD_OVERFLOW_HIDDEN});
  rd_node_set_style(ctx, tall, &(rd_style){.width = rd_px(60), .height = rd_px(100), .background = rd_hex(0xff0000), .flex_shrink = rd_num(0)});
  rd_node_set_style(ctx, canvas, &(rd_style){.width = rd_px(20), .height = rd_px(20)});
  rd_node_set_paint(ctx, canvas, paint_after_pass, NULL, NULL);
  rd_node_append(ctx, view, tall);
  rd_node_append(ctx, r, view);
  rd_node_append(ctx, r, canvas);
  rd_ctx_layout(ctx, 150, 120);
  BeginDrawing();
  ClearBackground(BLACK);
  rlTranslatef(100, 50, 0);
  rd_draw(ctx);
  rlLoadIdentity();
  rlDrawRenderBatchActive();
  Image img   = LoadImageFromScreen();
  float scale = (float)img.width / (float)GetScreenWidth();
  EndDrawing();
  rd_rect cb = rd_node_bounds(ctx, canvas);
  CHECK(close_to(at(img, 100 + 15, 50 + 45, scale), 255, 0, 0));  // inside the view
  CHECK(close_to(at(img, 100 + 15, 50 + 55, scale), 0, 0, 0));    // clipped below it
  CHECK(close_to(at(img, 100 + cb.x + 10, 50 + cb.y + 10, scale), 0, 228, 48));
  UnloadImage(img);
  rd_ctx_destroy(ctx);
  UnloadRenderTexture(pass);
}

// An SVG drawn a little larger each frame rasterizes in steps, not every frame.
static void test_svg_steps(rd_ctx* ctx, rd_node root) {
  static const char svg[] =
      "<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10'>"
      "<rect width='10' height='10' fill='#00ff00'/></svg>";
  rd_image im = rd_image_load(ctx, svg, strlen(svg));
  CHECK(im.id);
  rd_node n = rd_node_create(ctx);
  rd_node_set_image(ctx, n, im);
  rd_node_append(ctx, root, n);
  float last    = 0;
  int   rasters = 0;
  for (int f = 0; f < 20; f++) {
    rd_node_set_style(ctx, n, &(rd_style){.position = RD_POSITION_ABSOLUTE, .width = rd_px(20 + (float)f), .height = rd_px(20 + (float)f)});
    UnloadImage(frame(ctx));
    float s = rd__image_get(ctx, im.id)->raster_scale;
    CHECK(s * 10 >= (20 + (float)f) * rd_pixel_ratio(ctx) - 0.01f);  // never magnified
    rasters += s != last;
    last = s;
  }
  CHECK(rasters <= 4);  // 20 to 39 px is about three steps of 1.25
  rd_node_destroy(ctx, n);
  rd_image_unload(ctx, im);
}

// raylib's high-DPI window: its modelview carries the display's scale, and BeginScissorMode applies it
// again. On a 2x display (macOS Retina), clips and text must still land where they're laid out; on a 1x
// display this is the plain case again.
static void test_high_dpi(void) {
  SetConfigFlags(FLAG_WINDOW_HIGHDPI);
  InitWindow(320, 240, "raydom gl test, high dpi");
  if (!IsWindowReady()) return;
  rd_ctx* ctx = rd_ctx_create();
  rd_font_load_file(ctx, RAYDOM_TEST_FONT);
  rd_node root = rd_ctx_root(ctx);
  rd_node_set_style(ctx, root, &(rd_style){.background = rd_hex(0x102030), .padding = rd_edges_all(rd_px(20)),
                                           .align_items = RD_ALIGN_START, .color = rd_hex(0xff0000)});
  // A 100x60 clip at (120, 20) holding a red box twice its width: red must stop at the clip's right edge.
  rd_node clip = rd_node_create(ctx), wide = rd_node_create(ctx);
  rd_node_set_style(ctx, clip, &(rd_style){.position = RD_POSITION_ABSOLUTE, .inset = {.left = rd_px(120), .top = rd_px(20)},
                                           .width = rd_px(100), .height = rd_px(60), .overflow_x = RD_OVERFLOW_HIDDEN,
                                           .overflow_y = RD_OVERFLOW_HIDDEN});
  rd_node_set_style(ctx, wide, &(rd_style){.width = rd_px(200), .height = rd_px(60), .flex_shrink = rd_num(0),
                                           .background = rd_hex(0xc83232)});
  rd_node_append(ctx, clip, wide);
  rd_node_append(ctx, root, clip);
  rd_node text = rd_node_create_text(ctx, "MMMM", RD_TEXT_NUL);  // red text at (20, 20)
  rd_node_set_style(ctx, text, &(rd_style){.font_size = rd_px(24)});
  rd_node_append(ctx, root, text);
  Image img   = frame(ctx);
  float scale = (float)img.width / (float)GetScreenWidth();
  CHECK(close_to(at(img, 170, 50, scale), 0xc8, 0x32, 0x32));  // inside the clip
  CHECK(close_to(at(img, 160, 90, scale), 0x10, 0x20, 0x30));  // below it
  CHECK(close_to(at(img, 260, 50, scale), 0x10, 0x20, 0x30));  // right of it: clipped
  rd_rect tb = rd_node_bounds(ctx, text);
  CHECK(reds(img, (rd_rect){tb.x, tb.y, 90, tb.height}, scale) > 50);  // the text draws where it's laid out
  UnloadImage(img);
  rd_ctx_destroy(ctx);
  CloseWindow();
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
  // A rounded bar snapped to no width draws nothing, not a plain bar.
  rd_node bar = rd_node_create(ctx);
  rd_node_set_style(ctx, bar, &(rd_style){.position = RD_POSITION_ABSOLUTE, .inset = {rd_px(150), rd_auto(), rd_auto(), rd_px(250.1f)}, .width = rd_px(0.2f), .height = rd_px(40), .background = rd_hex(0xff0000), .border_radius = rd_corners_all(rd_px(6)), .box_shadow = rd_shadow_of(0, 0, 0, rd_hex(0xff0000))});
  rd_node_append(ctx, root, bar);
  // Painters that replace or destroy themselves.
  int      sum     = 0;
  rd_node  swapper = rd_node_create(ctx), doomed = rd_node_create(ctx);
  painter *ps = malloc(sizeof *ps), *pd = malloc(sizeof *pd);
  *ps            = (painter){1, &sum, false};
  *pd            = (painter){10, &sum, true};
  rd_style small = {.position = RD_POSITION_ABSOLUTE, .inset = {rd_px(0), rd_auto(), rd_auto(), rd_px(0)}, .width = rd_px(4), .height = rd_px(4)};
  rd_node_set_style(ctx, swapper, &small);
  rd_node_set_style(ctx, doomed, &small);
  rd_node_set_paint(ctx, swapper, paint_self_replacing, ps, painter_free);
  rd_node_set_paint(ctx, doomed, paint_self_replacing, pd, painter_free);
  rd_node_append(ctx, root, swapper);
  rd_node_append(ctx, root, doomed);
  // A canvas whose callback draws under its own shader.
  blue          = solid_blue();
  rd_node shady = rd_node_create(ctx);
  rd_node_set_style(ctx, shady, &(rd_style){.width = rd_px(60), .height = rd_px(30)});
  rd_node_set_paint(ctx, shady, paint_shaded, NULL, NULL);
  rd_node_append(ctx, root, shady);

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
  CHECK(reds(img, (rd_rect){245, 145, 12, 50}, scale) == 0);  // the empty bar
  CHECK(sum == 11 && painters_freed == 2 && !rd_node_alive(ctx, doomed));
  rd_rect sb = rd_node_bounds(ctx, shady);
  CHECK(close_to(at(img, sb.x + 45, sb.y + 15, scale), 0, 0, 255));  // the rectangle, through the shader
  UnloadImage(img);
  UnloadShader(blue);
  rd_node_set_paint(ctx, shady, NULL, NULL, NULL);

  // At a pixel ratio of 2, everything is twice the size on screen.
  rd_set_pixel_ratio(ctx, 2);
  img = frame(ctx);
  CHECK(close_to(at(img, 140, 100, scale), 0xc8, 0x32, 0x32));   // the fill, at twice (70, 50)
  CHECK(close_to(at(img, 140, 44, scale), 0x32, 0xc8, 0x32));    // the border, at twice (70, 22)
  UnloadImage(img);
  test_svg_steps(ctx, root);
  rd_set_pixel_ratio(ctx, 1);
  test_svg_steps(ctx, root);
  test_blend_kept(ctx);

  rd_ctx_destroy(ctx);
  test_nested();
  test_translated();
  CloseWindow();
  test_high_dpi();
  return TEST_RESULT();
}
