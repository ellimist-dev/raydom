#ifndef RAYDOM_TEXT_H
#define RAYDOM_TEXT_H

#include <raydom/layout.h>
#include <raydom/style.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Pixel metrics at a font size. descent is positive, below the baseline.
typedef struct rd_font_metrics {
  float ascent, descent, line_gap;
} rd_font_metrics;

// TrueType or OpenType data. Without copy, data must outlive the context.
// The first font loaded becomes the default.
rd_font rd_font_load(rd_ctx* ctx, const void* data, size_t size, bool copy);
rd_font rd_font_load_file(rd_ctx* ctx, const char* path);
void    rd_ctx_set_default_font(rd_ctx* ctx, rd_font font);

rd_font_metrics rd_font_get_metrics(const rd_ctx* ctx, rd_font font, float size);
// Advance width of a single unwrapped line.
float rd_font_measure(const rd_ctx* ctx, rd_font font, float size, const char* utf8, size_t len);

#define RD_TEXT_NUL SIZE_MAX

// Makes the node a text leaf, styled by its inherited font, font_size, line_height and white_space.
// RD_TEXT_NUL reads up to a NUL terminator.
bool rd_node_set_text(rd_ctx* ctx, rd_node node, const char* utf8, size_t len);
// Null for a node that isn't a text leaf.
const char* rd_node_text(const rd_ctx* ctx, rd_node node, size_t* len);

// A byte range drawn with a background, and optionally its own text color (alpha 0 keeps the node's).
typedef struct rd_highlight {
  size_t   start, end;
  rd_color background, color;
} rd_highlight;

// Replaces the text node's highlights; null or 0 clears them. Ranges are copied, kept across
// rd_node_set_text, and clamped to the text when drawn.
bool rd_node_set_highlights(rd_ctx* ctx, rd_node node, const rd_highlight* ranges, size_t count);

// Document text selection: from (anchor, offset) to (focus, offset) in tree order, across text
// nodes. Dragging over text with user_select: auto creates it. Offsets are bytes.
bool rd_ctx_select(rd_ctx* ctx, rd_node anchor, size_t anchor_offset, rd_node focus, size_t focus_offset);
void rd_ctx_clear_selection(rd_ctx* ctx);
// False when nothing is selected.
bool rd_ctx_selection(const rd_ctx* ctx, rd_node* anchor, size_t* anchor_offset, rd_node* focus,
                      size_t* focus_offset);
// The selected text, one line per text node. Returns its full length; writes at most cap - 1 bytes
// and a NUL when buf is non-null.
size_t rd_ctx_selection_text(rd_ctx* ctx, char* buf, size_t cap);

// Distance from the border-box top to the first baseline after layout. NaN if the node has none.
float rd_node_baseline(const rd_ctx* ctx, rd_node node);

#ifdef __cplusplus
}
#endif

#endif
