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

#ifdef __cplusplus
}
#endif

#endif
