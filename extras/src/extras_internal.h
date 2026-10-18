#ifndef RAYDOM_EXTRAS_INTERNAL_H
#define RAYDOM_EXTRAS_INTERNAL_H

#include <raydom/extras/controls.h>
#include <raydom/extras/toast.h>

typedef struct rdx__theme {
  rdx_classes classes;
  rd_image    check;    // the checkmark, an SVG drawn in currentColor
  rd_image    chevron;  // a select's down arrow, an expanded tree item
  rd_image    chevron_right;  // a collapsed tree item
} rdx__theme;

rdx__theme* rdx__theme_get(rd_ctx* ctx);

// Marks a part of a control: shares the control's states and doesn't start text selection.
void rdx__part(rd_ctx* ctx, rd_node part, rd_class cls);



// Objects found by their node, one registry per key. An object must be freed with its node (as a
// listener's data), so a node that's gone means its object is gone; lookups check that first.
bool  rdx__track(rd_ctx* ctx, const void* key, rd_node node, void* obj);
void* rdx__lookup(const rd_ctx* ctx, const void* key, rd_node node);
// Every live object under key, for hooks that visit them all. Returns the count; *out is owned by the registry.
int   rdx__all(rd_ctx* ctx, const void* key, void*** out);

#endif
