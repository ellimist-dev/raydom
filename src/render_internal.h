#ifndef RAYDOM_RENDER_INTERNAL_H
#define RAYDOM_RENDER_INTERNAL_H

#include "internal.h"

enum { RD_CMD_RECT,
       RD_CMD_SHADOW,
       RD_CMD_TEXT,
       RD_CMD_CLIP,
       RD_CMD_FILL,
       RD_CMD_PLACEHOLDER,
       RD_CMD_PAINT };

// Root coordinates, logical pixels. Colors carry inherited opacity in their alpha.
typedef struct rd__cmd {
  uint8_t  kind;
  uint32_t node;
  // RECT: border box; SHADOW: shadow box; TEXT, PLACEHOLDER: content box; CLIP: scissor;
  // FILL: a plain rectangle, used for selections and carets.
  rd_rect  rect;
  float    radii[4];   // top-left, top-right, bottom-right, bottom-left
  float    border[4];  // top, right, bottom, left
  rd_color fill, stroke;
  float    blur;
  float    dx;    // TEXT, PLACEHOLDER: horizontal scroll offset
  rd_rect  clip;  // PAINT: the clip in effect
} rd__cmd;

// Rebuilds the display list from the last layout.
const rd__cmd* rd__display_list(rd_ctx* ctx, uint32_t* count);
void           rd__render_ctx_free(rd_ctx* ctx);

#endif
