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
} rd_paint_info;

// Custom drawing with raylib, for canvases. Called from rd_draw after the node's background and
// before its children, with raylib's batch flushed and no scissor or shader active. Restore any
// raylib state you change; render-texture passes are safe.
typedef void (*rd_paint_fn)(rd_ctx* ctx, const rd_paint_info* info, void* data);

// Null fn clears it.
bool rd_node_set_paint(rd_ctx* ctx, rd_node node, rd_paint_fn fn, void* data, rd_destroy_fn destroy);
// Clips raylib drawing to the content box within the clip. Ended by raydom after the callback.
void rd_paint_scissor(const rd_paint_info* info);

// raylib integration. rd_update reads raylib's mouse state and lays the tree out at the screen
// size; rd_draw draws it between BeginDrawing and EndDrawing. Destroy the context before
// CloseWindow so its textures and shader are released.
void rd_update(rd_ctx* ctx);
// Lets raydom manage raylib's event waiting: idle frames wait for input (EnableEventWaiting), except
// while a timer is pending, when frames keep running so it fires on time. Use instead of calling
// EnableEventWaiting yourself.
void rd_set_event_waiting(rd_ctx* ctx, bool enabled);
void rd_draw(rd_ctx* ctx);

#ifdef __cplusplus
}
#endif

#endif
