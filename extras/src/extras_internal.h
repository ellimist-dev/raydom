#ifndef RAYDOM_EXTRAS_INTERNAL_H
#define RAYDOM_EXTRAS_INTERNAL_H

#include <raydom/extras/controls.h>
#include <raydom/extras/toast.h>

typedef struct rdx__theme {
  rdx_classes classes;
  rdx_palette palette;
  bool        images_loaded;
  rd_image    check;    // the checkmark, an SVG drawn in currentColor
  rd_image    chevron;  // a select's down arrow, an expanded tree item
  rd_image    chevron_right;  // a collapsed tree item
  rd_image    chevron_up;     // a numeric input's step up; chevron steps down
} rdx__theme;

rdx__theme* rdx__theme_get(rd_ctx* ctx);

// Marks a part of a control: shares the control's states and doesn't start text selection.
void rdx__part(rd_ctx* ctx, rd_node part, rd_class cls);

#endif
