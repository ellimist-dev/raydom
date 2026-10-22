#ifndef RAYDOM_EXTRAS_TOAST_H
#define RAYDOM_EXTRAS_TOAST_H

#include <raydom/extras/theme.h>

#ifdef __cplusplus
extern "C" {
#endif

// Toasts: short notices that stack in a corner of the window, in the overlay. Each closes after its
// duration, or from its close button; hovering pauses the countdown. A stack keeps the newest five.

RD_ENUM typedef enum rdx_toast_position : uint8_t {
  RDX_TOAST_BOTTOM_RIGHT,
  RDX_TOAST_BOTTOM_LEFT,
  RDX_TOAST_TOP_RIGHT,
  RDX_TOAST_TOP_LEFT,
  RDX_TOAST_TOP_CENTER,
  RDX_TOAST_BOTTOM_CENTER,
} rdx_toast_position;

RD_ENUM typedef enum rdx_toast_kind : uint8_t {
  RDX_TOAST_INFO,
  RDX_TOAST_SUCCESS,
  RDX_TOAST_WARNING,
  RDX_TOAST_ERROR,
} rdx_toast_kind;

// Zero is an info toast at the bottom right for four seconds.
typedef struct rdx_toast_desc {
  const char*        title RD_DEFAULT({});  // optional
  const char*        text RD_DEFAULT({});
  rdx_toast_position position RD_DEFAULT({});
  rdx_toast_kind     kind RD_DEFAULT({});
  double             seconds RD_DEFAULT({});  // 0 means 4; negative stays until closed
} rdx_toast_desc;

// Returns the toast's node, for rdx_toast_close or to add content. Null on failure.
rd_node rdx_toast_show(rd_ctx* ctx, const rdx_toast_desc* desc);
void    rdx_toast_close(rd_ctx* ctx, rd_node toast);

#ifdef __cplusplus
}
#endif

#endif
