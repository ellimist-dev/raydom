#ifndef RAYDOM_RENDER_H
#define RAYDOM_RENDER_H

#include <raydom/layout.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rd_vec2 {
  float x, y;
} rd_vec2;

// Topmost node under a point, in root coordinates, after layout. Skips pointer_events: none.
rd_node rd_node_at(const rd_ctx* ctx, float x, float y);

// Scroll offset of an overflow container, clamped to its scrollable range.
void    rd_node_set_scroll(rd_ctx* ctx, rd_node node, float x, float y);
rd_vec2 rd_node_scroll(const rd_ctx* ctx, rd_node node);

typedef struct rd_paint_info {
  rd_node node;
  rd_rect content;  // the node's content box, in screen coordinates
  rd_rect clip;     // the visible part of the screen from enclosing overflow containers
  float   dpi;      // device pixels per logical pixel
  float   ratio;    // the pixel ratio (rd_set_pixel_ratio) the transform is scaled by
} rd_paint_info;

// Custom drawing with raylib, for canvases. Called from rd_draw after the node's background and
// before its children, with raylib's batch flushed and no scissor or shader active, and the
// transform scaled by the pixel ratio so that drawing is in logical coordinates. Blending is raydom's:
// colors blend as usual and alpha accumulates toward opaque, so translucent drawing doesn't make a web
// canvas see-through; raydom restores it after the callback. Restore any other raylib state you change.
// Render-texture passes are safe: size and draw into the texture in device pixels (dpi), and call
// rd_paint_restore after EndTextureMode. A callback may draw another context with rd_draw, which then
// clips and rasterizes for the transform it's drawn under.
typedef void (*rd_paint_fn)(rd_ctx* ctx, const rd_paint_info* info, void* data);

// Null fn clears it. From inside the node's own callback, replacing or clearing it, or destroying the
// node, defers the old destroy until the callback returns, so the callback may go on using its data.
bool rd_node_set_paint(rd_ctx* ctx, rd_node node, rd_paint_fn fn, void* data, rd_destroy_fn destroy);
// Clips raylib drawing to the content box within the clip. Ended by raydom after the callback.
void rd_paint_scissor(const rd_paint_info* info);
// Restores the transform the callback started with, after raylib calls that reset it: EndTextureMode,
// EndMode2D, EndMode3D. Needed at any pixel ratio when rd_draw ran under a transform of the app's.
void rd_paint_restore(const rd_paint_info* info);
// Draws a line of text from a paint callback, with raydom's fonts: rasterized for the display's
// density, like the rest of the UI. x is the left edge and baseline the baseline, in logical
// coordinates; rd_font_measure and rd_font_get_metrics give the width and the ascent for placing it.
// A null font ({0}) is the default font; len may be RD_TEXT_NUL. No wrapping. Draws on top of what
// the callback drew before it, with the shader the callback has active, and changes no raylib state.
void rd_paint_text(rd_ctx* ctx, const rd_paint_info* info, rd_font font, float size, float x, float baseline,
                   const char* utf8, size_t len, rd_color color);

// raylib integration. rd_update reads raylib's mouse state and lays the tree out at the screen
// size; rd_draw draws it between BeginDrawing and EndDrawing. Both do nothing without a raylib window
// (IsWindowReady), for instance when InitWindow failed. Destroy the context before
// CloseWindow so its textures and shader are released. rd_draw leaves GL's blending, and rlgl's blend
// mode and custom factors, as it found them, and raylib's default shader active: rlgl doesn't expose
// the active one to restore.
void rd_update(rd_ctx* ctx);
// Lets raydom manage raylib's event waiting: idle frames wait for input (EnableEventWaiting), except
// while a timer is pending, when frames keep running so it fires on time. Use instead of calling
// EnableEventWaiting yourself.
void rd_set_event_waiting(rd_ctx* ctx, bool enabled);
void rd_draw(rd_ctx* ctx);
// Logical pixels are `ratio` raylib screen pixels: rd_update divides the pointer and the screen size
// by it, and rd_draw scales its drawing and rasterizes text and shapes for it. For the web, where
// raylib renders at CSS resolution: size raylib's window (the canvas) in device pixels, display it
// at CSS size, and set the ratio to devicePixelRatio. Also usable as a zoom. Default 1. Pass logical
// sizes to rd_ctx_layout when calling it yourself.
void  rd_set_pixel_ratio(rd_ctx* ctx, float ratio);
float rd_pixel_ratio(const rd_ctx* ctx);

#ifdef __cplusplus
}
#endif

#endif
