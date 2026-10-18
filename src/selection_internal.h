#ifndef RAYDOM_SELECTION_INTERNAL_H
#define RAYDOM_SELECTION_INTERNAL_H

#include "internal.h"

// Orders the tree and resolves the selection's endpoints; false when nothing is selected.
bool rd__selection_prepare(rd_ctx* ctx);
// The selected byte range within node i, after rd__selection_prepare.
bool rd__selection_range(const rd_ctx* ctx, uint32_t i, size_t* lo, size_t* hi);

// Pointer default actions: start at a press, extend while dragging. hit is the node under the point.
void rd__selection_begin(rd_ctx* ctx, uint32_t hit, float x, float y);
void rd__selection_drag(rd_ctx* ctx, uint32_t hit, float x, float y);
// Copy, select all, Escape. True when the key was used.
bool rd__selection_key(rd_ctx* ctx, const rd_event* ev);

#endif
