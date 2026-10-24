#ifndef RAYDOM_EVENTS_INTERNAL_H
#define RAYDOM_EVENTS_INTERNAL_H

#include "internal.h"

// Drops focus, firing change, blur and focusout, when the focused node was destroyed, detached,
// hidden or disabled. Needs computed styles: run it after layout.
void rd__focus_fixup(rd_ctx* ctx);
// Numbers attached nodes in pre-order into layouts[].order; detached ones get UINT32_MAX (selection.c).
void rd__tree_number(rd_ctx* ctx);

#endif
