#ifndef RAYDOM_INPUT_H
#define RAYDOM_INPUT_H

#include <raydom/text.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { RD_INPUT_MULTILINE = 1u << 0 };

// Makes the node an editable text leaf. Single-line inputs scroll horizontally; multi-line inputs
// wrap and grow. Inputs are always focusable. rd_node_text reads the value; rd_node_set_text
// replaces it and clamps the selection. A replaced value is its own undo step: it ends the typing
// run, and undo after it returns to the previous value. rd_input_select and blur end the run too.
bool rd_node_make_input(rd_ctx* ctx, rd_node node, uint32_t flags);
bool rd_node_is_input(const rd_ctx* ctx, rd_node node);
bool rd_node_set_placeholder(rd_ctx* ctx, rd_node node, const char* utf8, size_t len);

// Byte offsets into the value. The caret is the moving end.
bool rd_input_select(rd_ctx* ctx, rd_node node, size_t anchor, size_t caret);
bool rd_input_selection(const rd_ctx* ctx, rd_node node, size_t* anchor, size_t* caret);

// Multi-line inputs. A line is a line of the value, counted from 0 between newlines, however many rows
// it wraps to; a value ending in a newline has an empty last line. False for another kind of node,
// or a line past the last.
// The box the line's text fills, across its rows, relative to the node's border box, so it doesn't
// change with scrolling. The width is the text's: 0 for an empty line. As of the last layout, with the
// current value; false before the node's first layout.
bool rd_input_line_rect(rd_ctx* ctx, rd_node node, uint32_t line, rd_rect* out);
// rd_node_scroll_into_view for that box: an input grows with its value, so its enclosing containers
// scroll, and it may be called right after setting the value.
bool rd_input_scroll_to_line(rd_ctx* ctx, rd_node node, uint32_t line);

#ifdef __cplusplus
}
#endif

#endif
